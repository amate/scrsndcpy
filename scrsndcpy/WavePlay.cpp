#include "stdafx.h"
#include "WavePlay.h"
#pragma comment(lib, "Winmm.lib")

#include "Utility\CodeConvert.h"



#pragma comment(lib, "Avrt.lib")

using namespace CodeConvert;


// =============================================

WavePlay::WavePlay()
{
	::CoInitialize(NULL);
}

WavePlay::~WavePlay()
{
	Term();

	::CoUninitialize();
}

#ifndef DEBUG
#define DEBUG
#endif

bool WavePlay::Init(int buffer_ms)
{
	try {
		HRESULT hr = S_FALSE;
		CComPtr<IMMDeviceEnumerator> spDeviceEnumrator;

		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

		// マルチメディアデバイス列挙子
		hr = spDeviceEnumrator.CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL);
		if (FAILED(hr)) {
			throw std::runtime_error("spDeviceEnumrator.CoCreateInstance failed");
		}

		// デフォルトのデバイスを選択
		CComPtr<IMMDevice> spDevice;
		hr = spDeviceEnumrator->GetDefaultAudioEndpoint(eRender, eConsole, &spDevice);
		if (FAILED(hr)) {
			throw std::runtime_error("spDeviceEnumrator->GetDefaultAudioEndpoint failed");
		}

		// オーディオクライアント
		hr = spDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&m_spAudioClient);
		if (FAILED(hr)) {
			throw std::runtime_error("spDevice->Activat failed");
		}

		// 2. Setting the audio client properties – note that low latency offload is not supported
		AudioClientProperties audioProps = { 0 };
		audioProps.cbSize = sizeof(AudioClientProperties);
		audioProps.eCategory = AudioCategory_Media;
		hr = m_spAudioClient->SetClientProperties(&audioProps);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->SetClientProperties failed");
		}

		// 再生音声のフォーマットの構築
		WAVEFORMATEXTENSIBLE wf = {};
		wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);           // = 22
		wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		wf.Format.nChannels = 2;
		wf.Format.nSamplesPerSec = 48000;
		wf.Format.wBitsPerSample = 16;
		wf.Format.nBlockAlign = wf.Format.nChannels * wf.Format.wBitsPerSample / 8;
		wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
		wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
		wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		wf.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

		m_waveformat = wf;

		// 1サンプルのサイズを保存(16bitステレオなら4byte)
		auto iFrameSize = wf.Format.nBlockAlign;
		m_frameSize = iFrameSize;

		// フォーマットのサポートチェック
		WAVEFORMATEX* pwfm = nullptr;
		hr = m_spAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wf.Format, &pwfm);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->IsFormatSupported failed");
		}
#if 0
		WAVEFORMATEX* pwfx = NULL;
		hr = m_spAudioClient->GetMixFormat(&pwfx);
		::CoTaskMemFree(pwfx);
#endif

		// 3. Querying the legal periods
		//WAVEFORMATEX* mixFormat = NULL;
		//ret = spAudioClient->GetMixFormat(&mixFormat);

		UINT32 defaultPeriodInFrames = 0;
		UINT32 fundamentalPeriodInFrames = 0;
		UINT32 minPeriodInFrames = 0;
		UINT32 maxPeriodInFrames;
		hr = m_spAudioClient->GetSharedModeEnginePeriod(&wf.Format, &defaultPeriodInFrames, &fundamentalPeriodInFrames, &minPeriodInFrames, &maxPeriodInFrames);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->GetSharedModeEnginePeriod failed");
		}
		// 5. Initializing a client with a specific format (if the format needs to be different than the default format)
		//AudioClientProperties audioProps = { 0 };
		//audioProps.cbSize = sizeof(AudioClientProperties);
		//audioProps.eCategory = AudioCategory_Media;
		//audioProps.Options |= AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;

		//ret = spAudioClient->SetClientProperties(&audioProps);

		//WAVEFORMATEX* closest;
		//ret = spAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wf.Format, &closest);
		//if (S_OK == ret) {
		//	/* device supports the app format */
		//} else if (S_FALSE == ret) {
		//	/* device DOES NOT support the app format; closest supported format is in the "closest" output variable */
		//} else {
		//	/* device DOES NOT support the app format, and Windows could not find a close supported format */
		//}

		hr = m_spAudioClient->InitializeSharedAudioStream(
			0,
			defaultPeriodInFrames,
			&wf.Format,
			nullptr); // audio session GUID
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->InitializeSharedAudioStream failed");
		}

		// レンダラーの取得
		hr = m_spAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_spRenderClient);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->GetService(__uuidof(IAudioRenderClient) failed");
		}

		CComPtr<IAudioClock> audio_clock_;
		hr = m_spAudioClient->GetService(__uuidof(IAudioClock), (void**)&m_spAudioClock);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->GetService(__uuidof(IAudioClock) failed");
		}

		hr = m_spAudioClock->GetFrequency(&m_device_frequency);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClock->GetFrequency failed");
		}

		// WASAPI情報取得
		UINT32 frame = 0;
		hr = m_spAudioClient->GetBufferSize(&frame);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->GetBufferSize failed");
		}

		const UINT32 bufferSize = frame * iFrameSize;
		//m_bufferBytes = bufferSize / 2;	// とりあえず最大バッファサイズを要求する
		m_bufferBytes = bufferSize;	// とりあえず最大バッファサイズを要求する

		// バッファに溜まっていたデータを初期化しておく
		LPBYTE pData = nullptr;
		hr = m_spRenderClient->GetBuffer(frame, &pData);
		if (SUCCEEDED(hr)) {
			m_spRenderClient->ReleaseBuffer(frame, AUDCLNT_BUFFERFLAGS_SILENT);
		} else {
			ATLASSERT(FALSE);
		}

		REFERENCE_TIME nsLatency = 0;
		hr = m_spAudioClient->GetStreamLatency(&nsLatency);
		INFO_LOG << L"GetStreamLatency: " << nsLatency << L"ns";

		// 再生開始
		hr = m_spAudioClient->Start();
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->Start() failed");
		}

		//m_threadBufferConsume = std::thread([this]() {
		//	_BufferConsume();
		//});
	} catch (std::exception& e) {
		ERROR_LOG << L"WavePlay::Init failed: " << UTF16fromUTF8(e.what());
		ATLASSERT(FALSE);
		return false;
	}

	return true;
}

