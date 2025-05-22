// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "stdafx.h"

#include <assert.h>
#include <chrono>
#include <queue>
#include <string>
#include <thread>

#include <boost\filesystem.hpp>
#include <unordered_map>
#include <Windows.h>
#include <boost\optional.hpp>

#include "MinHook.h"
#include "SharedMemoryData.h"
#include "SDL_events.h"

#include "Socket.h"
#include "WavePlay.h"
#include "VolumeControl.h"

#pragma comment(lib, "Imm32.lib")

using namespace std::chrono;
namespace fs = boost::filesystem;

constexpr LPCSTR kavutil_dllNameA = "avutil-59.dll";
constexpr LPCWSTR kavutil_dllNameW = L"avutil-59.dll";

// 前方宣言
struct AVFrame;

// グローバル変数
HMODULE g_hModule;

// 遅延用のフレームが入ってる
std::queue<AVFrame*>   g_delayFrameQue;
uint32_t    g_delayFrameCount = 0;
SharedMemoryData*    g_sharedMemoryData;

bool	g_jpLocale = false;

std::unique_ptr<CVolumeControl>	g_pVolumeControl;


#define PUTLOG	PutLog

void PutLog(LPCWSTR pstrFormat, ...)
{
	CString* pLog = new CString;
	CString& strText = *pLog;
	{
		va_list args;
		va_start(args, pstrFormat);

		strText.FormatV(pstrFormat, args);

		va_end(args);

		//INFO_LOG << (LPCWSTR)strText;
	}
	//strText += _T("\n");
	COPYDATASTRUCT cds = {};
	cds.dwData = kPutLog;
	cds.lpData = (PVOID)(LPCWSTR)strText;
	cds.cbData = sizeof(WCHAR) * (strText.GetLength() + 1);
	::SendMessage(g_sharedMemoryData->hwndMainDlg, WM_COPYDATA, NULL, (LPARAM)&cds);
}



////////////////////////////////////////////////////////////////

class StreamingSoundPlay
{
public:

	~StreamingSoundPlay()
	{
		if (m_threadSoundStreeming.joinable()) {
			m_cancelSoundStreaming = true;
			m_threadSoundStreeming.join();
		}
	}

