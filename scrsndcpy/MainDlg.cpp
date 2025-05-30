﻿
#include "stdafx.h"
#include "MainDlg.h"
#include <thread>
#include <chrono>
#include <unordered_set>
#include <regex>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <boost\algorithm\string\trim.hpp>

#include "WavePlay.h"

#include "Utility\CommonUtility.h"
#include "Utility\Logger.h"
#include "Utility\CodeConvert.h"
#include "Utility\json.hpp"
#include "Socket.h"

#include "ConfigDlg.h"

#include "..\delayFrame\SharedMemoryData.h"

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

CMainDlg::CMainDlg() : m_wndScreenShotButton(this, 1)
{
}

void CMainDlg::PutLog(LPCWSTR pstrFormat, ...)
{
	if (!m_editLog.IsWindow())
		return;


	CString* pLog = new CString;
	CString& strText = *pLog;
	{
		va_list args;
		va_start(args, pstrFormat);

		strText.FormatV(pstrFormat, args);

		va_end(args);

		INFO_LOG << (LPCWSTR)strText;
	}
	//strText += _T("\n");
	CWindow mainDlg = m_editLog.GetParent();
	mainDlg.PostMessage(WM_PUTLOG, (WPARAM)pLog);

	//strText.Insert(0, _T('\n'));
	//strText.Replace(_T("\n"), _T("\r\n"));

	//m_editLog.AppendText(strText);
	//m_editLog.LineScroll(0, INT_MIN);
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

	// コンソールウィンドウを非表示にする
	INFO_LOG << L"起動";
	HWND hwndConsole = ::GetConsoleWindow();
	ATLASSERT(hwndConsole);
#ifndef _DEBUG
	::ShowWindow(hwndConsole, SW_HIDE);
#endif
	// スリープ復帰通知
	m_powerNotifyHandle = ::RegisterSuspendResumeNotification(m_hWnd, DEVICE_NOTIFY_WINDOW_HANDLE);
	ATLASSERT(m_powerNotifyHandle);

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
				m_scrcpyWidowPos = CPoint(100, 100);
			}
		}
		{
			std::ifstream fs((GetExeDirectory() / "Common.json").string());
			ATLASSERT(fs);
			if (!fs) {
				PUTLOG(L"Common.jsonが存在しません");
			} else {
				fs >> m_jsonCommon;
			}
		}
	} catch (std::exception& e)
	{
		PUTLOG(CA2W(e.what()));
	}
	m_config.LoadConfig();

	{
		auto ssFolderPath = GetExeDirectory() / L"screenshot";
		// 既定のフォルダ
		if (!fs::is_directory(ssFolderPath)) {
			fs::create_directory(ssFolderPath);
		}
	}

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

	::UnregisterSuspendResumeNotification(m_powerNotifyHandle);
	m_powerNotifyHandle = NULL;

	_StopStreaming();

	m_adbTrackDevicesProcess.Terminate();

	if (m_threadScreenshot.joinable()) {
		m_threadScreenshot.detach();
	}

	return 0;
}

BOOL CMainDlg::OnPowerBroadcast(DWORD dwPowerEvent, DWORD_PTR dwData)
{
	switch (dwPowerEvent) {
	case PBT_APMSUSPEND:	// スリープに入る
		PUTLOG(L"Power: susupend - running: %d", m_scrcpyProcess.IsRunning());
		if (m_scrcpyProcess.IsRunning()) {
			m_runningBeforeSleep = true;

			_StopStreaming();
		}
		break;

	case PBT_APMRESUMEAUTOMATIC:	// 復帰した
		PUTLOG(L"Power: resume - m_runningBeforeSleep: %d", m_runningBeforeSleep);
		if (m_runningBeforeSleep && m_config.reconnectOnResume) {
			SetTimer(kSleepResumeTimerId, kSleepResumeTimerInterval);
		}
		m_runningBeforeSleep = false;
		break;

	default:
		break;
	} 
	return TRUE;
}

void CMainDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == kSleepResumeTimerId) {
		KillTimer(kSleepResumeTimerId);
		PUTLOG(L"Sleep Resume - Screen Sound Copy");
		if (m_scrcpyProcess.IsRunning()) {
			PUTLOG(L"m_scrcpyProcess.IsRunning() == true");
			return;
		}

		ATLASSERT(m_checkSSC.GetCheck() == BST_UNCHECKED);
		ATLASSERT(!m_scrcpyProcess.IsRunning());
		m_checkSSC.SetCheck(BST_CHECKED);
		BOOL bHandled = TRUE;
		OnScreenSoundCopy(0, 0, NULL, bHandled);

	} else if (nIDEvent == kAutoRunTimerId) {
		KillTimer(kAutoRunTimerId);
		PUTLOG(L"AutoRunTimer");
		if (m_scrcpyProcess.IsRunning()) {
			PUTLOG(L"m_scrcpyProcess.IsRunning() == true");
			return;
		}

		ATLASSERT(m_checkSSC.GetCheck() == BST_UNCHECKED);
		ATLASSERT(!m_scrcpyProcess.IsRunning());
		m_checkSSC.SetCheck(BST_CHECKED);
		BOOL bHandled = TRUE;
		OnScreenSoundCopy(0, 0, NULL, bHandled);
	}
}

