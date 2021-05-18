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

class CSocket;


class WavePlay
{
public:
	WavePlay();
	~WavePlay();

	bool	Init(int bufferMultiple, int playTimeRealTimeThreshold_ms);
	void	Term();

	int		GetBufferBytes() const {
		return m_bufferBytes;
	}

	void	WriteBuffer(const BYTE* buffer, int bufferSize);

	void	SetVolume(int volume);

	CSocket* m_pSock = nullptr;
private:

	void	_BufferConsume();
#if 0

	HWAVEOUT m_hWaveOut = NULL;

	int		m_bufferIndex = 0;
	std::vector<BYTE> m_buffer[2];
	WAVEHDR	m_wh[2];
#endif
	CComPtr<IAudioClient3> m_spAudioClient;
	CComPtr<IAudioRenderClient> m_spRenderClient;
	CComPtr<IAudioClock> m_spAudioClock;
	CComPtr<ISimpleAudioVolume> m_spSimpleAudioVolume;
	HANDLE	m_hTask = NULL;
	CEvent	m_eventBufferReady;
	
	WAVEFORMATEXTENSIBLE m_waveformat = {};

	UINT64	m_device_frequency;

	int		m_frameSize;
	std::string m_buffer;
	std::chrono::steady_clock::time_point	m_bufferTimestamp;
	LONGLONG	m_playTime_ns;

	UINT64	last_qpc_position_ = 0;

	uint64_t m_last_played_out_frames = 0;

	int64_t	m_diffPlaytimeRealTime = 0;

	int		m_bufferBytes;
	int		m_playTimeRealTimeThreshold_ms;

	std::mutex m_mtx;
	std::condition_variable	m_cond;

	std::thread	m_threadBufferConsume;
	bool		m_exit = false;
};