    void    ConnectAndPlay()
    {
		m_cancelSoundStreaming = false;
		m_threadSoundStreeming = std::thread([this]()
			{
				// sndcpy起動
				BOOL ret = ::SendMessage(g_sharedMemoryData->hwndMainDlg, WM_DELAYFRAME_COMMAND, kStartSndcpy, 0);
				ATLASSERT(ret);
				
				::Sleep(500);	// 初回接続までは少し待機する必要がある

				CSocket sock;
				auto funcConnect = [&, this]() -> bool {
					IPAddress addr;
					addr.Set("127.0.0.1", "28200");
					std::atomic_bool valid = true;
					enum { kMaxConnectRetryCount = 15 };
					for (int i = 0; i < kMaxConnectRetryCount; ++i) {

						if (m_cancelSoundStreaming) {
							break;		// cancel
						}

						if (!sock.Connect(addr, valid)) {
							::Sleep(500);
						} else {
							sock.SetBlocking(true);
							char temp[64] = "";
							int recvSize = sock.Read(temp, 64);
							if (recvSize == 0) {
								PUTLOG(L"Connect retry: %d", i);
								::Sleep(500);
								continue;	// retry
							}
							break;
						}
					}
					if (!sock.IsConnected()) {
						PUTLOG(L"接続に失敗しました...");
						//funcSSCButtonEnable();
						return false;		// failed

					} else {
						PUTLOG(L"接続成功");
						return true;	// success
					}
				};
				if (!funcConnect()) {
					return;
				}

				auto func_wavPlayInit = [this]() {
					m_wavePlay = std::make_unique<WavePlay>();
					m_wavePlay->SetMainDlgHWND(g_sharedMemoryData->hwndMainDlg);
					m_wavePlay->Init(g_sharedMemoryData->bufferMultiple, g_sharedMemoryData->maxBufferSampleCount);
					SetVolume(g_sharedMemoryData->playSoundVolume);
				};

				int reconnectCount = 0;
				auto funcReConnect = [&, this]() -> bool {

					enum { kMaxReconnectRetryCount = 3 };
					for (int i = 0; i < kMaxReconnectRetryCount; ++i) {
						++reconnectCount;
						PUTLOG(L"ReConnect: %d", reconnectCount);
						BOOL ret = ::SendMessage(g_sharedMemoryData->hwndMainDlg, WM_DELAYFRAME_COMMAND, kRestartSndcpy, 0);
						ATLASSERT(ret);
						//std::string outret = _SendADBCommand(L"shell am start com.rom1v.sndcpy/.MainActivity --activity-clear-top"); // お試し --activity-clear-top

						bool success = funcConnect();
						if (success) {
							return true;
						}
					}

					return false;
				};

				using namespace std::chrono;
				auto prevTime = steady_clock::now();

				auto funcMutePlayStop = [&](const char* buffer, int recvSize) -> bool {
					//enum { kMaxCount = 32 };
					//int i64size = min(recvSize / 8, kMaxCount);
					int i64size = recvSize / 8;
					const std::int64_t* bufferBegin = reinterpret_cast<const std::int64_t*>(buffer);
					//Utility::timer timer;
					bool bMute = std::all_of(bufferBegin, bufferBegin + i64size, [](std::int64_t c) { return c == 0; });
					//bool bMute = std::all_of(buffer, buffer + recvSize, [](char c) { return c == 0; });
					//INFO_LOG << L"all_of: " << timer.format();
					if (bMute) {
						if (!m_wavePlay) {
							return true;	// 再生ストップ
						}

						// 一定時間ミュートが続く、かつ、画面がオフの時に再生を止める
						enum { kDisplayOffConfirmMinutes = 1 };
						using namespace std::chrono;
						auto nowTime = steady_clock::now();
						auto elapsed = duration_cast<minutes>(nowTime - prevTime).count();
						if (kDisplayOffConfirmMinutes <= elapsed) {
							prevTime = nowTime;

							BOOL ret = ::SendMessage(g_sharedMemoryData->hwndMainDlg, WM_DELAYFRAME_COMMAND, kWakefulness, 0);
							if (ret) {
								m_wavePlay.reset();
								return true;	// 再生ストップ
							}
						}
					} else {
						// 音が出ている
						if (!m_wavePlay) {
							// 再生を開始する
							func_wavPlayInit();
						}
					}
					return false;	// 再生を止めない
				};

				func_wavPlayInit();

				sock.SetBlocking(true);
				const int bufferSize = m_wavePlay->GetBufferBytes();
				auto buffer = std::make_unique<char[]>(bufferSize);

				// 最初に送られてくる音声データを破棄する (最新のデータを取得するため)
				enum { kDataDiscardSec = 1 };
				auto timebegin = std::chrono::steady_clock::now();
				while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - timebegin).count() < kDataDiscardSec) {
					int recvSize = sock.Read(buffer.get(), bufferSize);
				}
				PUTLOG(L"音声のストリーミング再生を開始します");
				//funcSSCButtonEnable();
				g_sharedMemoryData->nowSoundStreaming = true;

				while (!m_cancelSoundStreaming) {
					int recvSize = sock.Read(buffer.get(), bufferSize);
					if (recvSize == 0 && sock.IsConnected()) {
						continue;
					}
					if (recvSize == -1) {
						PUTLOG(L"Socket Error - audio streaming finish");

						if (funcReConnect()) {
							continue;

						} else {
							break;
						}
					}
					if (funcMutePlayStop(buffer.get(), recvSize)) {
						continue;	// 音を再生しない
					}
		#if 0
					if (::GetAsyncKeyState(VK_MENU) < 0 && ::GetAsyncKeyState(VK_SHIFT)) {
						continue;	// drain
					}
		#endif
					m_wavePlay->WriteBuffer((const BYTE*)buffer.get(), recvSize);
				}
				m_wavePlay.reset();

				g_sharedMemoryData->nowSoundStreaming = false;
			}
		);
    }

	int		GetVolume()
	{
		return m_nowVolume;
	}

	void	SetVolume(int volume)
	{
		if (m_wavePlay) {
			m_nowVolume = volume;
			m_wavePlay->SetVolume(volume);
		}
	}

private:
    std::thread	m_threadSoundStreeming;
    std::atomic_bool	m_cancelSoundStreaming = false;
    std::unique_ptr<WavePlay>	m_wavePlay;
	int	m_nowVolume;
};

std::unique_ptr<StreamingSoundPlay>	g_streamingSoundPlay;



// =============================================================

using Func_av_frame_clone = AVFrame* (*)(const AVFrame* src);
Func_av_frame_clone g_func_av_frame_clone;

using Func_av_frame_ref = int (*)(AVFrame* dst, const AVFrame* src);
Func_av_frame_ref   g_func_av_frame_ref;