BOOL CMainDlg::OnCopyData(CWindow wnd, PCOPYDATASTRUCT pCopyDataStruct)
{
	if (pCopyDataStruct->dwData == kPutLog) {
		PUTLOG(L"%s", (LPCWSTR)pCopyDataStruct->lpData);
	}

	return 0;
}

LRESULT CMainDlg::OnDelayFrameCommand(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case kStartSndcpy:
	{
		PUTLOG(L"sndcpy起動");
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
		PUTLOG(L"[sndcpy_start.bat] %s", UTF16fromUTF8(stdoutText).c_str());

		//::Sleep(500);	// 初回接続までは少し待機する必要がある
		return TRUE;

	}
	break;

	case kRestartSndcpy:
	{
		std::string outret = _SendADBCommand(L"shell am start com.rom1v.sndcpy/.MainActivity --activity-clear-top"); // お試し --activity-clear-top
		return TRUE;
	}
	break;

	case kWakefulness:
	{
		std::string stdoutText = _SendCommonAdbCommand("DumpsysPower");
		std::regex rx(R"(mWakefulness=([^\r]+))");
		std::smatch result;
		if (std::regex_search(stdoutText, result, rx)) {
			std::string displayState = result[1].str();
			if (displayState != "Awake") {
				m_wavePlayInfo.SetWindowText(L"Pausing playback");
				return TRUE;
			}
		}
		return FALSE;
	}

	case kHankakuZenkaku:
	{
		// https://developer.android.com/reference/android/view/KeyEvent.html#constants
		// KEYCODE_LANGUAGE_SWITCH
		// Key code constant: Language Switch key. 
		
		//std::string outret = _SendADBCommand(L"shell input keyevent KEYCODE_ZENKAKU_HANKAKU");
		//PUTLOG(L"language switch");
	}
	break;

	case kCtrlEnter:
	{
		std::string outret = _SendADBCommand(L"shell input touchscreen tap 1 1");
		PUTLOG(L"Ctrl+Enter: touchscreen tap 1 1");
	}
	break;

	default:
	{
		ATLASSERT(FALSE);
	}
	break;
	}
	return FALSE;
}

LRESULT CMainDlg::OnPutLog(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::unique_ptr<CString> pLogText((CString*)wParam);
	ATLASSERT(pLogText);
	CString& strText = *pLogText;

	strText.Insert(0, _T('\n'));
	strText.Replace(_T("\n"), _T("\r\n"));

	m_editLog.AppendText(strText);
	m_editLog.LineScroll(0, INT_MIN);
	return LRESULT();
}

LRESULT CMainDlg::OnRunScrsndcpy(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PUTLOG(L"OnRunScrsndcpy");

	if (m_scrcpyProcess.IsRunning()) {
		PUTLOG(L"m_scrcpyProcess.IsRunning() == true : cancelします");
		return 0;
	}

	ATLASSERT(m_checkSSC.GetCheck() == BST_UNCHECKED);
	ATLASSERT(!m_scrcpyProcess.IsRunning());
	m_checkSSC.SetCheck(BST_CHECKED);
	BOOL bHandled = TRUE;
	OnScreenSoundCopy(0, 0, NULL, bHandled);

	return LRESULT();
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
		_StopStreaming();

	} else {

		const int index = m_cmbDevices.GetCurSel();
		if (index == -1) {
			MessageBox(L"接続デバイスを選択してください");
			return 0;
		}
		if (m_cmbDevices.GetItemData(index) == 0) {
			MessageBox(L"デバイスとの接続が切れています");
			return 0;
		}
		std::wstring deviceSerial = UTF16fromUTF8(m_deviceList[index]);
		m_currentDeviceSerial = deviceSerial;

		DoDataExchange(DDX_SAVE, IDC_CHECK_NATIVE_RESOLUTION);

		// scrcpy
		bool success = _ScrcpyStart();
		if (!success) {
			_StopStreaming();

			++m_scrcpyRetryCount;
			PUTLOG(L"scrcpyの実行に失敗 - retryCount: %d", m_scrcpyRetryCount);
			if (m_scrcpyRetryCount <= kScrcpyMaxRetryCount) {
				_SendADBCommand(L"kill-server", m_deviceList[index]);

				m_adbTrackDevicesProcess.Terminate();
				_AdbTrackDeviceInit();

				SetTimer(kAutoRunTimerId, kAutoRunTimerInterval);
			}			
			return 0;
		}
		m_scrcpyRetryCount = 0;

		// Android version check
		m_currentDeviceAndroidVersion = 0;
		std::string androidVersion = _SendCommonAdbCommand("AndroidVersion");
		if (androidVersion.length() && std::isdigit(androidVersion.front())) {
			enum { kSndcpyMinRequreAndroidVersion = 10 };
			const int version = std::stoi(androidVersion);
			m_currentDeviceAndroidVersion = version;
			PUTLOG(L"Android %d", version);

		} else {
			PUTLOG(L"Androidのバージョンが不明です");
		}

		// 自動ログイン
		if (m_config.loginPassword.size()) {
			std::string stdoutText = _SendCommonAdbCommand("IsScreenLock");
			if (stdoutText.find("00000001") != std::string::npos) {	// 画面がロックされている
				PUTLOG(L"自動ログインします");
				// ロック解除画面へ
				_SendADBCommand(L"shell input keyevent MENU");
				::Sleep(200);
				_SendADBCommand(L"shell input keyevent MENU");
				::Sleep(200);

				// パスワード入力
				_SendADBCommand(L"shell input text " + UTF16fromUTF8(m_config.loginPassword));
				PUTLOG(L"自動ログイン完了");
			}
		}
	
		// sndcpy
		if (m_config.useScrcpyAudio) {
			// 内臓を使うのでボタンを無効化しておく
			GetDlgItem(IDC_BUTTON_MANUAL_SNDCPY).EnableWindow(FALSE);
			enum { kAudioDupOkAndroidVersion = 13 };
			if (kAudioDupOkAndroidVersion <= m_currentDeviceAndroidVersion) {
				::Sleep(1000);
				m_sharedMemoryData->simpleAudioReady = true;
			}

		} else {
			GetDlgItem(IDC_BUTTON_MANUAL_SNDCPY).EnableWindow(TRUE);
			_SndcpyAutoPermission();
		}
		if (m_config.deviceMuteOnStart) {
			_SendCommonAdbCommand("Mute");
		}

		m_checkSSC.SetWindowTextW(L"Streaming...");

		// カスタムコマンドを実行する
		_DoCustomAdbCommand();
	}
	return 0;
}


