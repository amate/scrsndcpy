#pragma once

#include <thread>
#include <string>
#include <functional>

class ProcessIO
{
public:
	ProcessIO();
	~ProcessIO();

	bool	StartProcess(const std::wstring& exePath, const std::wstring& commandLine, bool showWindow = true);

	// ���������v���Z�X�̕W�����͂ɏ�������
	void	WriteStdIn(const std::string& text);

	// ���������v���Z�X�̕W���o�͂��󂯎��R�[���o�b�N��o�^����
	void	RegisterStdOutCallback(std::function<void(const std::string&)> callback) {
		m_stdoutCallback = callback;
	}

	bool	IsRunning() const {
		return m_processThread.joinable();
	}

	void	Terminate();

private:
	void	_IOThreadMain();

	std::wstring	m_exePath;

	CHandle	m_stdinWrite;	//	pipe -> process
	CHandle	m_stdoutRead;	//	pipe <- process

	PROCESS_INFORMATION m_processInfo = {};

	std::thread	m_processThread;
	std::atomic_bool	m_threadCancel = false;
	std::function<void (const std::string&)> m_stdoutCallback;
};
