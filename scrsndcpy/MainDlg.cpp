
#include "stdafx.h"
#include "MainDlg.h"
#include <thread>
#include <chrono>
#include <unordered_set>
#include <boost\algorithm\string\trim.hpp>

#include "WavePlay.h"

#include "Utility\CommonUtility.h"
#include "Utility\Logger.h"
#include "Utility\CodeConvert.h"
#include "Utility\json.hpp"
#include "Socket.h"

#include "ConfigDlg.h"

using namespace CodeConvert;
using json = nlohmann::json;



bool	SaveWindowScreenShot(HWND hWndTarget, const std::wstring& filePath)
{
	CWindowDC dc(hWndTarget);

	CRect rcWindow;
	::GetWindowRect(hWndTarget, &rcWindow);

	CDC dcMemory;
	dcMemory.CreateCompatibleDC(dc);
	CBitmap hbmp = ::CreateCompatibleBitmap(dc, rcWindow.Width(), rcWindow.Height());
	auto prevhbmp = dcMemory.SelectBitmap(hbmp);

	dcMemory.BitBlt(0, 0, rcWindow.Width(), rcWindow.Height(), dc, 0, 0, SRCCOPY);
	dcMemory.SelectBitmap(prevhbmp);

	Gdiplus::Bitmap bmp(hbmp, NULL);
	auto pngEncoder = GetEncoderByMimeType(L"image/png");
	auto ret = bmp.Save(filePath.c_str(), &pngEncoder->Clsid);
	bool success = ret == Gdiplus::Ok;
	return success;
}

fs::path	GetAdbPath()
{
	auto adbPath = GetExeDirectory() / L"scrcpy" / L"adb.exe";
	return adbPath;
}

std::vector<std::string>	GetDevices()
{
	std::wstring commandLine = L" devices";
	std::string strout;
	DWORD ret = StartProcessGetStdOut(GetAdbPath(), commandLine, strout);
	if (ret != 0) {
		throw std::runtime_error("devices failed");
	}

	std::vector<std::string> devices;

	std::vector<std::string> lines;
	boost::algorithm::split(lines, strout, boost::is_any_of("\n"));
	for (const std::string& line : lines) {
		auto spacePos = line.find("\t");
		if (spacePos == std::string::npos) {
			continue;
		}
		std::string device = line.substr(0, spacePos);
		devices.push_back(device);
	}
	return devices;
}

// ======================================================

CEdit	CMainDlg::m_editLog;

void CMainDlg::PutLog(LPCWSTR pstrFormat, ...)
{
	if (!m_editLog.IsWindow())
		return;

	CString strText;
	{
		va_list args;
		va_start(args, pstrFormat);

		strText.FormatV(pstrFormat, args);

		va_end(args);

		INFO_LOG << (LPCWSTR)strText;
	}
	//strText += _T("\n");
	strText.Insert(0, _T('\n'));
	strText.Replace(_T("\n"), _T("\r\n"));

	m_editLog.AppendText(strText);
	m_editLog.LineScroll(0, INT_MIN);
}

LRESULT CMainDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UIAddChildWindowContainer(m_hWnd);

	DoDataExchange(DDX_LOAD);

	// �R���\�[���E�B���h�E���\���ɂ���
	INFO_LOG << L"�N��";
	HWND hwndConsole = ::GetConsoleWindow();
	ATLASSERT(hwndConsole);
#ifndef _DEBUG
	::ShowWindow(hwndConsole, SW_HIDE);