void CMainDlg::OnToggleMute(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (!m_config.toggleMuteReverse) {

		// Android11で有効
		// i32 3  3 はメディア音量
		// i32 101 101 は ADJUST_TOGGLE_MUTE
		// https://developer.android.com/reference/android/media/AudioManager#ADJUST_TOGGLE_MUTE
		// i32 0 はビットフラグ 1でUI表示 4で音量変更時に音が鳴る
		_SendCommonAdbCommand("ToggleMute");

	} else {
		int soundVolume = kMaxVolume - m_sliderVolume.GetPos();
		if (soundVolume > 0) {
			m_prevVolume = m_sliderVolume.GetPos();
			m_sliderVolume.SetPos(kMaxVolume);

			//m_wavePlay->SetVolume(0);
			m_sharedMemoryData->playSoundVolume = 0;
			_SendCommonAdbCommand("UnMute");
		} else {
			int prevVolume = kMaxVolume - m_prevVolume;
			m_sliderVolume.SetPos(m_prevVolume);

			//m_wavePlay->SetVolume(prevVolume);
			m_sharedMemoryData->playSoundVolume = prevVolume;
			_SendCommonAdbCommand("Mute");
		}
	}
	PUTLOG(L"toggle mute");
}

void CMainDlg::OnManualSndcpy(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (m_currentDeviceSerial.empty()) {
		MessageBox(L"画面が表示されていません\r\n[Screen Sound Copy]を実行してください");
		return;
	}
	if (m_threadSoundStreeming.joinable()) {
		PUTLOG(L"SoundStreamingThreadを終了させます");
		m_cancelSoundStreaming = true;
		m_threadSoundStreeming.join();
	}
	PUTLOG(L"Manual SndCpy");
	_SndcpyAutoPermission(true);
}

void CMainDlg::OnScreenshot(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (!m_scrcpyProcess.IsRunning()) {
		return;
	}

	if (m_threadScreenshot.joinable()) {
		return;
	}

	CButton btn = GetDlgItem(IDC_BUTTON_SCREENSHOT);
	btn.EnableWindow(FALSE);
	btn.SetWindowText(L"capture...");

	m_threadScreenshot = std::thread([this]() {
		std::string ssPngBinary = _SendADBCommand(L"exec-out screencap -p");

		auto ssFolderPath = GetExeDirectory() / L"screenshot";
		auto ssPath = ssFolderPath / (L"screenshot_" + std::to_wstring(std::time(nullptr)) + L".png");

		std::ofstream fs(ssPath.wstring(), std::ios::out | std::ios::binary);
		fs.write(ssPngBinary.c_str(), ssPngBinary.length());

		PUTLOG(L"Screenshot: %s", ssPath.filename().wstring().c_str());

		CButton btn = GetDlgItem(IDC_BUTTON_SCREENSHOT);
		btn.SetWindowText(L"Screenshot");
		btn.EnableWindow(TRUE);

		m_threadScreenshot.detach();
	});
}

