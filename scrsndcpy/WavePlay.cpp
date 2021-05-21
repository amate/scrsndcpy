#include "stdafx.h"
#include "WavePlay.h"

#include <avrt.h>

#include "MainDlg.h"
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

bool WavePlay::Init(int bufferMultiple, int maxBufferSampleCount)
{
	ATLASSERT(1 <= bufferMultiple && bufferMultiple <= 10);
	ATLASSERT(0 <= maxBufferSampleCount && bufferMultiple <= 48000);
	m_maxBufferSampleCount = maxBufferSampleCount;

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
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
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

		//hr = m_spAudioClock->GetFrequency(&m_device_frequency);
		//if (FAILED(hr)) {
		//	throw std::runtime_error("m_spAudioClock->GetFrequency failed");
		//}

		// WASAPI情報取得
		UINT32 frame = 0;
		hr = m_spAudioClient->GetBufferSize(&frame);
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->GetBufferSize failed");
		}

		const UINT32 bufferSize = frame * iFrameSize;
		m_bufferBytes = bufferSize / 2;	// とりあえず最大バッファの半分のサイズを要求する
		//m_bufferBytes = bufferSize;	// とりあえず最大バッファサイズを要求する
		m_bufferBytes *= bufferMultiple;
		INFO_LOG << L"bufferSize: " << m_bufferBytes;

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

		// イベントオブジェクトを設定
		m_eventBufferReady.Create(NULL, FALSE, FALSE, NULL);
		hr = m_spAudioClient->SetEventHandle(m_eventBufferReady);
		ATLASSERT(SUCCEEDED(hr));

		// SimpleAudioVolume取得
		hr = m_spAudioClient->GetService(__uuidof(ISimpleAudioVolume), (void**)&m_spSimpleAudioVolume);
		ATLASSERT(SUCCEEDED(hr));

		//// 優先度を Audio に設定
		//DWORD taskIndex = 0;
		//m_hTask = ::AvSetMmThreadCharacteristics(TEXT("Audio"), &taskIndex);
		//ATLASSERT(m_hTask);

		// 再生開始
		hr = m_spAudioClient->Start();
		if (FAILED(hr)) {
			throw std::runtime_error("m_spAudioClient->Start() failed");
		}

		m_threadBufferConsume = std::thread([this]() {
			// 優先度を Audio に設定
			DWORD taskIndex = 0;
			m_hTask = ::AvSetMmThreadCharacteristics(TEXT("Audio"), &taskIndex);
			ATLASSERT(m_hTask);

			_BufferConsume();

			// 優先度を元に戻す
			if (m_hTask != NULL) {
				::AvRevertMmThreadCharacteristics(m_hTask);
				m_hTask = NULL;
			}
		});
	} catch (std::exception& e) {
		ERROR_LOG << L"WavePlay::Init failed: " << UTF16fromUTF8(e.what());
		ATLASSERT(FALSE);
		return false;
	}

	return true;
}

void WavePlay::Term()
{
	//std::unique_lock<std::mutex> lock(m_mtx);
	m_exit = true;
	//lock.unlock();

	//// 優先度を元に戻す
	//if (m_hTask != NULL) {
	//	::AvRevertMmThreadCharacteristics(m_hTask);
	//	m_hTask = NULL;
	//}
	if (m_spAudioClient) {
		m_threadBufferConsume.join();

		m_spSimpleAudioVolume.Release();
		m_spAudioClock.Release();
		m_spRenderClient.Release();

		HRESULT hr = S_FALSE;
		hr = m_spAudioClient->Stop();
		m_spAudioClient.Release();

	}
}


void WavePlay::WriteBuffer(const BYTE* buffer, int bufferSize)
{
	//std::unique_lock<std::mutex> lock(m_mtx);
	CCritSecLock lock(m_cs);
	m_buffer.append((const char*)buffer, bufferSize);
	//m_buffer.insert(m_buffer.begin() + m_buffer.size(), buffer, buffer + bufferSize);
	//m_bufferTimestamp = std::chrono::steady_clock::now();
	//m_cond.notify_one();

	//_BufferConsume();
}