#endif

	m_editLog.SetLimitText(0);

	m_sliderVolume.ModifyStyle(0, TBS_REVERSED);
	m_sliderVolume.SetRange(0, 100);
	m_sliderVolume.SetPos(kMaxVolume - 50);

	try {
		std::ifstream fs((GetExeDirectory() / "setting.json").string());
		if (fs) {
			json jsonSetting;
			fs >> jsonSetting;

			{
				auto& windowRect = jsonSetting["MainDlg"]["WindowRect"];
				if (windowRect.is_null() == false) {
					CRect rc(windowRect[0], windowRect[1], windowRect[2], windowRect[3]);
					SetWindowPos(NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);
					//MoveWindow(&rc);
				}
			}
			std::string deviceName = jsonSetting["MainDlg"].value("LastConnectedDeviceName", "");
			if (deviceName.length()) {
				m_deviceList.push_back(deviceName);

				deviceName.insert(0, "*");
				m_cmbDevices.AddString(UTF16fromUTF8(deviceName).c_str());
				m_cmbDevices.SetCurSel(0);
			}

			int soundVolume = jsonSetting["MainDlg"].value("SoundVolume", 50);
			m_sliderVolume.SetPos(kMaxVolume - soundVolume);

			if (jsonSetting["ScrcpyWindow"].is_object()) {
				m_scrcpyWidowPos.x = jsonSetting["ScrcpyWindow"].value("x", -1);
				m_scrcpyWidowPos.y = jsonSetting["ScrcpyWindow"].value("y", -1);
			} else {
				m_scrcpyWidowPos = CPoint(-1, -1);
			}
		}
		{
			std::ifstream fs((GetExeDirectory() / "Common.json").string());
			ATLASSERT(fs);
			if (!fs) {
				PUTLOG(L"Common.json�����݂��܂���");
			} else {
				fs >> m_jsonCommon;
			}
		}
	} catch (std::exception& e)
	{
		PUTLOG(CA2W(e.what()));
	}
	m_config.LoadConfig();

	_AdbTrackDeviceInit();

	return TRUE;
}

LRESULT CMainDlg::OnCancel(WORD, WORD wID, HWND, BOOL&)
{
	if (IsIconic() == FALSE) {
		//DoDataExchange(DDX_SAVE);

		json jsonSetting;
		std::ifstream fs((GetExeDirectory() / "setting.json").string());
		if (fs) {
			fs >> jsonSetting;
			fs.close();
		}
		{
			CRect rcWindow;
			GetWindowRect(&rcWindow);
			jsonSetting["MainDlg"]["WindowRect"] =
				nlohmann::json::array({ rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom });
		}

		std::string lastConnectedDeviceName;
		const int index = m_cmbDevices.GetCurSel();
		if (index != -1) {
			lastConnectedDeviceName = m_deviceList[index];
		}
		jsonSetting["MainDlg"]["LastConnectedDeviceName"] = lastConnectedDeviceName;
		int soundVolume = kMaxVolume - m_sliderVolume.GetPos();
		jsonSetting["MainDlg"]["SoundVolume"] = soundVolume;

		jsonSetting["ScrcpyWindow"]["x"] = m_scrcpyWidowPos.x;
		jsonSetting["ScrcpyWindow"]["y"] = m_scrcpyWidowPos.y;

		std::ofstream ofs((GetExeDirectory() / "setting.json").string());
		ofs << jsonSetting.dump(4);

	}
	CloseDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnWavePlayInfo(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int playSample = LOWORD(wParam);
	int minSample = LOWORD(lParam);
	int maxSample = HIWORD(lParam);
	int dropSample = HIWORD(wParam);

	CString title;
	title.Format(L"PlaySample: %d min: %d max %d dropSample: %d", playSample, minSample, maxSample, dropSample);
	m_wavePlayInfo.SetWindowText(title);

	return LRESULT();
}


LRESULT CMainDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	_StopStreaming();

	m_adbTrackDevicesProcess.Terminate();

	return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD, WORD, HWND, BOOL&)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnScreenSoundCopy(WORD, WORD wID, HWND, BOOL&)
{
	if (m_checkSSC.GetCheck() == BST_UNCHECKED) {
		PUTLOG(L"�X�g���[�~���O���~���܂�");
		_StopStreaming();
		m_checkSSC.SetWindowTextW(L"Screen Sound Copy");
		m_currentDeviceSerial.clear();

	} else {

		const int index = m_cmbDevices.GetCurSel();
		if (index == -1) {
			MessageBox(L"�ڑ��f�o�C�X��I�����Ă�������");
			return 0;
		}
		if (m_cmbDevices.GetItemData(index) == 0) {
			MessageBox(L"�f�o�C�X�Ƃ̐ڑ����؂�Ă��܂�");
			return 0;
		}
		std::wstring deviceSerial = UTF16fromUTF8(m_deviceList[index]);
		m_currentDeviceSerial = deviceSerial;

		if (m_config.deviceMuteOnStart) {
			_SendCommonAdbCommand("Mute");
		}

		// scrcpy
		bool success = _ScrcpyStart();
		if (!success) {
			PUTLOG(L"scrcpy�̎��s�Ɏ��s");
			return 0;
		}

		// �������O�C��
		if (m_config.loginPassword.size()) {
			std::string stdoutText = _SendCommonAdbCommand("IsScreenLock");
			if (stdoutText.find("00000001") != std::string::npos) {	// ��ʂ����b�N����Ă���
				PUTLOG(L"�������O�C�����܂�");
				// ���b�N������ʂ�
				_SendADBCommand(L"shell input keyevent MENU");
				_SendADBCommand(L"shell input keyevent MENU");
				// �p�X���[�h����
				_SendADBCommand(L"shell input text " + UTF16fromUTF8(m_config.loginPassword));
				PUTLOG(L"�������O�C������");
			}
		}
	
		// sndcpy
		_SndcpyAutoPermission();
	}
	return 0;
}


