
#include "stdafx.h"
#include "MainDlg.h"
#include <thread>
#include <chrono>

#include "WavePlay.h"

#include "Utility\CommonUtility.h"
#include "Utility\Logger.h"
#include "Utility\CodeConvert.h"
#include "Utility\json.hpp"
#include "TesseractWrapper.h"
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

	TesseractWrapper::TesseractInit();

	DoDataExchange(DDX_LOAD);

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

	TesseractWrapper::TesseractTerm();

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
		PUTLOG(L"ストリーミングを停止します");
		_StopStreaming();
		m_checkSSC.SetWindowTextW(L"Screen Sound Copy");
		m_currentDeviceSerial.clear();

	} else {
		m_checkSSC.EnableWindow(FALSE);
		m_checkSSC.SetWindowTextW(L"Prepareing...");

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

		if (m_config.deviceMuteOnStart) {
			_SendADBCommand(_GetAdbCommand("Mute"));
		}

		auto scrcpyPath = GetExeDirectory() / L"scrcpy" / L"scrcpy.exe";

		m_scrcpyProcess.RegisterStdOutCallback([this](const std::string& text) {
			std::string log = text;
			if (log.back() == '\n') {
				log = log.substr(0, log.size() - 2);
			}
			PUTLOG(L"[scrcpy] %s", UTF16fromUTF8(log).c_str());

			if (log.find("Killing the server") != std::string::npos) {
				// scrcpyの画面が閉じられた
				if (m_checkSSC.GetCheck() == BST_CHECKED) {
					m_checkSSC.SetCheck(BST_UNCHECKED);
					BOOL bHandled = FALSE;
					OnScreenSoundCopy(0, 0, NULL, bHandled);
				}
			}
			});

		// scrcpy
		std::wstring scrcpyCommandLine = _BuildScrcpyCommandLine();
		scrcpyCommandLine += L" --serial " + deviceSerial;
		PUTLOG(L"scrcpyCommandLine: %s", scrcpyCommandLine.c_str());
		m_scrcpyProcess.StartProcess(scrcpyPath.wstring(), scrcpyCommandLine);

		if (m_config.loginPassword.size()) {
			std::string stdoutText = _SendADBCommand(_GetAdbCommand("IsScreenLock"));
			if (stdoutText.find("00000001") != std::string::npos) {	// 画面がロックされている
				PUTLOG(L"自動ログインします");
				// ロック解除画面へ
				_SendADBCommand(L"shell input keyevent MENU");
				_SendADBCommand(L"shell input keyevent MENU");
				// パスワード入力
				_SendADBCommand(L"shell input text " + UTF16fromUTF8(m_config.loginPassword));
				PUTLOG(L"自動ログイン完了");
			}
		}
		

		// sndcpy
		_SndcpyAutoPermission(deviceSerial);

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
		//m_checkSSC.EnableWindow(TRUE);
		//m_checkSSC.SetWindowTextW(L"Streaming...");
	}
	return 0;
}


void CMainDlg::OnToggleMute(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	// Android11で有効
	// i32 3  3 はメディア音量
	// i32 101 101 は ADJUST_TOGGLE_MUTE
	// https://developer.android.com/reference/android/media/AudioManager#ADJUST_TOGGLE_MUTE
	// i32 0 はビットフラグ 1でUI表示 4で音量変更時に音が鳴る
	_SendADBCommand(_GetAdbCommand("ToggleMute"));
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
	_SndcpyAutoPermission(m_currentDeviceSerial, true);
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
		// 最後に接続したデバイス以外をコンボボックスから削除
		while (m_cmbDevices.DeleteString(1) != CB_ERR) {
			m_deviceList.erase(m_deviceList.begin() + 1);
		}

		std::string lastConnectedDeviceName;
		if (m_deviceList.size()) {
			lastConnectedDeviceName = m_deviceList.front();
		}

		auto deviceList = GetDevices();
		bool lastConnctedDeviceFound = false;
		for (const std::string& device : deviceList) {
			if (device == lastConnectedDeviceName) {
				lastConnctedDeviceFound = true;
				continue;
			}
			std::wstring u16deviceName = UTF16fromUTF8(device);
			int n = m_cmbDevices.AddString(u16deviceName.c_str());
			m_cmbDevices.SetItemData(n, true);
			m_deviceList.push_back(device);
		}

		if (lastConnectedDeviceName.length()) {
			if (!lastConnctedDeviceFound) {	// 前回接続したデバイスが見つからなかった場合
				lastConnectedDeviceName.insert(0, "*[disconnect]");
			} else {
				lastConnectedDeviceName = "*" + m_deviceList.front();
			}
			m_cmbDevices.DeleteString(0);
			m_cmbDevices.InsertString(0, UTF16fromUTF8(lastConnectedDeviceName).c_str());
			m_cmbDevices.SetItemData(0, lastConnctedDeviceFound);
		}

		if (m_cmbDevices.GetCount() <= curIndex) {
			--curIndex;
		}
		m_cmbDevices.SetCurSel(curIndex);

		});
	bool success = m_adbTrackDevicesProcess.StartProcess(GetAdbPath().wstring(), L" track-devices", false);
	if (!success) {
		PUTLOG(L"adb track-devicesに失敗");
		ATLASSERT(FALSE);
	}
}

