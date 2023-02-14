#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <mmeapi.h>

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <FunctionDiscoveryKeys_devpkey.h>

#include <atlsync.h>


class WavePlay
{
public:
	WavePlay();
	~WavePlay();

	void	SetMainDlgHWND(HWND hWnd) {
		m_hWndMainDlg = hWnd;
	}

	bool	Init(int bufferMultiple, int maxBufferSampleCount);
	void	Term();

	int		GetBufferBytes() const {
		return m_bufferBytes;
	}

	void	WriteBuffer(const BYTE* buffer, int bufferSize);

	void	SetVolume(int volume);

private:
	void	_BufferConsume();

	enum {
		//kForceBufferClearSampleCount = 10000,	// m_maxBufferSampleCount * 2;�ɕύX
	};

	CComPtr<IAudioClient3> m_spAudioClient;
	CComPtr<IAudioRenderClient> m_spRenderClient;
	CComPtr<IAudioClock> m_spAudioClock;
	CComPtr<ISimpleAudioVolume> m_spSimpleAudioVolume;
	HANDLE	m_hTask = NULL;
	CEvent	m_eventBufferReady;
	
	WAVEFORMATEXTENSIBLE m_waveformat = {};

	int		m_bufferBytes;
	int		m_maxBufferSampleCount;
	int		m_frameSize;
	CCriticalSection m_cs;
	std::string m_buffer;

	std::thread	m_threadBufferConsume;
	bool		m_exit = false;

	HWND	m_hWndMainDlg = NULL;
};

