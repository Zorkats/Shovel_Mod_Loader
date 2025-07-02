// d3d9ex_hook_clean.cpp - Clean D3D9Ex Hook for Shovel Knight
#include <windows.h>
#include <d3d9.h>
#include <iostream>
#include <string>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

// Function type definitions
typedef HRESULT(APIENTRY* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(APIENTRY* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(APIENTRY* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

// Hook storage
static EndScene_t o_EndScene = nullptr;
static Present_t o_Present = nullptr;
static Reset_t o_Reset = nullptr;

// State variables
static bool g_bInitialized = false;
static bool g_bShowMenu = true;
static HWND g_hWindow = nullptr;
static IDirect3DDevice9* g_pd3dDevice = nullptr;
static WNDPROC o_WndProc = nullptr;

// Forward declarations
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Logging
void Log(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);

    std::cout << "[D3D9Ex Hook] " << buffer << std::endl;
    OutputDebugStringA(("[D3D9Ex Hook] " + std::string(buffer) + "\n").c_str());
}

// Initialize ImGui
void InitImGui(IDirect3DDevice9* pDevice) {
    if (g_bInitialized) return;

    Log("InitImGui called with device: %p", pDevice);

    // Get device parameters
    D3DDEVICE_CREATION_PARAMETERS params;
    if (FAILED(pDevice->GetCreationParameters(&params))) {
        Log("Failed to get creation parameters");
        return;
    }

    g_hWindow = params.hFocusWindow;
    if (!g_hWindow) {
        // Fallback: try to find the window
        g_hWindow = FindWindowA(nullptr, "Shovel Knight: Treasure Trove");
        if (!g_hWindow) {
            g_hWindow = FindWindowA("SDL_app", nullptr);  // Try SDL window class
        }
    }

    if (!g_hWindow) {
        Log("Failed to find game window");
        return;
    }

    Log("Found window: %p", g_hWindow);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    if (!ImGui_ImplWin32_Init(g_hWindow)) {
        Log("Failed to initialize Win32 backend");
        return;
    }

    if (!ImGui_ImplDX9_Init(pDevice)) {
        Log("Failed to initialize DX9 backend");
        ImGui_ImplWin32_Shutdown();
        return;
    }

    // Hook window procedure
    o_WndProc = (WNDPROC)SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);

    g_pd3dDevice = pDevice;
    g_bInitialized = true;
    Log("ImGui initialized successfully!");
}

// Render ImGui
void RenderImGui() {
    if (!g_bInitialized) return;

    // Start the Dear ImGui frame
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Draw menu
    if (g_bShowMenu) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Shovel Knight Mod Menu", &g_bShowMenu);

        ImGui::Text("D3D9Ex Hook Active!");
        ImGui::Separator();

        ImGui::Text("Device: %p", g_pd3dDevice);
        ImGui::Text("Window: %p", g_hWindow);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Cheats")) {
            static bool godMode = false;
            static bool infiniteGems = false;

            ImGui::Checkbox("God Mode", &godMode);
            ImGui::Checkbox("Infinite Gems", &infiniteGems);

            if (ImGui::Button("Test Button")) {
                Log("Button clicked!");
            }
        }

        ImGui::Separator();
        ImGui::Text("Press INSERT to toggle menu");

        ImGui::End();
    }

    // Render
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

// Hooked functions
HRESULT APIENTRY hkEndScene(IDirect3DDevice9* pDevice) {
    static bool once = false;
    if (!once) {
        once = true;
        Log("EndScene hooked and called!");
    }

    if (!g_bInitialized && pDevice) {
        InitImGui(pDevice);
    }

    if (g_bInitialized) {
        RenderImGui();
    }

    return o_EndScene(pDevice);
}

HRESULT APIENTRY hkPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    static bool once = false;
    if (!once) {
        once = true;
        Log("Present hooked and called!");
    }

    if (!g_bInitialized && pDevice) {
        InitImGui(pDevice);
    }

    if (g_bInitialized) {
        RenderImGui();
    }

    return o_Present(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT APIENTRY hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    Log("Device Reset called");

    if (g_bInitialized) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }

    HRESULT hr = o_Reset(pDevice, pPresentationParameters);

    if (SUCCEEDED(hr) && g_bInitialized) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    return hr;
}

