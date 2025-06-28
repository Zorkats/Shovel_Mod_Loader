// d3d9_proxy.cpp
// --------------------------
// A drop-in D3D9 proxy + mod loader for Shovel Knight.
// Place the resulting d3d9.dll next to ShovelKnight.exe,
// create a "mods" subfolder, and your .dll mods will auto-load.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "MinHook.h"
#include <filesystem>
#include <iostream>
#include <string>

// Minimal D3D9 declarations to avoid conflicts
typedef void* LPDIRECT3D9;
typedef void* LPDIRECT3DDEVICE9;

// Constants we need
#define D3D_SDK_VERSION 32

// -----------------------------------------------------------------------------
// typedefs for all D3D9 functions we need to proxy
typedef LPDIRECT3D9 (WINAPI* PFN_Direct3DCreate9)(UINT);
typedef DWORD       (WINAPI* PFN_D3DPERF_GetStatus)();
typedef int         (WINAPI* PFN_D3DPERF_BeginEvent)(DWORD, LPCWSTR);
typedef int         (WINAPI* PFN_D3DPERF_EndEvent)();
typedef void        (WINAPI* PFN_D3DPERF_SetMarker)(DWORD, LPCWSTR);
typedef void        (WINAPI* PFN_D3DPERF_SetRegion)(DWORD, LPCWSTR);
typedef BOOL        (WINAPI* PFN_D3DPERF_QueryRepeatFrame)();
typedef void        (WINAPI* PFN_D3DPERF_SetOptions)(DWORD);

// Storage for real function pointers
static HMODULE              real_d3d9_dll               = nullptr;
static PFN_Direct3DCreate9  real_Direct3DCreate9        = nullptr;
static PFN_D3DPERF_GetStatus      real_D3DPERF_GetStatus      = nullptr;
static PFN_D3DPERF_BeginEvent     real_D3DPERF_BeginEvent     = nullptr;
static PFN_D3DPERF_EndEvent       real_D3DPERF_EndEvent       = nullptr;
static PFN_D3DPERF_SetMarker      real_D3DPERF_SetMarker      = nullptr;
static PFN_D3DPERF_SetRegion      real_D3DPERF_SetRegion      = nullptr;
static PFN_D3DPERF_QueryRepeatFrame real_D3DPERF_QueryRepeatFrame = nullptr;
static PFN_D3DPERF_SetOptions     real_D3DPERF_SetOptions     = nullptr;

// -----------------------------------------------------------------------------
// 6 missing exports that Shovel Knight expects:
typedef HRESULT (WINAPI* PFN_Direct3DCreate9Ex)(UINT, void**);
static PFN_Direct3DCreate9Ex real_Direct3DCreate9Ex = nullptr;

typedef HRESULT (WINAPI* PFN_Direct3DShaderValidatorCreate9)(DWORD, void**);
static PFN_Direct3DShaderValidatorCreate9 real_Direct3DShaderValidatorCreate9 = nullptr;

// Stubbed PERF debug calls & PSGP calls
extern "C" __declspec(dllexport) void    WINAPI DebugSetMute       (BOOL   mute)      { /*no-op*/ }
extern "C" __declspec(dllexport) void    WINAPI DebugSetLevel      (DWORD  level)     { /*no-op*/ }
extern "C" __declspec(dllexport) void    WINAPI PSGPError          (void)             { /*no-op*/ }
extern "C" __declspec(dllexport) void    WINAPI PSGPSampleTexture  (void)             { /*no-op*/ }

// -----------------------------------------------------------------------------
// storage for the original Present function
static void* origPresent = nullptr;

// once-only flag
static bool g_initialized = false;

// our hook for IDirect3DDevice9::Present
HRESULT WINAPI hkPresent(void* device, void* src, void* dst, void* hwnd, void* dr)
{
    typedef HRESULT (WINAPI* Present_t)(void*, void*, void*, void*, void*);
    auto original = reinterpret_cast<Present_t>(origPresent);
    return original(device, src, dst, hwnd, dr);
}