using Func_av_frame_free = void (*)(AVFrame** frame);
Func_av_frame_free  g_func_av_frame_free;

using Func_av_frame_unref = void (*)(AVFrame* frame);
Func_av_frame_unref g_func_av_frame_unref;

// ====================================

using Func_av_frame_move_ref = void (*)(AVFrame* dst, AVFrame* src);
Func_av_frame_move_ref  g_org_av_frame_move_ref;

void 	Replaced_av_frame_move_ref(AVFrame* dst, AVFrame* src)
{
#if 0
    static int i = 0;
    ++i;
    static auto prevTime = steady_clock::now();
    auto nowTime = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(nowTime - prevTime).count();
    if (elapsed >= 1000) {
        std::wstring log = L"i: " + std::to_wstring(i) + L"\n";
        OutputDebugStringW(log.c_str());

        prevTime = nowTime;
        i = 0;
    }
#endif
    if (g_delayFrameCount == 0) {   // 0 ならオリジナル関数を呼び出すだけにする
        g_org_av_frame_move_ref(dst, src);

    } else {
        AVFrame* cloneFrame = g_func_av_frame_clone(src); // copyFrame
        g_delayFrameQue.push(cloneFrame);

        AVFrame* srcFrame = g_delayFrameQue.front();

        if (g_delayFrameCount < g_delayFrameQue.size()) {
            // delay queからあふれた分は破棄されてもいい
            g_delayFrameQue.pop();
            g_org_av_frame_move_ref(dst, srcFrame);

            g_func_av_frame_free(&srcFrame);    // av_frame_cloneしたものは av_frame_freeしなければならない

        } else {
            // queが空いてるならコピーする 
            g_func_av_frame_ref(dst, srcFrame);     // srcFrame は破棄されない
        }

        //g_func_av_frame_free(&src);
        g_func_av_frame_unref(src);
    }
}

// ===========================================


using Func_SDL_WaitEvent = int (SDLCALL*)(SDL_Event* event);
Func_SDL_WaitEvent  g_org_SDL_WaitEvent;

int 	Replaced_SDL_WaitEvent(SDL_Event* event)
{
    int ret = g_org_SDL_WaitEvent(event);
#if 0
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_g) {
            OutputDebugStringW(L"g\n");
        }
    }
#endif

	// sndcpy
	if (g_sharedMemoryData->streamingReady) {
		PUTLOG(L"streamingReady");
		g_streamingSoundPlay = std::make_unique<StreamingSoundPlay>();
		g_streamingSoundPlay->ConnectAndPlay();
		g_sharedMemoryData->streamingReady = false;
	}
	if (g_streamingSoundPlay) {
		const int volume = g_streamingSoundPlay->GetVolume();
		if (volume != g_sharedMemoryData->playSoundVolume) {
			g_streamingSoundPlay->SetVolume(g_sharedMemoryData->playSoundVolume);
		}
	}

	// scrcpy内臓
	if (g_sharedMemoryData->simpleAudioReady) {
		g_pVolumeControl = std::make_unique<CVolumeControl>();
		bool success = g_pVolumeControl->InitVolumeControl();
		PUTLOG(L"InitVolumeControl : %s", success ? L"success!" : L"failed...");
		if (success) {
			g_pVolumeControl->SetVolume(g_sharedMemoryData->playSoundVolume);
		}
		g_sharedMemoryData->simpleAudioReady = false;
	}
	if (g_pVolumeControl) {
		const int volume = g_pVolumeControl->GetVolume();
		if (volume != g_sharedMemoryData->playSoundVolume) {
			g_pVolumeControl->SetVolume(g_sharedMemoryData->playSoundVolume);
		}
	}

    if (event->type == SDL_MOUSEMOTION) {
        if (event->motion.x == 0 && event->motion.y == 0) {
            if (g_sharedMemoryData->doEventFlag) {
                g_sharedMemoryData->doEventFlag = false;
                // Ctrl + g : ウィンドウをオリジナルサイズへリサイズさせる
                event->type = SDL_KEYDOWN;
                event->key.keysym.scancode = SDL_SCANCODE_G;
                event->key.keysym.mod = KMOD_LCTRL;
                event->key.keysym.sym = SDLK_g;
                OutputDebugStringW(L"Replaced_SDL_WaitEvent: Ctrl+G\n");
            }
        }
	} else if (event->type == SDL_KEYDOWN) {
		if (event->key.keysym.scancode == SDL_SCANCODE_GRAVE) {
			if (g_jpLocale) {
				OutputDebugStringW(L"Replaced_SDL_WaitEvent: 半角/全角\n");
				BOOL ret = ::PostMessage(g_sharedMemoryData->hwndMainDlg, WM_DELAYFRAME_COMMAND, kHankakuZenkaku, 0);
			}
		} else if (event->key.keysym.scancode == SDL_SCANCODE_RETURN) {
			if (event->key.keysym.mod & KMOD_CTRL)	{
				OutputDebugStringW(L"Replaced_SDL_WaitEvent: Ctrl+Enter\n");
				BOOL ret = ::PostMessage(g_sharedMemoryData->hwndMainDlg, WM_DELAYFRAME_COMMAND, kCtrlEnter, 0);
			}
		}
	}


    return ret;
}

