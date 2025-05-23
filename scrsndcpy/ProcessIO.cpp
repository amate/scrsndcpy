#include "stdafx.h"
#include "ProcessIO.h"


ProcessIO::ProcessIO()
{
}

ProcessIO::~ProcessIO()
{
	Terminate();
}

bool ProcessIO::StartProcess(const std::wstring& exePath, const std::wstring& commandLine, bool showWindow /*= true*/)
{
	ATLASSERT(!m_processThread.joinable());

	m_exePath = exePath;	// for debug
	INFO_LOG << L"StartProcess - exePath: " << exePath << L" commandLine: " << commandLine;

	m_threadCancel = false;

	SECURITY_ATTRIBUTES securityAttributes = { sizeof(SECURITY_ATTRIBUTES) };
	securityAttributes.bInheritHandle = TRUE;

	// 標準入力用のパイプハンドル作成
	CHandle stdinRead;		// 子プロセスへ渡す
	if (!::CreatePipe(&stdinRead.m_h, &m_stdinWrite.m_h, &securityAttributes, 0))
		throw std::runtime_error("CreatePipe failed");

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(m_stdinWrite, HANDLE_FLAG_INHERIT, 0))	// ハンドルが継承されないようにする
		throw std::runtime_error("Stdin SetHandleInformation");

	// 標準出力用のパイプハンドル作成
	CHandle stdoutWrite;	// 子プロセスへ渡す
	if (!::CreatePipe(&m_stdoutRead.m_h, &stdoutWrite.m_h, &securityAttributes, 0))
		throw std::runtime_error("CreatePipe failed");

	// Ensure the write handle to the pipe for STDIN is not inherited. 
	if (!SetHandleInformation(m_stdoutRead, HANDLE_FLAG_INHERIT, 0))	// ハンドルが継承されないようにする
		throw std::runtime_error("Stdout SetHandleInformation");

	STARTUPINFO startUpInfo = { sizeof(STARTUPINFO) };
	startUpInfo.dwFlags = STARTF_USESTDHANDLES;
	startUpInfo.hStdInput = stdinRead;
	startUpInfo.hStdOutput = stdoutWrite;
	startUpInfo.hStdError = stdoutWrite;
	if (!showWindow) {
		startUpInfo.dwFlags |= STARTF_USESHOWWINDOW;
		startUpInfo.wShowWindow = SW_HIDE;
	}

	BOOL bRet = ::CreateProcess(exePath.c_str(), (LPWSTR)commandLine.data(),
		nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startUpInfo, &m_processInfo);
	ATLASSERT(bRet);
	if (!bRet) {
		WARN_LOG << L"CreateProcess failed: " << exePath;
		return false;	// failed
	}

	m_processThread = std::thread([this]() {
		_IOThreadMain();
	});

	return bRet != 0;
}

void ProcessIO::WriteStdIn(const std::string& text)
{
	ATLASSERT(m_stdinWrite);

	DWORD dwWritten = 0;
	BOOL bRet = ::WriteFile(m_stdinWrite, (LPCVOID)text.c_str(), text.length(), &dwWritten, nullptr);
	ATLASSERT(bRet);
}

void ProcessIO::Terminate()
{
	INFO_LOG << L"TerminateProcess : " << m_exePath << L"\n hProcess: " << m_processInfo.hProcess << L"\n m_processThread.joinable(): " << m_processThread.joinable();

	if (m_processThread.joinable()) {

		m_threadCancel = true;
		//DWORD writeBytes = 0;
		//BOOL bRet = ::WriteFile(m_stdoutWrite, "exit", 4, &writeBytes, nullptr);

		BOOL b = ::TerminateProcess(m_processInfo.hProcess, 0);

		m_processThread.detach();
	}

	if (m_processInfo.hThread) {
		::CloseHandle(m_processInfo.hThread);
	}
	if (m_processInfo.hProcess) {
		::CloseHandle(m_processInfo.hProcess);
	}
	m_processInfo = PROCESS_INFORMATION{};

	m_stdinWrite.Close();
	m_stdoutRead.Close();

	INFO_LOG << L"TerminateProcess finish!";
}

void ProcessIO::_IOThreadMain()
{
	for (;;) {
		enum { kBufferSize = 512 };
		char buffer[kBufferSize + 1] = "";
		DWORD readSize = 0;
		BOOL bRet = ::ReadFile(m_stdoutRead, (LPVOID)buffer, kBufferSize, &readSize, nullptr);
		if (!bRet || readSize == 0) { // EOF

			DWORD error = GetLastError();
			break;
		}
		if (m_threadCancel) {
			break;
		}

		ATLASSERT(m_stdoutCallback);
		m_stdoutCallback(buffer);
	}
	INFO_LOG << L"_IOThreadMain - " << L" hProcess: " << m_processInfo.hProcess << L" - ReadFile EOF";

	// 終了待ち
	DWORD ret = ::WaitForSingleObject(m_processInfo.hProcess, 5 * 1000);
	if (ret == WAIT_TIMEOUT) {
		WARN_LOG << L"WaitForSingleObject(m_processInfo.hProcess - timeout";
	}

	if (!m_threadCancel.load()) {	// 手動キャンセル時に呼ぶとフリーズする
		m_stdoutCallback("[_IOThreadMain] Killing the server");
	}

	DWORD dwExitCode = 0;
	GetExitCodeProcess(m_processInfo.hProcess, &dwExitCode);

	INFO_LOG << L"_IOThreadMain - " << L" hProcess: " << m_processInfo.hProcess << L" - process finish - dwExitCode : " << dwExitCode;

	if (!m_threadCancel.load()) {
		INFO_LOG << L"_IOThreadMain - " << L" hProcess: " << m_processInfo.hProcess << L" - !m_threadCancel - normal finish";
		m_processThread.detach();
	}
}