void CMainDlg::OnScreenShotButtonUp(UINT nFlags, CPoint point)
{
	auto ssFolderPath = GetExeDirectory() / L"screenshot";
	//if (fs::is_directory(m_config.screenShotFolder)) {
	//	ssFolderPath = m_config.screenShotFolder;
	//}
	::ShellExecute(NULL, NULL, ssFolderPath.c_str(), NULL, NULL, SW_NORMAL);
}

void CMainDlg::OnResolutionChange(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (m_scrcpyProcess.IsRunning()) {
		// 停止
		_StopStreaming();

		// 起動
		OnRunScrsndcpy(0, 0, 0);
	} else {

	}	
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
	if (m_sharedMemoryData) {
		int soundVolume = kMaxVolume - m_sliderVolume.GetPos();
		m_sharedMemoryData->playSoundVolume = soundVolume;
	}

	//nPos = kMaxVolume - m_sliderVolume.GetPos();// sliderVolume.SetPos(nPos);
	//if (m_wavePlay) {
	//	m_wavePlay->SetVolume(nPos);
	//}
}

void CMainDlg::_AdbTrackDeviceInit()
{
	m_adbTrackDevicesProcess.RegisterStdOutCallback([this](const std::string& text) {
		int curIndex = m_cmbDevices.GetCurSel();
		// 最後に接続したデバイス以外をコンボボックスから削除
		while (m_cmbDevices.DeleteString(1) != CB_ERR) {
			m_deviceList.erase(m_deviceList.begin() + 1);
		}

		std::string lastConnectedDeviceName;
		if (m_deviceList.size()) {
			lastConnectedDeviceName = m_deviceList.front();
		}
		size_t prevDeviceListCount = m_deviceList.size();

		std::unordered_set<std::string> connectedDeviceList;

		// 全接続デバイス取得
		auto deviceList = GetDevices();
		bool lastConnctedDeviceFound = false;
		for (const std::string& device : deviceList) {
			// wifiならシリアル確認
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
			if (!lastConnctedDeviceFound) {	// 前回接続したデバイスが見つからなかった場合
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

		// 自動実行
		if (m_bFirstDeviceCheck && m_config.autoStart) {
			if (lastConnctedDeviceFound) {
				PUTLOG(L"2秒後に、自動実行します");
				//m_checkSSC.SetCheck(BST_CHECKED);
				//BOOL bHandled = TRUE;
				//OnScreenSoundCopy(0, 0, NULL, bHandled);
				SetTimer(kAutoRunTimerId, kAutoRunTimerInterval);

			} else {
				PUTLOG(L"前回接続したデバイスが見つからないため、自動実行はキャンセルされます");
			}
		}

		// デバイス側へwifi経由でのadb待ち受けを行うように指示する
		if (m_config.autoWifiConnect) {
			if (m_bFirstDeviceCheck || (prevDeviceListCount != m_deviceList.size())) {
				const int acceptPort = 5555;

				// デバイスへ待ち受け要求する
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
						PUTLOG(L"デバイスがローカルwifiに接続されていません: %s", UTF16fromUTF8(device).c_str());
						continue;
					}
					auto slashPos = wlanText.find("/", ipBeginPos);
					if (slashPos == std::string::npos) {
						ATLASSERT(FALSE);
						continue;
					}

					std::string deviceIPAddress = wlanText.substr(ipBeginPos, slashPos - ipBeginPos);
					std::string deviceIPPort = deviceIPAddress + ":" + std::to_string(acceptPort);
					// 待ち受け
					_SendADBCommand(L"tcpip " + std::to_wstring(acceptPort), device);
					// 接続
					StartProcess(GetAdbPath(), L" connect " + UTF16fromUTF8(deviceIPPort));
					PUTLOG(L"adb connect %s", UTF16fromUTF8(deviceIPPort).c_str());
				}
			}
		}

		m_bFirstDeviceCheck = false;
	});
	
	{	// 終了時に、デバイスとadbとの接続が切れないようにする
		extern HANDLE g_hJob;
		// ハンドルが継承されないようにする
		if (!SetHandleInformation(g_hJob, HANDLE_FLAG_INHERIT, 0)) {
			ATLASSERT(FALSE);
		}
		//::ShellExecute(NULL, NULL, GetAdbPath().c_str(), L" kill-server", NULL, SW_HIDE);
		//::Sleep(1000);

		//::ShellExecute(NULL, NULL, GetAdbPath().c_str(), L" start-server", NULL, SW_HIDE);

		// ハンドルが継承されるようにする
		if (!SetHandleInformation(g_hJob, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
			ATLASSERT(FALSE);
		}
	}

	bool success = m_adbTrackDevicesProcess.StartProcess(GetAdbPath().wstring(), L" track-devices", false);
	if (!success) {
		PUTLOG(L"adb track-devicesに失敗");
		ATLASSERT(FALSE);
	}
}

bool CMainDlg::_ScrcpyStart()
{
	ATLASSERT(!m_scrcpyProcess.IsRunning());
	ATLASSERT(!m_sharedMemFilemap);

	auto scrcpyPath = GetExeDirectory() / L"scrcpy" / L"scrcpy.exe";

	auto notify = std::make_shared<std::optional<bool>>();
	auto mtx = std::make_shared<std::mutex>();
	auto cond = std::make_shared<std::condition_variable>();

	auto firstTextureNotify = std::make_shared<bool>(true);

	m_scrcpyProcess.RegisterStdOutCallback([=](const std::string& text) {
		
		std::string log = text;
		if (log.back() == '\n') {
			log = log.substr(0, log.size() - 2);
		}
		PUTLOG(L"[scrcpy] %s", UTF16fromUTF8(log).c_str());

		if (log.find("Killing the server") != std::string::npos ||
			log.find("Device disconnected") != std::string::npos)
		{
			// scrcpyの画面が閉じられた
			if (m_checkSSC.GetCheck() == BST_CHECKED) {
				m_checkSSC.SetCheck(BST_UNCHECKED);
				PostMessage(WM_COMMAND, IDC_CHECK_SCREENSOUNDCOPY);
			}
		} else if (log.find("INFO: Device:") != std::string::npos) {
			// 接続成功
			std::lock_guard<std::mutex> lock(*mtx);
			notify->emplace(true);
			cond->notify_all();

		} else if (log.find("ERROR: Device is unauthorized") != std::string::npos) {
			// 接続失敗
			std::lock_guard<std::mutex> lock(*mtx);
			notify->emplace(false);
			cond->notify_all();

		} else if (log.find("Texture: ") != std::string::npos) {
			if (*firstTextureNotify) {
				*firstTextureNotify = false;
				return;		// 初回起動時に実行するとscrcpyが強制終了する
			}

			CWindow hwndScrcpy = _FindScrcpyWindow();
			if (!hwndScrcpy) {
				PUTLOG(L"Scrcpyのウィンドウが見つかりません...");
				return;
			}
			ATLASSERT(m_sharedMemoryData);
			if (m_sharedMemoryData) {
				// Ctrl+Gを実行する
				PUTLOG(L"Resize window to 1:1 (pixel-perfect)");
				m_sharedMemoryData->doEventFlag = true;
				hwndScrcpy.SendMessageW(WM_MOUSEMOVE, 0, 0);
			}
			return;
#if 0
			//   Texture: 1280x800
			std::regex rx(R"(Texture: (\d+)x(\d+))");
			std::smatch	result;
			if (std::regex_search(log, result, rx)) {
				int width = std::stoi(result[1].str());
				int height = std::stoi(result[2].str());
				if (width > height) {
					// 横画面になったとき
					CWindow hwndScrcpy = _FindScrcpyWindow();
					if (!hwndScrcpy) {
						PUTLOG(L"Scrcpyのウィンドウが見つかりません...");
						return;
					}
					ATLASSERT(m_sharedMemoryData);
					if (m_sharedMemoryData) {
						// Ctrl+Gを実行する
						PUTLOG(L"Resize window to 1:1 (pixel-perfect)");
						m_sharedMemoryData->doEventFlag = true;
						hwndScrcpy.SendMessageW(WM_MOUSEMOVE, 0, 0);
					}
				}
			}
#endif
		}
	});

	std::wstring scrcpyCommandLine = _BuildScrcpyCommandLine();
	scrcpyCommandLine += L" --serial " + m_currentDeviceSerial;
	PUTLOG(L"scrcpyCommandLine: %s", scrcpyCommandLine.c_str());
	bool success = m_scrcpyProcess.StartProcess(scrcpyPath.wstring(), scrcpyCommandLine);
	if (!success) {
		PUTLOG(L"scrcpyプロセスの実行に失敗...");
		return false;
	}

	ATLVERIFY(_DelayFrameInject());

#if 0
	// scrcpyのコンソールウィンドウを非表示にする
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
	std::unique_lock<std::mutex> lock(*mtx);
#ifdef _DEBUG
	cond->wait_for(lock, std::chrono::seconds(60 * 60), [=] { return notify->has_value(); });
#else
	cond->wait_for(lock, std::chrono::seconds(5), [=] { return notify->has_value(); });
#endif
	const bool ret = notify->has_value() ? notify->value() : false /* timeout*/;
	return ret;
}

bool CMainDlg::_DelayFrameInject()
{
	// Dll inject実行
	const DWORD scrcpyProcessID = ::GetProcessId(m_scrcpyProcess.GetProcessInfomation().hProcess);
	std::wstring sharedMemName = L"delayFrame" + std::to_wstring(scrcpyProcessID);
	BOOL ret = FALSE;
	auto delayDllPath = GetExeDirectory() / L"delayFrame.dll";
	if (!fs::exists(delayDllPath)) {
		ATLASSERT(FALSE);
		PUTLOG(L"delayFrame.dllが存在しません...");
		return false;
	}

	m_sharedMemFilemap.Attach(
		::CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			sizeof(SharedMemoryData),        // maximum object size (low-order DWORD)
			sharedMemName.c_str())	 // name of mapping object
	);
	if (!m_sharedMemFilemap) {
		ATLASSERT(FALSE);
		PUTLOG(L"共有メモリの作成に失敗");
		return false;
	} else {
		m_sharedMemoryData = (SharedMemoryData*)::MapViewOfFile(m_sharedMemFilemap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryData));
		ATLASSERT(m_sharedMemoryData);
		if (!m_sharedMemoryData) {
			PUTLOG(L"MapViewOfFileに失敗");
			m_sharedMemFilemap.Close();
			return false;
		}

		// 共有メモリ初期化
		m_sharedMemoryData->delayFrameCount = m_config.delayFrameCount;	// 設定書き込み
		m_sharedMemoryData->doEventFlag = false;

		m_sharedMemoryData->hwndMainDlg = m_hWnd;

		m_sharedMemoryData->streamingReady = false;
		m_sharedMemoryData->simpleAudioReady = false;
		m_sharedMemoryData->bufferMultiple = m_config.bufferMultiple;
		m_sharedMemoryData->maxBufferSampleCount = m_config.maxBufferSampleCount;
		int soundVolume = kMaxVolume - m_sliderVolume.GetPos();
		m_sharedMemoryData->playSoundVolume = soundVolume;
		m_sharedMemoryData->nowSoundStreaming = false;
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

	// リモートプロセスへ読み込ませたいDLLのパスを書き込む
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

	// 自プロセス内のLoadlibraryへのアドレスをそのまま引数として渡す 
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

	return true;	// success!
}


