/**
*	@file	VolumeControl.h
*	@berif	他プロセスのボリュームを変更する
*/

#pragma once

#include <set>
#include <vector>
#include <Mmdeviceapi.h>
#include <Audiopolicy.h>


class CVolumeControl
{
public:
	bool	InitVolumeControl()
	{
		::CoInitialize(NULL);

		try {
		if (FAILED(m_spEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator))))
			throw 1;

		if (FAILED(m_spEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &m_spCollection)))
			throw 2;

		if (!GetCurrentProcessSimpleAudioVolume()) {
			return false;
		}

		} catch(...) {
			return false;
		}
		return true;
	}

	void	TermVolumeControl()
	{
		m_spEnumerator.Release();
		m_spEnumerator.Release();
		::CoUninitialize();
	}

	bool	GetCurrentProcessSimpleAudioVolume()
	{
		const DWORD currenetProcessId = ::GetCurrentProcessId();

		try {
			CComPtr<IMMDeviceCollection>	spEnumDevices;
			m_spEnumerator->EnumAudioEndpoints(eRender, eMultimedia, &spEnumDevices);
			if (spEnumDevices == nullptr)
				throw 1;
			UINT nCount = 0;
			spEnumDevices->GetCount(&nCount);
			for (UINT i = 0; i < nCount; ++i) {
				CComPtr<IMMDevice>	spDefaultDevice;
				spEnumDevices->Item(i, &spDefaultDevice);
				//if (FAILED(m_spEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &spDefaultDevice))) 
				//	throw 3;

				CComPtr<IAudioSessionManager2>	spAudioSessionManager2;
				if (FAILED(spDefaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&spAudioSessionManager2)))
					throw 4;

				CComPtr<IAudioSessionEnumerator>	spAudioSessionEnumerator;
				if (FAILED(spAudioSessionManager2->GetSessionEnumerator(&spAudioSessionEnumerator)))
					throw 5;

				int nSessionCount = 0;
				spAudioSessionEnumerator->GetCount(&nSessionCount);
				for (int i = 0; i < nSessionCount; ++i) {
					CComPtr<IAudioSessionControl>	spAudioSessionControl;
					spAudioSessionEnumerator->GetSession(i, &spAudioSessionControl);
					CComQIPtr<IAudioSessionControl2> spAudioSessionControl2 = spAudioSessionControl.p;
					if (spAudioSessionControl2 == nullptr)
						continue;

					DWORD dwAudioSessionProcessId = 0;
					if (FAILED(spAudioSessionControl2->GetProcessId(&dwAudioSessionProcessId)))
						continue;
					//ATLTRACE(_T("spAudioSessionControl2->GetProcessId : %d\n"), dwAudioSessionProcessId);
					//auto itfound = setBelongProcessId.find(dwAudioSessionProcessId);
					//if (itfound == setBelongProcessId.end())
					//	continue;

					if (dwAudioSessionProcessId != currenetProcessId) {
						continue;
					}

					m_spSimpleAudioVolume = spAudioSessionControl2;
					return true;
					//m_vecspSimpleAudioVolume.emplace_back(spAudioSessionControl2);
					//m_spSimpleAudioVolume = spAudioSessionControl2;
					//if (m_spSimpleAudioVolume)
					//	return true;
				}
			}
			//if (m_vecspSimpleAudioVolume.size() > 0)
			//	return true;

		} catch(...) {
			return false;
		}
		return false;
	}

	int		GetVolume()
	{
		return m_nowVolume;
	}

	void	SetVolume(int volume)
	{
		ATLASSERT(0 <= volume && volume <= 100);
		if (!m_spSimpleAudioVolume) {
			return;
		}
		m_nowVolume = volume;

		float fLevel = volume / 100.0f;
		if (fLevel < 0.0) {
			fLevel = 0;
		} else if (1.0 < fLevel) {
			fLevel = 1.0;
	}
		HRESULT hr = m_spSimpleAudioVolume->SetMasterVolume(fLevel, nullptr);
	}

	void	VolumeChange()
	{
#if 0
		GUID	guid = GUID_NULL;
		CoCreateGuid(&guid);
		HRESULT hr = m_spSimpleAudioVolume->SetMute(true, &guid);
		ATLTRACE(_T("VolumeChange hr:%#x\n"), hr);
		return ;
#endif
#if 0
		CVolumeControlDialog dlg(m_vecspSimpleAudioVolume);
		dlg.DoModal(NULL);
		m_vecspSimpleAudioVolume.clear();
		//m_spSimpleAudioVolume.Release();
#endif
	}

	bool	IsMute()
	{
		ATLASSERT(m_spSimpleAudioVolume);
		BOOL bMute = FALSE;
		m_spSimpleAudioVolume->GetMute(&bMute);
		return bMute != FALSE;
	}

	void	ToggleMute()
	{
		ATLASSERT(m_spSimpleAudioVolume);
		BOOL bMute = FALSE;
		m_spSimpleAudioVolume->GetMute(&bMute);
		HRESULT hr = m_spSimpleAudioVolume->SetMute(!bMute, &GUID_NULL);
	}