// initialize console, MinHook, hook Present, load mods
void InitializeMods(void* device)
{
    if (g_initialized) return;
    g_initialized = true;

    // 1) bring up a console
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Shovel Knight Mod Loader");
    std::cout << "[ModLoader] Console allocated\n";

    // 2) init MinHook
    if (MH_Initialize() != MH_OK) {
        std::cout << "[ModLoader] MH_Initialize failed\n";
        return;
    }
    std::cout << "[ModLoader] MinHook initialized\n";

    // 4) load every DLL in "mods" folder
    auto modsDir = std::filesystem::current_path() / "mods";
    std::cout << "[ModLoader] Scanning mods in: " << modsDir.string() << "\n";

    std::error_code ec;
    if (!std::filesystem::exists(modsDir, ec)) {
        std::cout << "[ModLoader] Mods directory doesn't exist, creating it\n";
        std::filesystem::create_directory(modsDir, ec);
        return;
    }

    for (auto& f : std::filesystem::directory_iterator(modsDir, ec)) {
        if (f.path().extension() == L".dll") {
            auto wpath = f.path().wstring();
            HMODULE h = LoadLibraryW(wpath.c_str());
            if (h) {
                std::wcout << L"[ModLoader] Loaded mod: " << wpath << L"\n";
            } else {
                std::wcout << L"[ModLoader] Failed to load mod: " << wpath << L"\n";
            }
        }
    }
}

// Load all real D3D9 functions
void LoadRealD3D9Functions()
{
    if (real_d3d9_dll) return; // Already loaded

    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    wcscat_s(sysPath, L"\\d3d9.dll");
    real_d3d9_dll = LoadLibraryW(sysPath);

    if (real_d3d9_dll) {
        real_Direct3DCreate9       = reinterpret_cast<PFN_Direct3DCreate9>(       GetProcAddress(real_d3d9_dll, "Direct3DCreate9"));
        real_D3DPERF_GetStatus     = reinterpret_cast<PFN_D3DPERF_GetStatus>(     GetProcAddress(real_d3d9_dll, "D3DPERF_GetStatus"));
        real_D3DPERF_BeginEvent    = reinterpret_cast<PFN_D3DPERF_BeginEvent>(    GetProcAddress(real_d3d9_dll, "D3DPERF_BeginEvent"));
        real_D3DPERF_EndEvent      = reinterpret_cast<PFN_D3DPERF_EndEvent>(      GetProcAddress(real_d3d9_dll, "D3DPERF_EndEvent"));
        real_D3DPERF_SetMarker     = reinterpret_cast<PFN_D3DPERF_SetMarker>(     GetProcAddress(real_d3d9_dll, "D3DPERF_SetMarker"));
        real_D3DPERF_SetRegion     = reinterpret_cast<PFN_D3DPERF_SetRegion>(     GetProcAddress(real_d3d9_dll, "D3DPERF_SetRegion"));
        real_D3DPERF_QueryRepeatFrame = reinterpret_cast<PFN_D3DPERF_QueryRepeatFrame>(GetProcAddress(real_d3d9_dll, "D3DPERF_QueryRepeatFrame"));
        real_D3DPERF_SetOptions    = reinterpret_cast<PFN_D3DPERF_SetOptions>(    GetProcAddress(real_d3d9_dll, "D3DPERF_SetOptions"));
    }
}

// -----------------------------------------------------------------------------
// Exported D3D9 functions (as before)

// Direct3DCreate9
extern "C" __declspec(dllexport)
LPDIRECT3D9 WINAPI Direct3DCreate9(UINT sdkVer)
{
    LoadRealD3D9Functions();
    return real_Direct3DCreate9
        ? real_Direct3DCreate9(sdkVer)
        : nullptr;
}

