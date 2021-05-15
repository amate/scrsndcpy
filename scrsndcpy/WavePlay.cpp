#include "stdafx.h"
#include "WavePlay.h"
#pragma comment(lib, "Winmm.lib")



#pragma comment(lib, "Avrt.lib")


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
#if 0
	WAVEFORMATEX wfe = {};
	wfe.wFormatTag = WAVE_FORMAT_PCM;
	wfe.nChannels = 2;    //ステレオ
	wfe.wBitsPerSample = 16;    //量子化ビット数
	wfe.nBlockAlign = wfe.nChannels * wfe.wBitsPerSample / 8;
	wfe.nSamplesPerSec = 48000;    //標本化周波数
	wfe.nAvgBytesPerSec = wfe.nSamplesPerSec * wfe.nBlockAlign;

	m_bufferBytes = wfe.nAvgBytesPerSec / 1000 * buffer_ms;
	ATLASSERT(m_bufferBytes > 0);

	auto ret = ::waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &wfe, 0, 0, CALLBACK_NULL);
	ATLASSERT(ret == MMSYSERR_NOERROR);
	WAVEHDR wh = {};

	for (auto& wh : m_wh) {
		wh = WAVEHDR();	

		//ret = ::waveOutPrepareHeader(m_hWaveOut, &wh, sizeof(WAVEHDR));
		//ATLASSERT(ret == MMSYSERR_NOERROR);
		//ret = ::waveOutWrite(m_hWaveOut, &wh, sizeof(WAVEHDR));
		//ATLASSERT(ret == MMSYSERR_NOERROR);
	}
	for (auto& buffer : m_buffer) {
		buffer.resize(m_bufferBytes);
	}
#endif

	HRESULT ret;
	CComPtr<IMMDeviceEnumerator> spDeviceEnumrator;

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

	ret = spDeviceEnumrator.CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL);
	// マルチメディアデバイス列挙子
	//ret = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&spDeviceEnumrator);
	if (FAILED(ret)) {
		DEBUG("CLSID_MMDeviceEnumeratorエラー¥n");
		return FALSE;
	}

	// デフォルトのデバイスを選択
	CComPtr<IMMDevice> spDevice;
	ret = spDeviceEnumrator->GetDefaultAudioEndpoint(eRender, eConsole, &spDevice);
	if (FAILED(ret)) {
		DEBUG("GetDefaultAudioEndpointエラー¥n");
		return FALSE;
	}

	// オーディオクライアント
	//CComPtr<IAudioClient> spAudioClient;
	ret = spDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&spAudioClient);
	if (FAILED(ret)) {
		DEBUG("オーディオクライアント取得失敗¥n");
		return FALSE;
	}

	// 2. Setting the audio client properties – note that low latency offload is not supported
	AudioClientProperties audioProps = { 0 };
	audioProps.cbSize = sizeof(AudioClientProperties);
	audioProps.eCategory = AudioCategory_Media;
	//ret = spAudioClient->SetClientProperties(&audioProps);

	// フォーマットの構築
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

	format_ = wf;

	// 1サンプルのサイズを保存(16bitステレオなら4byte)
	auto iFrameSize = wf.Format.nBlockAlign;
	m_frameSize = iFrameSize;

	// フォーマットのサポートチェック
	//ret = spAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&wf, NULL);
	//if (FAILED(ret)) {
	//	DEBUG("未サポートのフォーマット¥n");
	//	return FALSE;
	//}
	WAVEFORMATEX* pwfx = NULL;
	ret = spAudioClient->GetMixFormat(&pwfx);

	::CoTaskMemFree(pwfx);

	// 3. Querying the legal periods
	WAVEFORMATEX* mixFormat = NULL;
	ret = spAudioClient->GetMixFormat(&mixFormat);

	UINT32 defaultPeriodInFrames = 0;
	UINT32 fundamentalPeriodInFrames = 0;
	UINT32 minPeriodInFrames = 0;
	UINT32 maxPeriodInFrames;
	ret = spAudioClient->GetSharedModeEnginePeriod(&wf.Format, &defaultPeriodInFrames, &fundamentalPeriodInFrames, &minPeriodInFrames, &maxPeriodInFrames);

	// legal periods are any multiple of fundamentalPeriodInFrames between
	// minPeriodInFrames and maxPeriodInFrames, inclusive
	// the Windows shared-mode engine uses defaultPeriodInFrames unless an audio client // has specifically requested otherwise
	
	// 4. Initializing a low-latency client
	//ret = spAudioClient->InitializeSharedAudioStream(
	//	0,
	//	minPeriodInFrames,
	//	mixFormat,
	//	nullptr); // audio session GUID