void CMainDlg::OnToggleMute(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	// Android11�ŗL��
	// i32 3  3 �̓��f�B�A����
	// i32 101 101 �� ADJUST_TOGGLE_MUTE
	// https://developer.android.com/reference/android/media/AudioManager#ADJUST_TOGGLE_MUTE
	// i32 0 �̓r�b�g�t���O 1��UI�\�� 4�ŉ��ʕύX���ɉ�����
	_SendCommonAdbCommand("ToggleMute");
	PUTLOG(L"toggle mute");
}

void CMainDlg::OnManualSndcpy(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (m_currentDeviceSerial.empty()) {
		MessageBox(L"��ʂ��\������Ă��܂���\r\n[Screen Sound Copy]�����s���Ă�������");
		return;
	}
	if (m_threadSoundStreeming.joinable()) {
		PUTLOG(L"SoundStreamingThread���I�������܂�");
		m_cancelSoundStreaming = true;
		m_threadSoundStreeming.join();
	}
	PUTLOG(L"Manual SndCpy");
	_SndcpyAutoPermission(true);
}

void CMainDlg::OnConfig(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	ConfigDlg	dlg(m_config);
	dlg.DoModal(m_hWnd);

#if 0
	auto scrcpyPath = GetExeDirectory() / L"scrcpy" / L"scrcpy.exe";
	std::wstring commandLine = L" --turn-screen-off";
	commandLine += L" --serial " + m_currentDeviceSerial;
	auto process = std::make_shared<ProcessIO>();
	process->RegisterStdOutCallback([process](const std::string& text) {
		// process teminate
		if (text.find("screen turned off") != std::string::npos) {
			PUTLOG(L"screen off");
			process->Terminate();
		}
	});
	process->StartProcess(scrcpyPath.wstring(), commandLine, false);
#endif
	return;
}

void CMainDlg::OnTrackberVScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
{
	nPos = kMaxVolume - m_sliderVolume.GetPos();// sliderVolume.SetPos(nPos);
	if (m_wavePlay) {
		m_wavePlay->SetVolume(nPos);
	}
}