// D3DPERF exports
extern "C" __declspec(dllexport)
DWORD WINAPI D3DPERF_GetStatus()
{
    LoadRealD3D9Functions();
    return real_D3DPERF_GetStatus
        ? real_D3DPERF_GetStatus()
        : 0;
}

extern "C" __declspec(dllexport)
int WINAPI D3DPERF_BeginEvent(DWORD color, LPCWSTR name)
{
    LoadRealD3D9Functions();
    return real_D3DPERF_BeginEvent
        ? real_D3DPERF_BeginEvent(color, name)
        : 0;
}

extern "C" __declspec(dllexport)
int WINAPI D3DPERF_EndEvent()
{
    LoadRealD3D9Functions();
    return real_D3DPERF_EndEvent
        ? real_D3DPERF_EndEvent()
        : 0;
}

extern "C" __declspec(dllexport)
void WINAPI D3DPERF_SetMarker(DWORD color, LPCWSTR name)
{
    LoadRealD3D9Functions();
    if (real_D3DPERF_SetMarker) real_D3DPERF_SetMarker(color, name);
}

extern "C" __declspec(dllexport)
void WINAPI D3DPERF_SetRegion(DWORD color, LPCWSTR name)
{
    LoadRealD3D9Functions();
    if (real_D3DPERF_SetRegion) real_D3DPERF_SetRegion(color, name);
}

extern "C" __declspec(dllexport)
BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
    LoadRealD3D9Functions();
    return real_D3DPERF_QueryRepeatFrame
        ? real_D3DPERF_QueryRepeatFrame()
        : FALSE;
}

extern "C" __declspec(dllexport)
void WINAPI D3DPERF_SetOptions(DWORD options)
{
    LoadRealD3D9Functions();
    if (real_D3DPERF_SetOptions) real_D3DPERF_SetOptions(options);
}

// -----------------------------------------------------------------------------
// Now the two new forwards:

extern "C" __declspec(dllexport)
HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVer, void** ppDeviceEx)
{
    LoadRealD3D9Functions();
    if (!real_Direct3DCreate9Ex) {
        real_Direct3DCreate9Ex = reinterpret_cast<PFN_Direct3DCreate9Ex>(
            GetProcAddress(real_d3d9_dll, "Direct3DCreate9Ex"));
    }
    return real_Direct3DCreate9Ex
        ? real_Direct3DCreate9Ex(sdkVer, ppDeviceEx)
        : E_FAIL;
}

extern "C" __declspec(dllexport)
HRESULT WINAPI Direct3DShaderValidatorCreate9(DWORD Flags, void** ppValidator)
{
    LoadRealD3D9Functions();
    if (!real_Direct3DShaderValidatorCreate9) {
        real_Direct3DShaderValidatorCreate9 = reinterpret_cast<PFN_Direct3DShaderValidatorCreate9>(
            GetProcAddress(real_d3d9_dll, "Direct3DShaderValidatorCreate9"));
    }
    return real_Direct3DShaderValidatorCreate9
        ? real_Direct3DShaderValidatorCreate9(Flags, ppValidator)
        : E_FAIL;
}

// -----------------------------------------------------------------------------
// Thread entry: waits a bit and then forces our InitializeMods
DWORD WINAPI InitThread(LPVOID)
{
    Sleep(1000);  // Wait for game to set up D3D9
    // Create a dummy D3D9 object to pull in vtables, then discard
    auto d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d) {
        // Get vtable pointer
        void** vtbl = *reinterpret_cast<void***>(d3d);
        // Release the dummy object
        typedef ULONG (WINAPI* Release_t)(void*);
        auto rel = reinterpret_cast<Release_t>(vtbl[2]);
        rel(d3d);
        // Now initialize hooks + mods using the device pointer
        InitializeMods(vtbl);
    }
    return 0;
}

// DllMain - start initialization in a separate thread
BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}