// 5. Initializing a client with a specific format (if the format needs to be different than the default format)

	//AudioClientProperties audioProps = { 0 };
	audioProps.cbSize = sizeof(AudioClientProperties);
	audioProps.eCategory = AudioCategory_Media;
	audioProps.Options |= AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;

	ret = spAudioClient->SetClientProperties(&audioProps);

	WAVEFORMATEX* closest;
	ret = spAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wf.Format, &closest);
	if (S_OK == ret) {
		/* device supports the app format */
	} else if (S_FALSE == ret) {
		/* device DOES NOT support the app format; closest supported format is in the "closest" output variable */
	} else {
		/* device DOES NOT support the app format, and Windows could not find a close supported format */
	}

	ret = spAudioClient->InitializeSharedAudioStream(
		0,
		defaultPeriodInFrames,
		&wf.Format,
		nullptr); // audio session GUID
	if (AUDCLNT_E_ENGINE_FORMAT_LOCKED == ret) {
		/* engine is already running at a different format */
	} else if (FAILED(ret)) {
		//...
	}
#if 0
	// レイテンシ設定
	REFERENCE_TIME default_device_period = 0;
	REFERENCE_TIME minimum_device_period = 0;
	int latency = 0;
	if (latency != 0) {
		default_device_period = (REFERENCE_TIME)latency * 10000LL;      // デフォルトデバイスピリオドとしてセット
		DEBUG("レイテンシ指定             : %I64d (%fミリ秒)¥n", default_device_period, default_device_period / 10000.0);
	} else {
		ret = spAudioClient->GetDevicePeriod(&default_device_period, &minimum_device_period);
		DEBUG("デフォルトデバイスピリオド : %I64d (%fミリ秒)¥n", default_device_period, default_device_period / 10000.0);
		DEBUG("最小デバイスピリオド       : %I64d (%fミリ秒)¥n", minimum_device_period, minimum_device_period / 10000.0);

		default_device_period *= 3;
	}

	// 初期化
	UINT32 frame = 0;
	ret = spAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		0,
		default_device_period,              // デフォルトデバイスピリオド値をセット
		default_device_period,              // デフォルトデバイスピリオド値をセット
		(WAVEFORMATEX*)&wf,
		NULL);
	if (FAILED(ret)) {
		if (ret == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
			DEBUG("バッファサイズアライメントエラーのため修正する¥n");

			// 修正後のフレーム数を取得
			ret = spAudioClient->GetBufferSize(&frame);
			DEBUG("修正後のフレーム数         : %d¥n", frame);
			default_device_period = (REFERENCE_TIME)(10000.0 *                     // (REFERENCE_TIME(100ns) / ms) *
				1000 *                        // (ms / s) *
				frame /                       // frames /
				wf.Format.nSamplesPerSec +    // (frames / s)
				0.5);                         // 四捨五入？
			DEBUG("修正後のレイテンシ         : %I64d (%fミリ秒)¥n", default_device_period, default_device_period / 10000.0);

			// 一度破棄してオーディオクライアントを再生成
			spAudioClient.Release();
			ret = spDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&spAudioClient);
			if (FAILED(ret)) {
				DEBUG("オーディオクライアント再取得失敗¥n");
				return FALSE;
			}

			// 再挑戦
			ret = spAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
				0,
				default_device_period,
				default_device_period,
				(WAVEFORMATEX*)&wf,
				NULL);
		}

		if (FAILED(ret)) {
			DEBUG("初期化失敗 : 0x%08X¥n", ret);
			return FALSE;
		}
	}
