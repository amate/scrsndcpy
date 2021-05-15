
#include "stdafx.h"
#include "MainDlg.h"
#include <thread>

#include "WavePlay.h"

#include "Utility\CommonUtility.h"
#include "Utility\Logger.h"
#include "Socket.h"


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

	return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD, WORD, HWND, BOOL&)
{
	//CAboutDlg dlg;
	//dlg.DoModal();
	std::thread([]()
		{

			IPAddress addr;
			addr.Set("127.0.0.1", "28200");
			CSocket sock;
			std::atomic_bool valid = true;
			while (!sock.Connect(addr, valid)) {
				::Sleep(100);
			}

			WavePlay wavePlay;
			wavePlay.Init(30);
			const int bufferSize = wavePlay.GetBufferBytes();

			sock.SetBlocking(true);
			auto buffer = std::make_unique<char[]>(bufferSize);

			bool bFirst = true;
			for (;;) {
				//char buffer[1024];
				int recvSize = sock.Read(buffer.get(), bufferSize);
				if (recvSize == 0 || bFirst) {
					bFirst = false;
					continue;
				}
				if (recvSize == -1) {
					ERROR_LOG << L"Socket Error";
					return;
				}
				//ATLASSERT(recvSize == bufferSize);
				//PUTLOG(L"recvSize: %d", recvSize);
				int a = 0;

				if (::GetAsyncKeyState(VK_MENU) < 0) {
					continue;	// drain
				}

				wavePlay.WriteBuffer((const BYTE*)buffer.get(), recvSize);

				//wh.lpData = buffer.get();
				//wh.dwBufferLength = recvSize;
				//ret = ::waveOutPrepareHeader(hWaveOut, &wh, sizeof(WAVEHDR));
				//while (ret != MMSYSERR_NOERROR) {
				//	::Sleep(100);
				//}
				//ret = ::waveOutWrite(hWaveOut, &wh, sizeof(WAVEHDR));
				//while (waveOutUnprepareHeader(hWaveOut, &wh, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
				//	Sleep(10);
				//}
				//bool success = ret == MMSYSERR_NOERROR;
			}

			//::waveOutReset(hWaveOut);
			//::waveOutUnprepareHeader(hWaveOut, &wh, sizeof(WAVEHDR));
			//::waveOutClose(hWaveOut);
		}
	).detach();
	return 0;
}

LRESULT CMainDlg::OnOK(WORD, WORD wID, HWND, BOOL&)
{
	auto scrcpyPath = GetExeDirectory() / L"scrcpy" / L"scrcpy.exe";
	auto sndcpyPath = GetExeDirectory() / L"sndcpy" / L"sndcpy_start.bat";

	WCHAR systemFolder[MAX_PATH] = L"";
	::GetSystemDirectory(systemFolder, MAX_PATH);
	auto cmdPath = fs::path(systemFolder) / L"cmd.exe";
	std::wstring commandLine = L"/S /C \"";
	commandLine += L"\"" + sndcpyPath.wstring() + L"\"";
	commandLine += L" R52N9143DNH";
	commandLine += L"\"";

	std::thread([=]() {
		std::string stdoutText;
		//DWORD ret = StartProcessGetStdOut(scrcpyPath, LR"( --window-title "My device" --shortcut-mod=lctrl,lalt,ralt,rctrl --window-x 1920 --window-y 30 --serial R52N9143DNH --max-fps 30 --max-size 1280 --bit-rate 20M)", stdoutText);
		DWORD ret = StartProcessGetStdOut(cmdPath, commandLine, stdoutText);
		}).detach();




	return 0;
}

LRESULT CMainDlg::OnCancel(WORD, WORD wID, HWND, BOOL&)
{
	CloseDialog(wID);
	return 0;
}

void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}