#if 0
	class CVolumeControlDialog : public CDialogImpl<CVolumeControlDialog>
	{
	public:
		enum { IDD = IDD_VOLUME };

		CVolumeControlDialog(std::vector<CComQIPtr<ISimpleAudioVolume>>& vecVolume) : m_vecspSimpleAudioVolume(vecVolume)
		{	}

		BEGIN_MSG_MAP_EX(CMainDlg)
			MSG_WM_INITDIALOG(OnInitDialog)
			MSG_WM_ACTIVATE( OnActivate )
			MSG_WM_VSCROLL( OnVolumeChange )
			COMMAND_ID_HANDLER_EX( IDC_CHECKBOX_MUTE, OnMute )
		END_MSG_MAP()

		BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
		{
			POINT pt;
			GetCursorPos(&pt);
			enum { kLeftMove = 27, kTopMove = 16 };
			SetWindowPos(NULL, pt.x - kLeftMove, pt.y - kTopMove, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

			BOOL bMute = FALSE;
			m_vecspSimpleAudioVolume[0]->GetMute(&bMute);
			if (bMute)
				CButton(GetDlgItem(IDC_CHECKBOX_MUTE)).SetCheck(TRUE);
			
			m_slideVolume = GetDlgItem(IDC_SLIDER_VOLUME);
			m_slideVolume.ModifyStyle(0, TBS_REVERSED);

			m_slideVolume.SetRange(0, 100, FALSE);
			m_slideVolume.SetTicFreq(50);
			m_slideVolume.SetPageSize(20);
			float level = 0.0;
			m_vecspSimpleAudioVolume[0]->GetMasterVolume(&level);
			int volume = static_cast<int>(level * 100);
			m_slideVolume.SetPos(100 - volume);
			return TRUE;
		}

		void OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther)
		{
			DefWindowProc();
			if (nState == WA_INACTIVE) {
				EndDialog(0);				
			}
		}

		void OnVolumeChange(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
		{
			float level = static_cast<float>(static_cast<float>(100 - m_slideVolume.GetPos()) / 100.0);
			for (auto& volume : m_vecspSimpleAudioVolume) {
				HRESULT hr = volume->SetMasterVolume(level, &GUID_NULL);
			}
		}

		void OnMute(UINT uNotifyCode, int nID, CWindow wndCtl)
		{
			bool bMute = CButton(GetDlgItem(IDC_CHECKBOX_MUTE)).GetCheck() != 0;
			for (auto& volume : m_vecspSimpleAudioVolume) {
				HRESULT hr = volume->SetMute(bMute, &GUID_NULL);
			}
		}

	private:
		// Data members
		//CComPtr<ISimpleAudioVolume>		m_spSimpleAudioVolume;
		std::vector<CComQIPtr<ISimpleAudioVolume>>&		m_vecspSimpleAudioVolume;
		CTrackBarCtrl	m_slideVolume;
	};
#endif


private:
	// Data members
	CComPtr<IMMDeviceEnumerator>	m_spEnumerator;
	CComPtr<IMMDeviceCollection>	m_spCollection;
	CComQIPtr<ISimpleAudioVolume>		m_spSimpleAudioVolume;

	int	m_nowVolume;
};