void CMainDlg::_AdbTrackDeviceInit()
{
	m_adbTrackDevicesProcess.RegisterStdOutCallback([this](const std::string& text) {
		int curIndex = m_cmbDevices.GetCurSel();
		// �Ō�ɐڑ������f�o�C�X�ȊO���R���{�{�b�N�X����폜
		while (m_cmbDevices.DeleteString(1) != CB_ERR) {
			m_deviceList.erase(m_deviceList.begin() + 1);
		}

		std::string lastConnectedDeviceName;
		if (m_deviceList.size()) {
			lastConnectedDeviceName = m_deviceList.front();
		}
		size_t prevDeviceListCount = m_deviceList.size();

		std::unordered_set<std::string> connectedDeviceList;

		// �S�ڑ��f�o�C�X�擾
		auto deviceList = GetDevices();
		bool lastConnctedDeviceFound = false;
		for (const std::string& device : deviceList) {
			// wifi�Ȃ�V���A���m�F
			std::string deviceSerialNumber;
			if (device.substr(0, 8) == "192.168.") {
				deviceSerialNumber = _SendCommonAdbCommand("DeviceSerialNumber", device);
				boost::algorithm::trim(deviceSerialNumber);
				connectedDeviceList.insert(deviceSerialNumber);
			}
			std::wstring u16deviceName = UTF16fromUTF8(device);
			if (deviceSerialNumber.length()) {
				u16deviceName += L" (" + UTF16fromUTF8(deviceSerialNumber) + L")";
			}

			if (device == lastConnectedDeviceName) {
				lastConnctedDeviceFound = true;
				lastConnectedDeviceName = UTF8fromUTF16(u16deviceName);
			} else {
				int n = m_cmbDevices.AddString(u16deviceName.c_str());
				m_cmbDevices.SetItemData(n, true);
				m_deviceList.push_back(device);
			}
		}

		if (lastConnectedDeviceName.length()) {
			if (!lastConnctedDeviceFound) {	// �O��ڑ������f�o�C�X��������Ȃ������ꍇ
				lastConnectedDeviceName.insert(0, "*[disconnect]");
			} else {
				lastConnectedDeviceName.insert(0, "*");
			}
			m_cmbDevices.DeleteString(0);
			m_cmbDevices.InsertString(0, UTF16fromUTF8(lastConnectedDeviceName).c_str());
			m_cmbDevices.SetItemData(0, lastConnctedDeviceFound);
		}

		if (m_cmbDevices.GetCount() <= curIndex) {
			--curIndex;
		}
		m_cmbDevices.SetCurSel(curIndex);

		// �������s
		if (m_bFirstDeviceCheck && m_config.autoStart) {
			if (lastConnctedDeviceFound) {
				PUTLOG(L"�������s���܂�");
				m_checkSSC.SetCheck(BST_CHECKED);
				BOOL bHandled = TRUE;
				OnScreenSoundCopy(0, 0, NULL, bHandled);
			} else {
				PUTLOG(L"�O��ڑ������f�o�C�X��������Ȃ����߁A�������s�̓L�����Z������܂�");
			}
		}

		// �f�o�C�X����wifi�o�R�ł�adb�҂��󂯂��s���悤�Ɏw������
		if (m_config.autoWifiConnect && m_bFirstDeviceCheck || (prevDeviceListCount != m_deviceList.size())) {
			m_bFirstDeviceCheck = false;
			const int acceptPort = 5555;

			// �f�o�C�X�֑҂��󂯗v������
			for (const std::string& device : deviceList) {
				if (connectedDeviceList.find(device) != connectedDeviceList.end()) {
					continue;
				}
				if (device.substr(0, 8) == "192.168.") {
					continue;
				}

				std::string wlanText = _SendCommonAdbCommand("DeviceIPAddress", device);
				auto ipBeginPos = wlanText.find("192.168.");
				if (ipBeginPos == std::string::npos) {
					PUTLOG(L"�f�o�C�X�����[�J��wifi�ɐڑ�����Ă��܂���: %s", UTF16fromUTF8(device).c_str());
					continue;
				}
				auto slashPos = wlanText.find("/", ipBeginPos);
				if (slashPos == std::string::npos) {
					ATLASSERT(FALSE);
					continue;
				}
				
				std::string deviceIPAddress = wlanText.substr(ipBeginPos, slashPos - ipBeginPos);
				std::string deviceIPPort = deviceIPAddress + ":" + std::to_string(acceptPort);
				// �҂���
				_SendADBCommand(L"tcpip " + std::to_wstring(acceptPort), device);
				// �ڑ�
				StartProcess(GetAdbPath(), L" connect " + UTF16fromUTF8(deviceIPPort));
				PUTLOG(L"adb connect %s", UTF16fromUTF8(deviceIPPort).c_str());
			}
		}
	});
	
	{	// �I�����ɁA�f�o�C�X��adb�Ƃ̐ڑ����؂�Ȃ��悤�ɂ���
		extern HANDLE g_hJob;
		// �n���h�����p������Ȃ��悤�ɂ���
		if (!SetHandleInformation(g_hJob, HANDLE_FLAG_INHERIT, 0)) {
			ATLASSERT(FALSE);
		}
		::ShellExecute(NULL, NULL, GetAdbPath().c_str(), L" start-server", NULL, SW_HIDE);

		// �n���h�����p�������悤�ɂ���
		if (!SetHandleInformation(g_hJob, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
			ATLASSERT(FALSE);
		}
	}

	bool success = m_adbTrackDevicesProcess.StartProcess(GetAdbPath().wstring(), L" track-devices", false);
	if (!success) {
		PUTLOG(L"adb track-devices�Ɏ��s");
		ATLASSERT(FALSE);
	}
}

bool CMainDlg::_ScrcpyStart()
{
	ATLASSERT(!m_scrcpyProcess.IsRunning());
	ATLASSERT(!m_sharedMemFilemap);

	auto scrcpyPath = GetExeDirectory() / L"scrcpy" / L"scrcpy.exe";

	m_scrcpyProcess.RegisterStdOutCallback([this](const std::string& text) {
		std::string log = text;
		if (log.back() == '\n') {
			log = log.substr(0, log.size() - 2);
		}
		PUTLOG(L"[scrcpy] %s", UTF16fromUTF8(log).c_str());

		if (log.find("Killing the server") != std::string::npos) {
			// scrcpy�̉�ʂ�����ꂽ
			if (m_checkSSC.GetCheck() == BST_CHECKED) {
				m_checkSSC.SetCheck(BST_UNCHECKED);
				BOOL bHandled = FALSE;
				OnScreenSoundCopy(0, 0, NULL, bHandled);
			}
		}
		});

	std::wstring scrcpyCommandLine = _BuildScrcpyCommandLine();
	scrcpyCommandLine += L" --serial " + m_currentDeviceSerial;
	PUTLOG(L"scrcpyCommandLine: %s", scrcpyCommandLine.c_str());
	bool success = m_scrcpyProcess.StartProcess(scrcpyPath.wstring(), scrcpyCommandLine);
	if (!success) {
		PUTLOG(L"scrcpy�v���Z�X�̎��s�Ɏ��s...");
		return false;
	}

	{
		const DWORD scrcpyProcessID = ::GetProcessId(m_scrcpyProcess.GetProcessInfomation().hProcess);
		std::wstring sharedMemName = L"delayFrame" + std::to_wstring(scrcpyProcessID);

		m_sharedMemFilemap.Attach(
			::CreateFileMapping(
				INVALID_HANDLE_VALUE,    // use paging file
				NULL,                    // default security
				PAGE_READWRITE,          // read/write access
				0,                       // maximum object size (high-order DWORD)
				sizeof(uint32_t),        // maximum object size (low-order DWORD)
				sharedMemName.c_str())	 // name of mapping object
		);
		if (!m_sharedMemFilemap) {
			ATLASSERT(FALSE);
			PUTLOG(L"���L�������̍쐬�Ɏ��s");
			return false;
		} else {
			uint32_t* pDelayFrameCount = (uint32_t*)::MapViewOfFile(m_sharedMemFilemap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(uint32_t));
			ATLASSERT(pDelayFrameCount);
			*pDelayFrameCount = m_config.delayFrameCount;	// �ݒ菑������
			::UnmapViewOfFile((LPCVOID)pDelayFrameCount);
		}

		// Dll inject���s
		BOOL ret = FALSE;
		auto delayDllPath = GetExeDirectory() / L"delayFrame.dll";
		if (!fs::exists(delayDllPath)) {
			ATLASSERT(FALSE);
			PUTLOG(L"delayFrame.dll�����݂��܂���...");
			return false;
		}
		auto dllPath = delayDllPath.string();
		HANDLE proc = m_scrcpyProcess.GetProcessInfomation().hProcess;
		LPSTR libPath = (LPSTR)dllPath.c_str();
		DWORD pathSize = static_cast<DWORD>(dllPath.length() + 1);

		LPSTR remoteLibPath = (LPSTR)::VirtualAllocEx(
			proc,
			NULL,
			pathSize,
			MEM_COMMIT,
			PAGE_READWRITE);
		ATLASSERT(remoteLibPath);
		if (!remoteLibPath) {
			PUTLOG(L"VirtualAllocEx failed");
			return false;
		}

		// �����[�g�v���Z�X�֓ǂݍ��܂�����DLL�̃p�X����������
		ret = ::WriteProcessMemory(
			proc,
			remoteLibPath,
			libPath,
			pathSize,
			NULL);
		ATLASSERT(ret);
		if (!ret) {
			PUTLOG(L"WriteProcessMemory failed");
			return false;
		}

		// ���v���Z�X����Loadlibrary�ւ̃A�h���X�����̂܂܈����Ƃ��ēn�� 
		HANDLE hRemoteThread = ::CreateRemoteThread(
			proc,
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)LoadLibraryA,
			remoteLibPath,
			0,
			NULL);
		ATLASSERT(hRemoteThread);
		if (!hRemoteThread) {
			PUTLOG(L"CreateRemoteThread failed");
			return false;
		}
		::CloseHandle(hRemoteThread);
	}

#if 0
	// scrcpy�̃R���\�[���E�B���h�E���\���ɂ���
	enum { kMaxRetryCount = 10 };
	for (int i = 0; i < kMaxRetryCount; ++i) {
		HWND hwndScrcpyConsole = ::FindWindow(L"ConsoleWindowClass", scrcpyPath.c_str());
		if (hwndScrcpyConsole) {
			::ShowWindow(hwndScrcpyConsole, SW_HIDE);
			break;
		}
		::Sleep(100);
	}
#endif
	return true;	// success!
}

