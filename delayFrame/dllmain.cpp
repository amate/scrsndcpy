// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"

#include <assert.h>
#include <chrono>
#include <queue>
#include <string>

#include "MinHook.h"
#include "SharedMemoryData.h"
#include "SDL_events.h"

using namespace std::chrono;

constexpr LPCSTR kavutil_dllNameA = "avutil-57.dll";
constexpr LPCWSTR kavutil_dllNameW = L"avutil-57.dll";

// 前方宣言
struct AVFrame;

// グローバル変数
HMODULE g_hModule;

// 遅延用のフレームが入ってる
std::queue<AVFrame*>   g_delayFrameQue;
uint32_t    g_delayFrameCount = 0;
SharedMemoryData*    g_sharedMemoryData;

#define CHKPROC_API __declspec(dllexport)


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
    }
    return ret;
}

// ===========================================

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