#endif
	//CComPtr<IAudioRenderClient> spRenderClient;
	// レンダラーの取得
	ret = spAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&spRenderClient);
	if (FAILED(ret)) {
		DEBUG("レンダラー取得エラー¥n");
		return FALSE;
	}

	// WASAPI情報取得
	UINT32 frame = 0;
	ret = spAudioClient->GetBufferSize(&frame);
	DEBUG("設定されたフレーム数       : %d¥n", frame);

	UINT32 size = frame * iFrameSize;
	DEBUG("設定されたバッファサイズ   : %dbyte¥n", size);
	DEBUG("1サンプルの時間            : %f秒¥n", (float)size / wf.Format.nSamplesPerSec);

	m_bufferBytes = size;	// とりあえず最大バッファの半分を要求する

	// バッファに溜まっていたデータを初期化しておく
	LPBYTE pData = nullptr;
	ret = spRenderClient->GetBuffer(frame, &pData);
	if (SUCCEEDED(ret)) {
		spRenderClient->ReleaseBuffer(frame, AUDCLNT_BUFFERFLAGS_SILENT);
	}

	REFERENCE_TIME nsLatency = 0;
	ret = spAudioClient->GetStreamLatency(&nsLatency);
	INFO_LOG << L"GetStreamLatency: " << nsLatency << L"ns";

	ret = spAudioClient->Start();

	m_threadBufferConsume = std::thread([this]() {
		//_BufferConsume();
	});

	return true;
}

void WavePlay::Term()
{
	std::unique_lock<std::mutex> lock(m_mtx);
	m_exit = true;
	lock.unlock();

	m_threadBufferConsume.join();
#if 0
	if (!m_hWaveOut) {
		return;
	}

	::waveOutReset(m_hWaveOut);
	for (auto& wh : m_wh) {
		::waveOutUnprepareHeader(m_hWaveOut, &wh, sizeof(WAVEHDR));
	}
	::waveOutClose(m_hWaveOut);
#endif
}