void CMainDlg::_SndcpyAutoPermission(bool bManual /*= false*/)
{
	m_checkSSC.EnableWindow(FALSE);
	m_checkSSC.SetWindowTextW(L"Prepareing...");

	auto funcSSCButtonEnable = [this](bool success) {
		m_checkSSC.EnableWindow(TRUE);
		m_checkSSC.SetWindowTextW(success ? L"Streaming..." : L"Screen Sound Copy");
	};

	auto funcStartSndcpy = [=]() -> bool {
		PUTLOG(L"sndcpy�N��");
		auto sndcpyPath = GetExeDirectory() / L"sndcpy" / L"sndcpy_start.bat";

		WCHAR systemFolder[MAX_PATH] = L"";
		::GetSystemDirectory(systemFolder, MAX_PATH);
		auto cmdPath = fs::path(systemFolder) / L"cmd.exe";
		std::wstring commandLine = L"/S /C \"";
		commandLine += L"\"" + sndcpyPath.wstring() + L"\" ";
		commandLine += m_currentDeviceSerial;
		commandLine += L"\"";

		std::string stdoutText;
		StartProcessGetStdOut(cmdPath, commandLine, stdoutText);
		PUTLOG(L"[sndcpy.bat] %s", UTF16fromUTF8(stdoutText).c_str());

		HWND hwnd_scrcpy = _FindScrcpyWindow();
		if (!hwnd_scrcpy) {
			PUTLOG(L"scrcpy�̉�ʂ�������܂���");
			return false;
		}

		if (bManual) {
			int ret = MessageBox(L"�f�o�C�X����sndcpy��[�������J�n]�����������[OK]��I�����Ă��������B", L"scrsndcpy", MB_OKCANCEL);
			if (ret == IDOK) {
				_DoSoundStreaming();
				return true;
			}
			return false;
		} else {
			::Sleep(500);	// �_�C�A���O���o�Ă���܂ő҂�
			PUTLOG(L"sndcpy�_�C�A���O�m�F��...");

			const std::string searchText = m_jsonCommon["Common"]["Sndcpy"]["AuthorizationGrep"];
			enum { kMaxRetryCount = 10 };
			for (int i = 0; i < kMaxRetryCount; ++i) {
				std::string adbRet = _SendCommonAdbCommand("IsSndcpyAuthorization");
				if (adbRet.find(searchText) != std::string::npos) {
					PUTLOG(L"�F�؃_�C�A���O�����A�����F�؂��܂�");
					_SendADBCommand(L"shell input keyevent DPAD_RIGHT");
					_SendADBCommand(L"shell input keyevent DPAD_RIGHT");
					_SendADBCommand(L"shell input keyevent ENTER");
					PUTLOG(L"�F�؂��܂���");

					::Sleep(500);
					_DoSoundStreaming();
					return true;	// success!

				}
				PUTLOG(L"retry count: %d", i);
				::Sleep(300);
			}
		}
		PUTLOG(L"sndcpy�̃_�C�A���O���m�F�ł��܂���ł���...");
		return false;
	};

	std::thread([=]()
	{
		std::string androidVersion = _SendCommonAdbCommand("AndroidVersion");
		if (androidVersion.length()) {
			enum { kSndcpyMinRequreAndroidVersion = 10 };
			const int version = std::stoi(androidVersion);
			if (version < kSndcpyMinRequreAndroidVersion) {
				PUTLOG(L"Android�̃o�[�W�������Â�����sndcpy�͎��s����܂���\nAndroid %d", version);
				funcSSCButtonEnable(true);
				return;
			} else {
				PUTLOG(L"Android %d", version);
			}
		} else {
			PUTLOG(L"Android�̃o�[�W�������s���ł�");
		}

		enum { kMaxSndcpyRetryCount = 2 };
		for (int k = 0; k < kMaxSndcpyRetryCount; ++k) {
			if (funcStartSndcpy()) {
				funcSSCButtonEnable(true);
				return;	// success!
			}
		}
		funcSSCButtonEnable(false);	// failed...
	}).detach();
}


