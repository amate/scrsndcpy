// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlddx.h>

#include "resource.h"

#include "aboutdlg.h"

#include "ProcessIO.h"
#include "WavePlay.h"
#include "Config.h"

#include "Utility\json.hpp"

#define PUTLOG	CMainDlg::PutLog

class CMainDlg : 
	public CDialogImpl<CMainDlg>, 
	public CUpdateUI<CMainDlg>,
	public CMessageFilter, 
	public CIdleHandler,
	public CWinDataExchange<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	enum {
		kMaxVolume = 100,

		WM_WAVEPlAY_INFO = WM_APP + 1,
	};

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		UIUpdateChildWindows();
		return FALSE;
	}

	static void PutLog(LPCWSTR pstrFormat, ...);


	BEGIN_UPDATE_UI_MAP(CMainDlg)
	END_UPDATE_UI_MAP()

	BEGIN_DDX_MAP(CMainDlg)
		DDX_CONTROL_HANDLE(IDC_EDIT_LOG, m_editLog)
		DDX_CONTROL_HANDLE(IDC_EDIT_WAVEPLAYINFO, m_wavePlayInfo)
		DDX_CONTROL_HANDLE(IDC_COMBO_DEVICES, m_cmbDevices)
		DDX_CONTROL_HANDLE(IDC_CHECK_SCREENSOUNDCOPY, m_checkSSC)
		DDX_CONTROL_HANDLE(IDC_SLIDER_VOLUME , m_sliderVolume)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDC_CHECK_SCREENSOUNDCOPY, OnScreenSoundCopy)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		MESSAGE_HANDLER_EX(WM_WAVEPlAY_INFO, OnWavePlayInfo)

		COMMAND_ID_HANDLER_EX(IDC_BUTTON_TOGGLE_MUTE, OnToggleMute)
		COMMAND_ID_HANDLER_EX(IDC_BUTTON_MANUAL_SNDCPY, OnManualSndcpy)
		COMMAND_ID_HANDLER_EX(IDC_BUTTON_CONFIG, OnConfig)
		MSG_WM_VSCROLL(OnTrackberVScroll)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnScreenSoundCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWavePlayInfo(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void	OnToggleMute(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnManualSndcpy(UINT uNotifyCode, int nID, CWindow wndCtl);
	void	OnConfig(UINT uNotifyCode, int nID, CWindow wndCtl);

	void	OnTrackberVScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar);

private:
	void	_AdbTrackDeviceInit();
	bool	_ScrcpyStart();
	void	_SndcpyAutoPermission(bool bManual = false);
	std::string	_SendADBCommand(const std::wstring& command, std::string deviceSerial = "");
	void	_DoSoundStreaming();
	void	_StopStreaming();

	std::wstring	_BuildScrcpyCommandLine();
	HWND			_FindScrcpyWindow();
	std::string		_SendCommonAdbCommand(const std::string& commandName, std::string deviceSerial = "");

	void CloseDialog(int nVal);

	static CEdit	m_editLog;

	CButton	m_checkSSC;
	CComboBox		m_cmbDevices;
	CTrackBarCtrl	m_sliderVolume;

	std::vector<std::string>	m_deviceList;
	bool			m_bFirstDeviceCheck = true;
	std::wstring	m_currentDeviceSerial;

	ProcessIO	m_adbTrackDevicesProcess;

	ProcessIO	m_scrcpyProcess;
	CHandle		m_sharedMemFilemap;
	std::thread	m_threadSoundStreeming;
	std::atomic_bool	m_cancelSoundStreaming = false;
	std::unique_ptr<WavePlay>	m_wavePlay;

	Config	m_config;
	nlohmann::json	m_jsonCommon;
	CPoint	m_scrcpyWidowPos;

	CEdit	m_wavePlayInfo;
};