void CMainDlg::_SndcpyAutoPermission(const std::wstring& deviceSerial, bool bManual /*= false*/)
{
	std::thread([this, deviceSerial, bManual]()
	{
		auto funcSSCButtonEnable = [this]() {
			m_checkSSC.EnableWindow(TRUE);
			m_checkSSC.SetWindowTextW(L"Streaming...");
		};
		enum { kMaxSndcpyRetryCount = 2 };
		for (int k = 0; k < kMaxSndcpyRetryCount; ++k) {
			PUTLOG(L"sndcpy起動");
			auto sndcpyPath = GetExeDirectory() / L"sndcpy" / L"sndcpy_start.bat";

			WCHAR systemFolder[MAX_PATH] = L"";
			::GetSystemDirectory(systemFolder, MAX_PATH);
			auto cmdPath = fs::path(systemFolder) / L"cmd.exe";
			std::wstring commandLine = L"/S /C \"";
			commandLine += L"\"" + sndcpyPath.wstring() + L"\" ";
			commandLine += deviceSerial;
			commandLine += L"\"";

			std::string stdoutText;
			StartProcessGetStdOut(cmdPath, commandLine, stdoutText);
			PUTLOG(L"[sndcpy.bat] %s", UTF16fromUTF8(stdoutText).c_str());

			HWND hwnd_scrcpy = _FindScrcpyWindow();
			if (!hwnd_scrcpy) {
				PUTLOG(L"scrcpyの画面が見つかりません");
				funcSSCButtonEnable();
				return;
			}

			if (bManual) {
				int ret = MessageBox(L"デバイス側でsndcpyの[今すぐ開始]を押した後に[OK]を選択してください。", L"scrsndcpy", MB_OKCANCEL);
				if (ret == IDOK) {
					_DoSoundStreaming();
				}
				funcSSCButtonEnable();
				return;
			} else {
				::Sleep(500);	// ダイアログが出てくるまで待つ
				PUTLOG(L"sndcpyダイアログ確認中...");

				const std::string searchText = m_jsonCommon["Common"]["Sndcpy"]["AuthorizationGrep"];
				enum { kMaxRetryCount = 10 };
				for (int i = 0; i < kMaxRetryCount; ++i) {
					std::string adbRet = _SendADBCommand(_GetAdbCommand("IsSndcpyAuthorization"));
					if (adbRet.find(searchText) != std::string::npos) {
						PUTLOG(L"認証ダイアログ発見、自動認証します");
						_SendADBCommand(L"shell input keyevent DPAD_RIGHT");
						_SendADBCommand(L"shell input keyevent DPAD_RIGHT");
						_SendADBCommand(L"shell input keyevent ENTER");
						PUTLOG(L"認証しました");

						::Sleep(500);
						_DoSoundStreaming();
						return;	// success!

					}
					PUTLOG(L"retry count: %d", i);
					::Sleep(300);
				}
#if  0
				auto ssPath = GetExeDirectory() / L"screenshot.png";
				enum { kMaxRetryCount = 5 };
				for (int i = 0; i < kMaxRetryCount; ++i) {
					bool success = SaveWindowScreenShot(hwnd_scrcpy, ssPath.wstring());
					if (!success) {
						PUTLOG(L"スクリーンショットの保存に失敗");
						funcSSCButtonEnable();
						return;
					}
	#ifdef _DEBUG
					std::wstring ocr_text = L"sndcpy";
	#else
					std::wstring ocr_text = TesseractWrapper::TextFromImage(ssPath.wstring());
	#endif
					if (ocr_text.find(L"sndcpy") != std::wstring::npos) {
						PUTLOG(L"認証ダイアログ発見、自動認証します");
						_SendADBCommand(L"shell input keyevent DPAD_RIGHT");
						_SendADBCommand(L"shell input keyevent DPAD_RIGHT");
						_SendADBCommand(L"shell input keyevent ENTER");
						PUTLOG(L"認証しました");

						::Sleep(500);
						_DoSoundStreaming();
						return;	// success!
					}
					PUTLOG(L"retry count: %d", i);
				}
#endif
			}
			PUTLOG(L"sndcpyのダイアログが確認できませんでした...");
		}
	}).detach();
}


std::string CMainDlg::_SendADBCommand(const std::wstring& command)
{
	std::wstring addserialCommand = L" -s " + m_currentDeviceSerial + L" " + command;
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

			PUTLOG(L"sndcpyに接続します");

			IPAddress addr;
			addr.Set("127.0.0.1", "28200");
			CSocket sock;
			std::atomic_bool valid = true;
			enum { kMaxConnectRetryCount = 10 };
			for (int i = 0; i < kMaxConnectRetryCount; ++i) {
				if (!sock.Connect(addr, valid)) {
					::Sleep(500);
				} else {
					PUTLOG(L"接続成功");
					break;
				}
			}
			if (!sock.IsConnected()) {
				PUTLOG(L"接続に失敗しました...");
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
			PUTLOG(L"音声のストリーミング再生を開始します");
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
				_SendADBCommand(_GetAdbCommand("UnMute"));
			}

			::PostMessage(hwnd_scrcpy, WM_CLOSE, 0, 0);
		}
	}

	if (m_threadSoundStreeming.joinable()) {
		m_cancelSoundStreaming = true;
		m_threadSoundStreeming.join();
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

std::wstring CMainDlg::_GetAdbCommand(const std::string& commandName)
{
	std::wstring adbCommand = UTF16fromUTF8(m_jsonCommon["Common"]["AdbCommand"][commandName].get<std::string>());
	return adbCommand;
}

void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}