std::string CMainDlg::_SendADBCommand(const std::wstring& command, std::string deviceSerial)
{
	if (deviceSerial.empty()) {
		deviceSerial = UTF8fromUTF16(m_currentDeviceSerial);
	}
	std::wstring addserialCommand = L" -s " + UTF16fromUTF8(deviceSerial) + L" " + command;
	std::string stdoutText;
	StartProcessGetStdOut(GetAdbPath(), addserialCommand, stdoutText);
	return stdoutText;
}

void CMainDlg::_DoSoundStreaming()
{
	ATLASSERT(!m_threadSoundStreeming.joinable());
	m_cancelSoundStreaming = false;
	m_threadSoundStreeming = std::thread([this]()
		{
			auto funcSSCButtonEnable = [this]() {
				m_checkSSC.EnableWindow(TRUE);
				m_checkSSC.SetWindowTextW(L"Streaming...");
			};

			PUTLOG(L"sndcpy�ɐڑ����܂�");

			IPAddress addr;
			addr.Set("127.0.0.1", "28200");
			CSocket sock;
			std::atomic_bool valid = true;
			enum { kMaxConnectRetryCount = 10 };
			for (int i = 0; i < kMaxConnectRetryCount; ++i) {
				if (!sock.Connect(addr, valid)) {
					::Sleep(500);
				} else {
					PUTLOG(L"�ڑ�����");
					break;
				}
			}
			if (!sock.IsConnected()) {
				PUTLOG(L"�ڑ��Ɏ��s���܂���...");
				funcSSCButtonEnable();
				return;
			}

			m_wavePlay = std::make_unique<WavePlay>();
			m_wavePlay->SetMainDlgHWND(m_hWnd);
			m_wavePlay->Init(m_config.bufferMultiple, m_config.maxBufferSampleCount);
			const int bufferSize = m_wavePlay->GetBufferBytes();

			m_wavePlay->SetVolume(kMaxVolume - m_sliderVolume.GetPos());

			sock.SetBlocking(true);
			auto buffer = std::make_unique<char[]>(bufferSize);

			enum { kDiscardCount = 2 };
			auto timebegin = std::chrono::steady_clock::now();
			while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - timebegin).count() < kDiscardCount) {
				int recvSize = sock.Read(buffer.get(), bufferSize);
			}
			PUTLOG(L"�����̃X�g���[�~���O�Đ����J�n���܂�");
			funcSSCButtonEnable();

			std::string bufferMain;
			while (!m_cancelSoundStreaming) {
				//char buffer[1024];
				int recvSize = sock.Read(buffer.get(), bufferSize);
				if (recvSize == 0 && sock.IsConnected()) {
					continue;
				}
				if (recvSize == -1) {
					ERROR_LOG << L"Socket Error";
					PUTLOG(L"Socket Error");
					break;
				}
#if 0
				if (::GetAsyncKeyState(VK_MENU) < 0 && ::GetAsyncKeyState(VK_SHIFT)) {
					continue;	// drain
				}
#endif
				m_wavePlay->WriteBuffer((const BYTE*)buffer.get(), recvSize);
			}
			m_wavePlay.reset();
		}
	);
}