void WavePlay::SetVolume(int volume)
{
	ATLASSERT(0 <= volume && volume <= 100);
	float fLevel = volume / 100.0f;
	if (fLevel < 0.0) {
		fLevel = 0;
	} else if (1.0 < fLevel) {
		fLevel = 1.0;
	}
	HRESULT hr = m_spSimpleAudioVolume->SetMasterVolume(fLevel, nullptr);
}

void WavePlay::_BufferConsume()
{
	HRESULT hr = S_FALSE;

	UINT32 bufferFrameCount = 0;
	hr = m_spAudioClient->GetBufferSize(&bufferFrameCount);
	ATLASSERT(SUCCEEDED(hr));

	if (m_maxBufferSampleCount < static_cast<int>(bufferFrameCount)) {
		m_maxBufferSampleCount = static_cast<int>(bufferFrameCount);
	}

	// 最初に最大までバッファを溜める
	const size_t maxBufferSize = (m_maxBufferSampleCount + bufferFrameCount)  * m_frameSize;
	while (!m_exit) {
		CCritSecLock lock(m_cs);
		if (maxBufferSize <= m_buffer.size()) {
			break;
		}
		lock.Unlock();
		::Sleep(0);
	}

	int i = 0;
	int minSample = INT_MAX;
	int maxSample = 0;
	int adjustBufferCount = 0;
	int playSample = 0;
	while (!m_exit) {
		UINT32	numFramesAvailable = 0;
		UINT32  availableBufferSize = 0;

		for (;;) {
			// Renader バッファが空くのを待つ
			// Wait for next buffer event to be signaled.
			DWORD retval = ::WaitForSingleObject(m_eventBufferReady, 2000);
			if (retval != WAIT_OBJECT_0) {
				if (m_exit) {
					return ;	// cancel
				}
				// Event handle timed out after a 2-second wait.
				ERROR_LOG << L"WaitForSingleObject timeout";
				ATLASSERT(FALSE);
				return;
			}
			// See how much buffer space is available.
			UINT32 numFramesPadding = 0;
			hr = m_spAudioClient->GetCurrentPadding(&numFramesPadding);
			//ATLASSERT(SUCCEEDED(hr));
			numFramesAvailable = bufferFrameCount - numFramesPadding;	// 空いてるフレーム数
			availableBufferSize = m_frameSize * numFramesAvailable;		// 利用可能なバッファサイズ

			if (availableBufferSize) {
				break;	// バッファが空いた

			} else {
				//ATLASSERT(FALSE);	// 今の実装だとここには来ない・・・はずだったが来る
				WARN_LOG << L"availableBufferSize == 0";
				::Sleep(0);	// バッファが空くまで待つ
			}
		}

		size_t bufferSampleCount = 0;
		{	// Render バッファに書き込む
			//std::unique_lock<std::mutex> lock(m_mtx);
			CCritSecLock lock(m_cs);

			const char* bufferBegin = m_buffer.data();
			size_t		restBufferSize = m_buffer.size();

			UINT32 bufferSize = min(availableBufferSize, static_cast<UINT32>(restBufferSize));

			UINT32 bufferFrameSize = bufferSize / m_frameSize;
			// Grab all the available space in the shared buffer.
			BYTE* pData = nullptr;
			hr = m_spRenderClient->GetBuffer(bufferFrameSize, &pData);
			ATLASSERT(SUCCEEDED(hr));

			::memcpy_s(pData, availableBufferSize, bufferBegin, bufferSize);
			hr = m_spRenderClient->ReleaseBuffer(bufferFrameSize, 0);
			ATLASSERT(SUCCEEDED(hr));

			playSample += bufferFrameSize;

			{
				//if (::GetAsyncKeyState(VK_CONTROL) < 0 && ::GetAsyncKeyState(VK_MENU) < 0) {
				//	m_buffer.clear();
				//}
				bufferSampleCount = (restBufferSize - bufferSize) / m_frameSize;

				minSample = min(minSample, static_cast<int>(bufferSampleCount));
				maxSample = max(maxSample, static_cast<int>(bufferSampleCount));

				// バッファ調整
				const int kMaxBufferSample = m_maxBufferSampleCount;//bufferFrameCount * 2;
				if (kForceBufferClearSampleCount < maxSample) {
					WARN_LOG << L"m_buffer.clear() - kForceBufferClearSampleCount < maxSample";
					m_buffer.clear();

				} else if (kMaxBufferSample < maxSample) {
					bufferSize += m_frameSize * 1;	// 1 sample分余計に削除する
					//INFO_LOG << L"buff erase, maxSample:" << maxSample;
					++adjustBufferCount;
					--maxSample;
				}
			}

			// 書き込んだ分削除する
			m_buffer.erase(0, bufferSize);
		}
		++i;

		using namespace std::chrono;
		static auto prevTime = steady_clock::now();
		auto nowTime = steady_clock::now();
		auto elapsed = duration_cast<milliseconds>(nowTime - prevTime).count();
		if (elapsed >= 1000) {
#ifdef _DEBUG
			//INFO_LOG << L"bufferSampleCount: " << bufferSampleCount << L" \ti: " << i  << L" playSample: " << playSample
			//		<< L" min: " << minSample << L" \tmax: " << maxSample << L" \tadjustBufferCount: " << adjustBufferCount;
#endif
			::PostMessage(m_hWndMainDlg, CMainDlg::WM_WAVEPlAY_INFO, MAKELONG(playSample, adjustBufferCount), MAKELONG(minSample, maxSample));

			prevTime = nowTime;
			i = 0;
			minSample = INT_MAX;
			maxSample = 0;
			adjustBufferCount = 0;
			playSample = 0;
		}
	}
#if 0
	for (;;) {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cond.wait(lock, [this]() { return m_buffer.size() > 0 || m_exit; });
		if (m_exit) {
			break;
		}

		std::string buffer;
		buffer.swap(m_buffer);
		//auto bufferTimestamp = m_bufferTimestamp;
		lock.unlock();


		//if (diffTime_ms >= 25) {
		//	sampleDrop = true;
		//	WARN_LOG << L"sample drop";
		//}

		//HRESULT hr = S_OK;
		HRESULT hr = S_FALSE;

		UINT32 bufferFrameCount = 0;
		hr = m_spAudioClient->GetBufferSize(&bufferFrameCount);
		int count = 0;
		const char*	bufferBegin = buffer.data();
		size_t	restBufferSize = buffer.size();
		while (restBufferSize) {
			UINT32	numFramesAvailable = 0;
			UINT32  availableBufferSize = 0;

			//auto timebegin = std::chrono::steady_clock::now();
			for (;;) {
				// Wait for next buffer event to be signaled.
				DWORD retval = ::WaitForSingleObject(m_eventBufferReady, 2000);
				if (retval != WAIT_OBJECT_0) {
					// Event handle timed out after a 2-second wait.
					ERROR_LOG << L"WaitForSingleObject timeout";
					ATLASSERT(FALSE);
					return;
				}

				// See how much buffer space is available.
				UINT32 numFramesPadding = 0;
				hr = m_spAudioClient->GetCurrentPadding(&numFramesPadding);
				//ATLASSERT(SUCCEEDED(hr));
				numFramesAvailable = bufferFrameCount - numFramesPadding;	// 空いてるフレーム数
				availableBufferSize = m_frameSize * numFramesAvailable;		// 利用可能なバッファサイズ

				{

					UINT64 position = 0;
					hr = m_spAudioClock->GetPosition(&position, nullptr);
					const uint64_t played_out_frames = m_waveformat.Format.nSamplesPerSec * position / m_device_frequency;

					auto currentTimestamp = std::chrono::steady_clock::now();
					// リアル時間の差分を取得
					auto diffTime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTimestamp - m_bufferTimestamp).count();
					m_bufferTimestamp = currentTimestamp;

					if (m_last_played_out_frames != 0) {
						// 再生時間の差分を取得
						const uint64_t diffFrames = played_out_frames - m_last_played_out_frames;
						const uint64_t playDuration_ns = diffFrames * 1000000000 / m_waveformat.Format.nSamplesPerSec;

						//WARN_LOG //<< L"_BufferConsume: buffer.size() == " << buffer.size() 
						//	<< L" diffTime: " << diffTime_ns 
						//	<< L" playDuration_ns: "<< playDuration_ns;

						m_diffPlaytimeRealTime += diffTime_ns - playDuration_ns;
						//INFO_LOG << L"m_diffPlaytimeRealTime: " << m_diffPlaytimeRealTime;

						bool sampleDrop = false;
						if (std::abs(m_diffPlaytimeRealTime) > m_playTimeRealTimeThreshold_ms * 1000000LL) {
							if (diffFrames == 0) {	// 前回再生されなかったのでリセットしておく
								m_diffPlaytimeRealTime = 0;
								//WARN_LOG << L"m_diffPlaytimeRealTime = 0;";
							} else {
								sampleDrop = true;
								WARN_LOG << L"sample drop diff:" << m_diffPlaytimeRealTime;

								// メインダイアログに通知する
								std::thread([](int64_t diffPlaytimeRealTime, uint64_t playDuration_ns, uint64_t realDiffTime_ns) {	
									PUTLOG(L"sample drop\r\ndiffPlaytimeRealTime: %ld", //\r\nplayDuration_ns\t: %ld\r\nrealDiffTime_ns\t: %ld", 
										diffPlaytimeRealTime);// , playDuration_ns, realDiffTime_ns);
								}, m_diffPlaytimeRealTime, playDuration_ns, static_cast<uint64_t>(diffTime_ns)).detach();
								//m_buffer.clear();	// 関数開始時に空にされるので要らない
								m_last_played_out_frames = 0;
								m_diffPlaytimeRealTime = 0;

								// ソケット内のバッファが空になるまで読みだす
								m_pSock->SetBlocking(false);
								//buffer.clear();
								auto tempbuffer = std::make_unique<char[]>(m_bufferBytes);
								int totalRecvSize = 0;
								for (;;) {
									int recvSize = m_pSock->Read(tempbuffer.get(), m_bufferBytes);
									ATLASSERT(recvSize != -1);
									if (recvSize == -1) {
										ERROR_LOG << L"SocketError : -1";										
										return;
									}
									buffer.append(tempbuffer.get(), recvSize);

									totalRecvSize += recvSize;
									if (recvSize == 0) {
										break;
									}
								}
								
								// bufferの後ろから availableBufferSize 分利用する
								restBufferSize = min(buffer.size(), availableBufferSize);
								bufferBegin = buffer.data() + (totalRecvSize - restBufferSize);
								INFO_LOG << L"totalRecvSize: " << totalRecvSize << L" availableBufferSize: " << availableBufferSize << L" restBufferSize: " << restBufferSize;

								m_pSock->SetBlocking(true);
								break;	// バッファが用意できたので処理を回す
							}
						}
					}
					m_last_played_out_frames = played_out_frames;
				}

				if (availableBufferSize) {
					break;	// バッファが空いた

				} else {
					//ATLASSERT(FALSE);	// 今の実装だとここには来ない・・・はずだったが来る
					WARN_LOG << L"availableBufferSize == 0";
					::Sleep(0);	// バッファが空くまで待つ
				}
			}
			//auto sleeptime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - timebegin).count();
			//INFO_LOG << L"sleeptime: " << sleeptime << L"ns";

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

	}
#endif
}