// ===========================================

/// 現在実行中の exeのあるフォルダのパスを返す
fs::path GetExeDirectory()
{
    WCHAR exePath[MAX_PATH] = L"";
    GetModuleFileName(NULL, exePath, MAX_PATH);
    fs::path exeFolder = exePath;
    return exeFolder.parent_path();
}


// for Logger
std::string	LogFileName()
{
    return (GetExeDirectory() / L"info_delayFrame.txt").string();
}

void    Init()
{
    {
        HMODULE hDll = ::LoadLibraryA(kavutil_dllNameA);
        assert(hDll);

        g_func_av_frame_clone = (Func_av_frame_clone)::GetProcAddress(hDll, "av_frame_clone");
        assert(g_func_av_frame_clone);

        g_func_av_frame_ref = (Func_av_frame_ref)::GetProcAddress(hDll, "av_frame_ref");
        assert(g_func_av_frame_ref);

        g_func_av_frame_free = (Func_av_frame_free)::GetProcAddress(hDll, "av_frame_free");
        assert(g_func_av_frame_free);

        g_func_av_frame_unref = (Func_av_frame_unref)::GetProcAddress(hDll, "av_frame_unref");
        assert(g_func_av_frame_unref);
    }
    {
        HMODULE hDll = ::LoadLibraryA("SDL2.dll");
        assert(hDll);

        g_org_SDL_WaitEvent = (Func_SDL_WaitEvent)::GetProcAddress(hDll, "SDL_WaitEvent");
        assert(g_org_SDL_WaitEvent);
    }

    // Hook
    MH_Initialize();
    auto ret = MH_CreateHookApi(kavutil_dllNameW, "av_frame_move_ref", (LPVOID)&Replaced_av_frame_move_ref, (LPVOID*)&g_org_av_frame_move_ref);
    bool success = ret == MH_OK;

    auto ret2 = MH_CreateHookApi(L"SDL2.dll", "SDL_WaitEvent", (LPVOID)&Replaced_SDL_WaitEvent, (LPVOID*)&g_org_SDL_WaitEvent);

    MH_EnableHook(MH_ALL_HOOKS);
    OutputDebugStringW(success ? L"Hook OK\n" : L"Hook failed...\n");

    // 設定読み込み
    const DWORD processID = ::GetCurrentProcessId();
    std::wstring sharedMemName = L"delayFrame" + std::to_wstring(processID);
    HANDLE hFilemap = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, sharedMemName.c_str());
    assert(hFilemap);

    g_sharedMemoryData = (SharedMemoryData*)::MapViewOfFile(hFilemap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedMemoryData));
    assert(g_sharedMemoryData);
    g_delayFrameCount = g_sharedMemoryData->delayFrameCount;
    OutputDebugStringW((std::wstring(L"delayFrameCount: " + std::to_wstring(g_delayFrameCount) + L"\n").c_str()));

	// IME Disable
	BOOL ret3 = ImmDisableIME(-1);
	OutputDebugStringW((std::wstring(L"ImmDisableIME: " + std::to_wstring(ret3) + L"\n").c_str()));

	WCHAR defaultLocaleName[LOCALE_NAME_MAX_LENGTH + 1] = L"";
	GetUserDefaultLocaleName(defaultLocaleName, LOCALE_NAME_MAX_LENGTH);
	OutputDebugStringW((std::wstring(L"GetUserDefaultLocaleName: " + std::wstring(defaultLocaleName) + L"\n").c_str()));

	if (::_wcsicmp(defaultLocaleName, L"ja-JP") == 0) {
		g_jpLocale = true;
	}

    //::UnmapViewOfFile((LPCVOID)pDelayFrameCount);
    //::CloseHandle(hFilemap);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hModule = hModule;

        Init();
    }
    break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