void WavePlay::WriteBuffer(const BYTE* buffer, int bufferSize)
{
	std::unique_lock<std::mutex> lock(m_mtx);
	m_buffer.insert(m_buffer.begin() + m_buffer.size(), buffer, buffer + bufferSize);
	//m_bufferTimestamp = std::chrono::steady_clock::now();
	//m_cond.notify_one();

	_BufferConsume();

#if 0
	HRESULT hr = S_OK;

	UINT32 bufferFrameCount = 0;
	hr = spAudioClient->GetBufferSize(&bufferFrameCount);

	UINT32	numFramesAvailable = 0;
	UINT32  availableBufferSize = 0;
	for (;;) {
		// See how much buffer space is available.
		UINT32 numFramesPadding = 0;
		hr = spAudioClient->GetCurrentPadding(&numFramesPadding);
		numFramesAvailable = bufferFrameCount - numFramesPadding;	// 空いてるフレーム数
		availableBufferSize = m_frameSize * numFramesAvailable;		// 利用可能なバッファサイズ

		if (bufferSize <= availableBufferSize) {
			break;	// バッファが空いた

		} else {
			::Sleep(0);	// バッファが空くまで待つ
		}
	}

	UINT32 bufferFrameSize = bufferSize / m_frameSize;
	// Grab all the available space in the shared buffer.
	BYTE* pData = nullptr;
	hr = spRenderClient->GetBuffer(bufferFrameSize, &pData);

	::memcpy_s(pData, availableBufferSize, buffer, bufferSize);
	hr = spRenderClient->ReleaseBuffer(bufferFrameSize, 0);
#endif
#if 0
	// バッファの再生が終わるのを待つ
	while (::waveOutUnprepareHeader(m_hWaveOut, &m_wh[m_bufferIndex], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
		Sleep(0);
	}	

	::memcpy_s(m_buffer[m_bufferIndex].data(), m_buffer[m_bufferIndex].size(), buffer, bufferSize);
	m_wh[m_bufferIndex].lpData = (LPSTR)m_buffer[m_bufferIndex].data();
	m_wh[m_bufferIndex].dwBufferLength = bufferSize;

	::waveOutPrepareHeader(m_hWaveOut, &m_wh[m_bufferIndex], sizeof(WAVEHDR));
	::waveOutWrite(m_hWaveOut, &m_wh[m_bufferIndex], sizeof(WAVEHDR));

	m_bufferIndex++;
	if (m_bufferIndex == 2) {
		m_bufferIndex = 0;
	}
#endif
}

void WavePlay::_BufferConsume()
{
	//for (;;) {
		//std::unique_lock<std::mutex> lock(m_mtx);
		//m_cond.wait(lock, [this]() { return m_buffer.size() > 0 || m_exit; });
		//if (m_exit) {
		//	break;
		//}
		HRESULT hr = S_FALSE;
		CComPtr<IAudioClock> audio_clock_;
		hr = spAudioClient->GetService(__uuidof(IAudioClock), (void**)&audio_clock_);
	
		UINT64 device_frequency;
		hr = audio_clock_->GetFrequency(&device_frequency);


		// Derive the audio delay which corresponds to the delay between
// a render event and the time when the first audio sample in a
// packet is played out through the speaker. This delay value
// can typically be utilized by an acoustic echo-control (AEC)
// unit at the render side.
		UINT64 position = 0;
		UINT64 qpc_position = 0;
		hr = audio_clock_->GetPosition(&position, &qpc_position);

		const uint64_t played_out_frames =
			format_.Format.nSamplesPerSec * position / device_frequency;

		if (m_last_played_out_frames != 0) {
			const uint64_t diffFrames = played_out_frames - m_last_played_out_frames;
			const uint64_t playDuration_ns = diffFrames * 1000000000 / format_.Format.nSamplesPerSec;

			auto currentTimestamp = std::chrono::steady_clock::now();
			auto diffTime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTimestamp - m_bufferTimestamp).count();
			m_bufferTimestamp = currentTimestamp;

			//WARN_LOG //<< L"_BufferConsume: buffer.size() == " << buffer.size() 
			//	<< L" diffTime: " << diffTime_ns 
			//	<< L" playDuration_ns: "<< playDuration_ns;

			m_diffPlaytimeRealTime += diffTime_ns - playDuration_ns;
			//WARN_LOG << L"m_diffPlaytimeRealTime: " << m_diffPlaytimeRealTime;

			bool sampleDrop = false;
			if (m_diffPlaytimeRealTime > 50 * 1000000) {
				if (diffFrames == 0) {
					m_diffPlaytimeRealTime = 0;
				} else {
					sampleDrop = true;
					WARN_LOG << L"sample drop diff:" << m_diffPlaytimeRealTime;
					m_buffer.clear();
				}
			}
		}
		m_last_played_out_frames = played_out_frames;

		std::vector<BYTE> buffer = std::move(m_buffer);
		auto bufferTimestamp = m_bufferTimestamp;
		//lock.unlock();


		//if (diffTime_ms >= 25) {
		//	sampleDrop = true;
		//	WARN_LOG << L"sample drop";
		//}

		//HRESULT hr = S_OK;
		UINT32 bufferFrameCount = 0;
		hr = spAudioClient->GetBufferSize(&bufferFrameCount);
		int count = 0;
		while (buffer.size()) {
			UINT32	numFramesAvailable = 0;
			UINT32  availableBufferSize = 0;
			for (;;) {
				// See how much buffer space is available.
				UINT32 numFramesPadding = 0;
				hr = spAudioClient->GetCurrentPadding(&numFramesPadding);
				numFramesAvailable = bufferFrameCount - numFramesPadding;	// 空いてるフレーム数
				availableBufferSize = m_frameSize * numFramesAvailable;		// 利用可能なバッファサイズ

				if (availableBufferSize) {
					break;	// バッファが空いた

				} else {
					::Sleep(0);	// バッファが空くまで待つ
				}
			}
			UINT32 bufferSize = min(availableBufferSize, static_cast<UINT32>(buffer.size()));

			//if (!sampleDrop) {
				UINT32 bufferFrameSize = bufferSize / m_frameSize;
				// Grab all the available space in the shared buffer.
				BYTE* pData = nullptr;
				hr = spRenderClient->GetBuffer(bufferFrameSize, &pData);

				::memcpy_s(pData, availableBufferSize, buffer.data(), bufferSize);
				hr = spRenderClient->ReleaseBuffer(bufferFrameSize, 0);

			//}
			//sampleDrop = false;

			buffer.erase(buffer.begin(), buffer.begin() + bufferSize);
			++count;
		}
		//INFO_LOG << L"_BufferConsume: count == " << count;

	//}
}