// Window procedure
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_bShowMenu = !g_bShowMenu;
        Log("Menu toggled: %s", g_bShowMenu ? "ON" : "OFF");
        return 0;
    }

    if (g_bShowMenu && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    return CallWindowProc(o_WndProc, hWnd, msg, wParam, lParam);
}

// Get D3D9 device vtable
bool GetD3D9DeviceVTable(void** pVTable, size_t size) {
    if (!pVTable) return false;

    // Create dummy window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "DX", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow(wc.lpszClassName, NULL, WS_OVERLAPPEDWINDOW,
                            100, 100, 300, 300, GetDesktopWindow(), NULL,
                            wc.hInstance, NULL);
    if (!hWnd) {
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    // Try D3D9Ex first (what ANGLE uses)
    IDirect3D9Ex* pD3DEx = nullptr;
    IDirect3DDevice9Ex* pDeviceEx = nullptr;

    if (SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx))) {
        D3DPRESENT_PARAMETERS d3dpp = {};
        d3dpp.Windowed = TRUE;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
        d3dpp.hDeviceWindow = hWnd;

        if (SUCCEEDED(pD3DEx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                                            &d3dpp, NULL, &pDeviceEx))) {
            memcpy(pVTable, *(void***)pDeviceEx, size);
            pDeviceEx->Release();
            pD3DEx->Release();
            DestroyWindow(hWnd);
            UnregisterClass(wc.lpszClassName, wc.hInstance);
            return true;
        }
        pD3DEx->Release();
    }

    // Fallback to regular D3D9
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) {
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    IDirect3DDevice9* pDevice = nullptr;
    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.hDeviceWindow = hWnd;

    if (SUCCEEDED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                                    &d3dpp, &pDevice))) {
        memcpy(pVTable, *(void***)pDevice, size);
        pDevice->Release();
    }

    pD3D->Release();
    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return true;
}

// Main thread
DWORD WINAPI MainThread(LPVOID lpParam) {
    // Wait a bit for game to initialize
    Sleep(100);

    // Allocate console
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Shovel Knight D3D9Ex Hook");

    Log("=== D3D9Ex Hook Starting ===");

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        Log("Failed to initialize MinHook");
        return 0;
    }

    // Wait for D3D9
    Log("Waiting for d3d9.dll...");
    while (!GetModuleHandleA("d3d9.dll")) {
        Sleep(50);
    }
    Log("d3d9.dll loaded");

    // Get D3D9 device vtable
    void* d3d9_device_vtable[119];
    if (!GetD3D9DeviceVTable(d3d9_device_vtable, sizeof(d3d9_device_vtable))) {
        Log("Failed to get D3D9 device vtable");
        return 0;
    }

    Log("Got D3D9 device vtable");

    // Hook EndScene (index 42)
    if (MH_CreateHook(d3d9_device_vtable[42], &hkEndScene, (void**)&o_EndScene) != MH_OK) {
        Log("Failed to create EndScene hook");
        return 0;
    }

    // Hook Present (index 17)
    if (MH_CreateHook(d3d9_device_vtable[17], &hkPresent, (void**)&o_Present) != MH_OK) {
        Log("Failed to create Present hook");
        return 0;
    }

    // Hook Reset (index 16)
    if (MH_CreateHook(d3d9_device_vtable[16], &hkReset, (void**)&o_Reset) != MH_OK) {
        Log("Failed to create Reset hook");
        return 0;
    }

    // Enable hooks
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        Log("Failed to enable hooks");
        return 0;
    }

    Log("Hooks installed successfully!");
    Log("Waiting for game to render...");

    // Keep thread alive
    while (true) {
        Sleep(100);

        // Check if we should unhook
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
    }

    // Cleanup
    Log("Shutting down...");
    if (g_bInitialized) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}