void WavePlay::Term()
{
	std::unique_lock<std::mutex> lock(m_mtx);
	m_exit = true;
	lock.unlock();

	m_threadBufferConsume.join();
}

void WavePlay::WriteBuffer(const BYTE* buffer, int bufferSize)
{
	std::unique_lock<std::mutex> lock(m_mtx);
	m_buffer.insert(m_buffer.begin() + m_buffer.size(), buffer, buffer + bufferSize);
	//m_bufferTimestamp = std::chrono::steady_clock::now();
	//m_cond.notify_one();

	_BufferConsume();
}

void WavePlay::_BufferConsume()
{
	//for (;;) {
	//	std::unique_lock<std::mutex> lock(m_mtx);
	//	m_cond.wait(lock, [this]() { return m_buffer.size() > 0 || m_exit; });
	//	if (m_exit) {
	//		break;
	//	}
		HRESULT hr = S_FALSE;

		UINT64 position = 0;
		hr = m_spAudioClock->GetPosition(&position, nullptr);
		const uint64_t played_out_frames = m_waveformat.Format.nSamplesPerSec * position / m_device_frequency;
		auto currentTimestamp = std::chrono::steady_clock::now();

		if (m_last_played_out_frames != 0) {
			// 再生時間の差分を取得
			const uint64_t diffFrames = played_out_frames - m_last_played_out_frames;
			const uint64_t playDuration_ns = diffFrames * 1000000000 / m_waveformat.Format.nSamplesPerSec;

			// リアル時間の差分を取得
			auto diffTime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTimestamp - m_bufferTimestamp).count();

			//WARN_LOG //<< L"_BufferConsume: buffer.size() == " << buffer.size() 
			//	<< L" diffTime: " << diffTime_ns 
			//	<< L" playDuration_ns: "<< playDuration_ns;

			m_diffPlaytimeRealTime += diffTime_ns - playDuration_ns;
			//INFO_LOG << L"m_diffPlaytimeRealTime: " << m_diffPlaytimeRealTime;

			bool sampleDrop = false;
			if (m_diffPlaytimeRealTime > 50 * 1000000) {
				if (diffFrames == 0) {
					m_diffPlaytimeRealTime = 0;
					WARN_LOG << L"m_diffPlaytimeRealTime = 0;";
				} else {
					sampleDrop = true;
					WARN_LOG << L"sample drop diff:" << m_diffPlaytimeRealTime;
					m_buffer.clear();
					m_last_played_out_frames = 0;
				}
			}
		}
		m_last_played_out_frames = played_out_frames;
		m_bufferTimestamp = currentTimestamp;

		std::vector<BYTE> buffer = std::move(m_buffer);
		//auto bufferTimestamp = m_bufferTimestamp;
		//lock.unlock();


		//if (diffTime_ms >= 25) {
		//	sampleDrop = true;
		//	WARN_LOG << L"sample drop";
		//}

		//HRESULT hr = S_OK;
		UINT32 bufferFrameCount = 0;
		hr = m_spAudioClient->GetBufferSize(&bufferFrameCount);
		int count = 0;
		BYTE*	bufferBegin = buffer.data();
		size_t	restBufferSize = buffer.size();
		while (restBufferSize) {
			UINT32	numFramesAvailable = 0;
			UINT32  availableBufferSize = 0;
			for (;;) {
				// See how much buffer space is available.
				UINT32 numFramesPadding = 0;
				hr = m_spAudioClient->GetCurrentPadding(&numFramesPadding);
				numFramesAvailable = bufferFrameCount - numFramesPadding;	// 空いてるフレーム数
				availableBufferSize = m_frameSize * numFramesAvailable;		// 利用可能なバッファサイズ

				if (availableBufferSize) {
					break;	// バッファが空いた

				} else {
					::Sleep(0);	// バッファが空くまで待つ
				}
			}
			UINT32 bufferSize = min(availableBufferSize, static_cast<UINT32>(restBufferSize));

			//if (!sampleDrop) {
				UINT32 bufferFrameSize = bufferSize / m_frameSize;
				// Grab all the available space in the shared buffer.
				BYTE* pData = nullptr;
				hr = m_spRenderClient->GetBuffer(bufferFrameSize, &pData);

				::memcpy_s(pData, availableBufferSize, bufferBegin, bufferSize);
				hr = m_spRenderClient->ReleaseBuffer(bufferFrameSize, 0);

			//}
			//sampleDrop = false;

			// バッファの先頭と残りのバッファサイズを更新
			bufferBegin += bufferSize;
			restBufferSize -= bufferSize;

			++count;
		}
		//INFO_LOG << L"_BufferConsume: count == " << count;

	//}
}

