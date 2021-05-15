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


class WavePlay
{
public:
	WavePlay();
	~WavePlay();

	bool	Init(int buffer_ms);
	void	Term();

	int		GetBufferBytes() const {
		return m_bufferBytes;
	}

	void	WriteBuffer(const BYTE* buffer, int bufferSize);

private:

	void	_BufferConsume();
#if 0

	HWAVEOUT m_hWaveOut = NULL;

	int		m_bufferIndex = 0;
	std::vector<BYTE> m_buffer[2];
	WAVEHDR	m_wh[2];
#endif
	CComPtr<IAudioClient3> spAudioClient;
	CComPtr<IAudioRenderClient> spRenderClient;

	int		m_frameSize;
	std::vector<BYTE> m_buffer;
	std::chrono::steady_clock::time_point	m_bufferTimestamp;
	LONGLONG	m_playTime_ns;

	WAVEFORMATEXTENSIBLE format_ = {};
	UINT64	last_qpc_position_ = 0;

	uint64_t m_last_played_out_frames = 0;

	uint64_t	m_diffPlaytimeRealTime = 0;

	int		m_bufferBytes;

	std::mutex m_mtx;
	std::condition_variable	m_cond;

	std::thread	m_threadBufferConsume;
	bool		m_exit = false;
};