void CMainDlg::_SndcpyAutoPermission(bool bManual /*= false*/)
{
	auto funcSSCButtonEnable = [this](bool success) {
		m_checkSSC.EnableWindow(TRUE);
		m_checkSSC.SetWindowTextW(success ? L"Streaming..." : L"failed...");
		if (!success) {
			m_checkSSC.SetCheck(BST_UNCHECKED);
			PostMessage(WM_COMMAND, IDC_CHECK_SCREENSOUNDCOPY);
		}
	};

	// Android version check
	enum { kSndcpyMinRequreAndroidVersion = 10 };
	if (m_currentDeviceAndroidVersion < kSndcpyMinRequreAndroidVersion) {
		PUTLOG(L"Androidのバージョンが古いためsndcpyは実行されません\nAndroid %d", m_currentDeviceAndroidVersion);
		funcSSCButtonEnable(true);
		return;
	}

	// ボタンを連打できないように一時的に無効化しておく
	m_checkSSC.EnableWindow(FALSE);
	m_checkSSC.SetWindowTextW(L"Prepareing...");


	std::thread([=]() {
		// sndcpy uninstall
		//_SendADBCommand(L" uninstall com.rom1v.sndcpy");

		CWindow hwnd_scrcpy;
		enum { kMaxRetryCount = 10 };
		for (int i = 0; i < kMaxRetryCount; ++i) {
			hwnd_scrcpy = _FindScrcpyWindow();
			if (!hwnd_scrcpy) {
				PUTLOG(L"_FindScrcpyWindow retry: %d", i);
				::Sleep(500);
			}
		}

		if (!hwnd_scrcpy) {
			PUTLOG(L"scrcpyの画面が見つかりません");
			ATLASSERT(FALSE);
			// Todo: ボタンを元に戻す処理を入れる
			return;
		}

		if (m_config.noResize) {
			hwnd_scrcpy.ModifyStyle(WS_THICKFRAME, 0);	// サイズ変更禁止にする
		}

		_DoSoundStreaming();

		m_checkSSC.EnableWindow(TRUE);
		m_checkSSC.SetWindowTextW(L"Streaming...");
		return ;	// success!
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
	ATLASSERT(m_scrcpyProcess.IsRunning());
	if (!m_scrcpyProcess.IsRunning() || !m_sharedMemoryData) {
		return;
	}

	m_sharedMemoryData->streamingReady = true;
	return;

	ATLASSERT(!m_threadSoundStreeming.joinable());
	m_cancelSoundStreaming = false;
	m_threadSoundStreeming = std::thread([this]()
		{
			auto funcSSCButtonEnable = [this]() {
				m_checkSSC.EnableWindow(TRUE);
				m_checkSSC.SetWindowTextW(L"Streaming...");
			};

			PUTLOG(L"sndcpyに接続します");

			CSocket sock;
			auto funcConnect = [&, this]() -> bool {
				IPAddress addr;
				addr.Set("127.0.0.1", "28200");
				std::atomic_bool valid = true;
				enum { kMaxConnectRetryCount = 15 };
				for (int i = 0; i < kMaxConnectRetryCount; ++i) {

					if (m_cancelSoundStreaming) {
						break;		// cancel
					}

					if (!sock.Connect(addr, valid)) {
						::Sleep(500);
					} else {
						sock.SetBlocking(true);
						char temp[64] = "";
						int recvSize = sock.Read(temp, 64);
						if (recvSize == 0) {
							PUTLOG(L"Connect retry: %d", i);
							::Sleep(500);
							continue;	// retry
						}
						break;
					}
				}
				if (!sock.IsConnected()) {
					PUTLOG(L"接続に失敗しました...");
					funcSSCButtonEnable();
					return false;		// failed

				} else {
					PUTLOG(L"接続成功");
					return true;	// success
				}
			};
			if (!funcConnect()) {
				return;
			}

			auto func_wavPlayInit = [this]() {
				m_wavePlay = std::make_unique<WavePlay>();
				m_wavePlay->SetMainDlgHWND(m_hWnd);
				m_wavePlay->Init(m_config.bufferMultiple, m_config.maxBufferSampleCount);

				m_wavePlay->SetVolume(kMaxVolume - m_sliderVolume.GetPos());
			};

			int reconnectCount = 0;
			auto funcReConnect = [&, this]() -> bool {
				++reconnectCount;
				PUTLOG(L"ReConnect: %d", reconnectCount);
				std::string outret = _SendADBCommand(L"shell am start com.rom1v.sndcpy/.MainActivity --activity-clear-top"); // お試し --activity-clear-top

				return funcConnect();
			};

			using namespace std::chrono;
			auto prevTime = steady_clock::now();

			auto funcMutePlayStop = [&](const char* buffer, int recvSize) -> bool {
				//enum { kMaxCount = 32 };
				//int i64size = min(recvSize / 8, kMaxCount);
				int i64size = recvSize / 8;
				const std::int64_t* bufferBegin = reinterpret_cast<const std::int64_t*>(buffer);
				//Utility::timer timer;
				bool bMute = std::all_of(bufferBegin, bufferBegin + i64size, [](std::int64_t c) { return c == 0; });
				//bool bMute = std::all_of(buffer, buffer + recvSize, [](char c) { return c == 0; });
				//INFO_LOG << L"all_of: " << timer.format();
				if (bMute) {
					if (!m_wavePlay) {
						return true;	// 再生ストップ
					}

					// 一定時間ミュートが続く、かつ、画面がオフの時に再生を止める
					enum { kDisplayOffConfirmMinutes = 1 };
					using namespace std::chrono;
					auto nowTime = steady_clock::now();
					auto elapsed = duration_cast<minutes>(nowTime - prevTime).count();
					if (kDisplayOffConfirmMinutes <= elapsed) {
						prevTime = nowTime;

						std::string stdoutText = _SendCommonAdbCommand("DumpsysPower");
						std::regex rx(R"(mWakefulness=([^\r]+))");
						std::smatch result;
						if (std::regex_search(stdoutText, result, rx)) {
							std::string displayState = result[1].str();
							if (displayState != "Awake") {
								m_wavePlay.reset();	
								m_wavePlayInfo.SetWindowText(L"Pausing playback");
								return true;	// 再生ストップ
							}
						}
					}
				} else {
					// 音が出ている
					if (!m_wavePlay) {
						// 再生を開始する
						func_wavPlayInit();
					}
				}
				return false;	// 再生を止めない
			};

			func_wavPlayInit();

			sock.SetBlocking(true);
			const int bufferSize = m_wavePlay->GetBufferBytes();
			auto buffer = std::make_unique<char[]>(bufferSize);

			// 最初に送られてくる音声データを破棄する (最新のデータを取得するため)
			enum { kDataDiscardSec = 1 };
			auto timebegin = std::chrono::steady_clock::now();
			while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - timebegin).count() < kDataDiscardSec) {
				int recvSize = sock.Read(buffer.get(), bufferSize);
			}
			PUTLOG(L"音声のストリーミング再生を開始します");
			funcSSCButtonEnable();

			while (!m_cancelSoundStreaming) {
				int recvSize = sock.Read(buffer.get(), bufferSize);
				if (recvSize == 0 && sock.IsConnected()) {
					continue;
				}
				if (recvSize == -1) {
					PUTLOG(L"Socket Error - audio streaming finish");

					if (funcReConnect()) {
						continue;

					} else {
						break;
					}
				}
				if (funcMutePlayStop(buffer.get(), recvSize)) {
					continue;	// 音を再生しない
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
		PUTLOG(L"ストリーミングを停止します");

		HWND hwnd_scrcpy = _FindScrcpyWindow();
		if (hwnd_scrcpy) {
			if (::IsIconic(hwnd_scrcpy) == FALSE) {
				//CRect rcWindow;
				//::GetWindowRect(hwnd_scrcpy, &rcWindow);
				
				CRect rcClient;
				::GetClientRect(hwnd_scrcpy , &rcClient);
				CWindow(hwnd_scrcpy).MapWindowPoints(NULL, &rcClient);

				if (::GetAsyncKeyState(VK_CONTROL) < 0) {
					m_scrcpyWidowPos.x = rcClient.left;
					m_scrcpyWidowPos.y = rcClient.top;
					PUTLOG(L"Save scrcpy window pos - x: %d y: %d", m_scrcpyWidowPos.x, m_scrcpyWidowPos.y);
				}
			}

			::PostMessage(hwnd_scrcpy, WM_CLOSE, 0, 0);
		} else {
			// scrcpy本体の閉じるボタンが押された場合
		}

		if (m_config.deviceMuteOnStart) {
			_SendCommonAdbCommand("UnMute");
		}
		// sndcpy uninstall
		//_SendADBCommand(L" uninstall com.rom1v.sndcpy");

		m_scrcpyProcess.Terminate();

		m_checkSSC.SetCheck(BST_UNCHECKED);
		m_checkSSC.SetWindowTextW(L"Screen Sound Copy");
		m_currentDeviceSerial.clear();
	}
	if (m_sharedMemFilemap) {
		::UnmapViewOfFile((LPCVOID)m_sharedMemoryData);
		m_sharedMemoryData = nullptr;
		m_sharedMemFilemap.Close();
	}

	if (m_threadSoundStreeming.joinable()) {
		m_cancelSoundStreaming = true;
		m_threadSoundStreeming.detach();
	}
	PUTLOG(L"停止完了");
}

std::wstring CMainDlg::_BuildScrcpyCommandLine()
{
	// 	std::wstring scrcpyCommandLine = LR"(  --turn-screen-off --window-title "My device" --shortcut-mod=lctrl,lalt,ralt,rctrl --window-x 1920 --window-y 30  --max-fps 30 --max-size 1280 --bit-rate 20M )
	std::wstring commandLine = UTF16fromUTF8(m_jsonCommon["Common"]["Scrcpy"]["CommandLine"].get<std::string>());
	if (m_config.maxSize > 0 && !m_nativeResolution) {
		commandLine += L" --max-size " + std::to_wstring(m_config.maxSize);
	}
	if (m_config.maxFPS > 0) {
		commandLine += L" --max-fps " + std::to_wstring(m_config.maxFPS);
	}
	if (m_config.bitrate > 0) {
		commandLine += L" --video-bit-rate " + std::to_wstring(m_config.bitrate) + L"M";
	}
	if (m_config.turnScreenOff) {
		commandLine += L"  --turn-screen-off ";
	}
	if (m_scrcpyWidowPos.x != -1 && m_scrcpyWidowPos.y != -1) {
		commandLine += L" --window-x " + std::to_wstring(m_scrcpyWidowPos.x) 
					 + L" --window-y " + std::to_wstring(m_scrcpyWidowPos.y);
	}
	if (m_config.videoBuffer_ms > 0) {
		commandLine += L" --video-buffer=" + std::to_wstring(m_config.videoBuffer_ms);
	}
	if (m_config.enableUHID) {
		commandLine += L" --keyboard=uhid ";
	}
	if (!m_config.useScrcpyAudio) {
		commandLine += L" --no-audio ";
	} else {
		commandLine += L" --audio-dup ";
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
	std::wstring adbCommand;
	auto jsonAdbCommand = m_jsonCommon["Common"]["AdbCommand"][commandName];
	if (jsonAdbCommand.is_null()) {
		// 現在のAndroid versionに合うAdbCommandを見つける
		if (m_currentDeviceAndroidVersion != 0) {
			std::string androidVersion = "Android" + std::to_string(m_currentDeviceAndroidVersion);
			auto jsonCurrentAndroidDeviceCommand = m_jsonCommon["Common"]["AdbCommand"][androidVersion];
			if (jsonCurrentAndroidDeviceCommand.is_object()) {
				jsonAdbCommand = jsonCurrentAndroidDeviceCommand[commandName];
			}
		}
		// デフォルトのAdbCommandを見つける
		if (jsonAdbCommand.is_null()) {
			std::string androidVersion = "Android" + std::to_string(m_jsonCommon["Common"]["DefaultAdbCommandAndroidVersion"].get<int>());
			jsonAdbCommand = m_jsonCommon["Common"]["AdbCommand"][androidVersion][commandName];
		}
		ATLASSERT(!jsonAdbCommand.is_null());
		if (jsonAdbCommand.is_null()) {
			PUTLOG(L"コマンドが見つかりません: %s", UTF16fromUTF8(commandName).c_str());
			return "";
		}

		adbCommand = UTF16fromUTF8(jsonAdbCommand.get<std::string>());

	} else {
		adbCommand = UTF16fromUTF8(m_jsonCommon["Common"]["AdbCommand"][commandName].get<std::string>());
	}
	ATLASSERT(adbCommand.length());	
	return _SendADBCommand(adbCommand, deviceSerial);
}

void CMainDlg::_DoCustomAdbCommand()
{
	auto customCommandTxtPath = GetExeDirectory() / (m_currentDeviceSerial + L".txt");
	if (!fs::exists(customCommandTxtPath)) {
		return;
	}

	std::ifstream fs(customCommandTxtPath.wstring());
	if (!fs) {
		ATLASSERT(FALSE);
		return;
	}

	std::string line;
	while (!fs.eof() && std::getline(fs, line)) {
		std::wstring command = CodeConvert::UTF16fromUTF8(line);
		PUTLOG(L"CustomAdbCommand: %s", command.c_str());
		_SendADBCommand(command);
		::Sleep(500);
	}	
}

void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}