void CMainDlg::_StopStreaming()
{
	if (m_scrcpyProcess.IsRunning()) {
		HWND hwnd_scrcpy = _FindScrcpyWindow();
		if (hwnd_scrcpy) {
			if (::IsIconic(hwnd_scrcpy) == FALSE) {
				//CRect rcWindow;
				//::GetWindowRect(hwnd_scrcpy, &rcWindow);
				
				CRect rcClient;
				::GetClientRect(hwnd_scrcpy , &rcClient);
				CWindow(hwnd_scrcpy).MapWindowPoints(NULL, &rcClient);

				m_scrcpyWidowPos.x = rcClient.left;
				m_scrcpyWidowPos.y = rcClient.top;
			}

			if (m_config.deviceMuteOnStart) {
				_SendCommonAdbCommand("UnMute");
			}

			::PostMessage(hwnd_scrcpy, WM_CLOSE, 0, 0);
		} else {
			// scrcpy�{�̂̕���{�^���������ꂽ�ꍇ
		}
	}
	if (m_sharedMemFilemap) {
		m_sharedMemFilemap.Close();
	}

	if (m_threadSoundStreeming.joinable()) {
		m_cancelSoundStreaming = true;
		m_threadSoundStreeming.detach();
	}
}

std::wstring CMainDlg::_BuildScrcpyCommandLine()
{
	// 	std::wstring scrcpyCommandLine = LR"(  --turn-screen-off --window-title "My device" --shortcut-mod=lctrl,lalt,ralt,rctrl --window-x 1920 --window-y 30  --max-fps 30 --max-size 1280 --bit-rate 20M )
	std::wstring commandLine = UTF16fromUTF8(m_jsonCommon["Common"]["Scrcpy"]["CommandLine"].get<std::string>());
	if (m_config.maxSize > 0) {
		commandLine += L" --max-size " + std::to_wstring(m_config.maxSize);
	}
	if (m_config.maxFPS > 0) {
		commandLine += L" --max-fps " + std::to_wstring(m_config.maxFPS);
	}
	if (m_config.bitrate > 0) {
		commandLine += L" --bit-rate " + std::to_wstring(m_config.bitrate) + L"M";
	}
	if (m_config.turnScreenOff) {
		commandLine += L"  --turn-screen-off ";
	}
	if (m_scrcpyWidowPos.x != -1 && m_scrcpyWidowPos.y != -1) {
		commandLine += L" --window-x " + std::to_wstring(m_scrcpyWidowPos.x) 
					 + L" --window-y " + std::to_wstring(m_scrcpyWidowPos.y);
	}

	return commandLine;
}

HWND CMainDlg::_FindScrcpyWindow()
{
	std::wstring className = UTF16fromUTF8(m_jsonCommon["Common"]["Scrcpy"]["ClassName"].get<std::string>());
	std::wstring windowTitle = UTF16fromUTF8(m_jsonCommon["Common"]["Scrcpy"]["WindowTitle"].get<std::string>());

	HWND hwnd = ::FindWindow(className.c_str(), windowTitle.c_str());
	return hwnd;
}

std::string CMainDlg::_SendCommonAdbCommand(const std::string& commandName, std::string deviceSerial)
{
	std::wstring adbCommand = UTF16fromUTF8(m_jsonCommon["Common"]["AdbCommand"][commandName].get<std::string>());
	ATLASSERT(adbCommand.length());	
	return _SendADBCommand(adbCommand, deviceSerial);
}

void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}

