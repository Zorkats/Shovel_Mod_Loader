// d3d9_proxy.cpp - Complete version with YAML BGM database support

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include "MinHook/include/MinHook.h"
#include "compact_bitmap_font.hpp"
#include "simple_yaml_parser.hpp"
#include "game_text_hook.cpp"


#pragma comment(lib, "d3d9.lib")

// Constants
#define D3D_SDK_VERSION 32

// Function typedefs for D3D9
typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT);
typedef HRESULT (WINAPI* Direct3DCreate9Ex_t)(UINT, IDirect3D9Ex**);
typedef HRESULT (WINAPI* Direct3DShaderValidatorCreate9_t)(DWORD, void**);
typedef DWORD (WINAPI* D3DPERF_GetStatus_t)();
typedef int (WINAPI* D3DPERF_BeginEvent_t)(DWORD, LPCWSTR);
typedef int (WINAPI* D3DPERF_EndEvent_t)();
typedef void (WINAPI* D3DPERF_SetMarker_t)(DWORD, LPCWSTR);
typedef void (WINAPI* D3DPERF_SetRegion_t)(DWORD, LPCWSTR);
typedef BOOL (WINAPI* D3DPERF_QueryRepeatFrame_t)();
typedef void (WINAPI* D3DPERF_SetOptions_t)(DWORD);

// Original function pointers
static HMODULE g_hRealD3D9 = nullptr;
static Direct3DCreate9_t real_Direct3DCreate9 = nullptr;
static Direct3DCreate9Ex_t real_Direct3DCreate9Ex = nullptr;
static Direct3DShaderValidatorCreate9_t real_Direct3DShaderValidatorCreate9 = nullptr;
static D3DPERF_GetStatus_t real_D3DPERF_GetStatus = nullptr;
static D3DPERF_BeginEvent_t real_D3DPERF_BeginEvent = nullptr;
static D3DPERF_EndEvent_t real_D3DPERF_EndEvent = nullptr;
static D3DPERF_SetMarker_t real_D3DPERF_SetMarker = nullptr;
static D3DPERF_SetRegion_t real_D3DPERF_SetRegion = nullptr;
static D3DPERF_QueryRepeatFrame_t real_D3DPERF_QueryRepeatFrame = nullptr;
static D3DPERF_SetOptions_t real_D3DPERF_SetOptions = nullptr;

// Hook function typedefs
typedef HRESULT (APIENTRY* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT (APIENTRY* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT (APIENTRY* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (WINAPI* CreateDeviceEx_t)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);

// Hook storage
static EndScene_t o_EndScene = nullptr;
static Present_t o_Present = nullptr;
static Reset_t o_Reset = nullptr;
static CreateDeviceEx_t o_CreateDeviceEx = nullptr;

// Frame counter and rendering state
static int g_frameCount = 0;
static DWORD g_lastLogTime = 0;
static bool g_bSafeToRender = false;
static int g_totalFrames = 0;
static bool g_bShowMenu = false;
static bool g_bShowFPS = false;
static bool g_bShowBGMInfo = true;

// BGM Information
static uint32_t g_currentBGM = 0;
static std::string g_currentBGMName = "None";
static DWORD g_bgmStartTime = 0;
static std::map<uint32_t, SimpleYAMLParser::BGMEntry> g_bgmDatabase;
static std::set<uint32_t> g_shownBGMs;  // Track shown BGMs per session

// BGM display state
static float g_bgmAlpha = 0.0f;
static const float BGM_FADE_SPEED = 0.02f;
static const DWORD BGM_DISPLAY_TIME = 5000;

// Font rendering
static CompactBitmapFont* g_pFont = nullptr;
static CompactBitmapFont* g_pFontSmall = nullptr;

// Simple vertex structure
struct CUSTOMVERTEX {
    float x, y, z, rhw;
    DWORD color;
};

// Menu items
struct MenuItem {
    const char* name;
    bool* pValue;
    bool isToggle;
};

static bool g_bGodMode = false;
static bool g_bSpeedHack = false;
static bool g_bInfiniteGems = false;
static int g_nSelectedItem = 0;

MenuItem g_menuItems[] = {
    {"God Mode", &g_bGodMode, true},
    {"Speed Hack (2x)", &g_bSpeedHack, true},
    {"Infinite Gems", &g_bInfiniteGems, true},
    {"Show FPS", &g_bShowFPS, true},
    {"Show BGM Info", &g_bShowBGMInfo, true},
};

// Initialize fonts
void InitializeFonts() {
    if (!g_pFont) {
        g_pFont = new CompactBitmapFont(2.0f);
        g_pFontSmall = new CompactBitmapFont(1.5f);
        std::cout << "[D3D9 Proxy] Bitmap fonts initialized!" << std::endl;
    }
}

// Draw a simple colored rectangle
void DrawSimpleRect(IDirect3DDevice9* pDevice, float x, float y, float width, float height, DWORD color) {
    if (!pDevice) return;

    CUSTOMVERTEX vertices[] = {
        {x, y, 0.0f, 1.0f, color},
        {x + width, y, 0.0f, 1.0f, color},
        {x, y + height, 0.0f, 1.0f, color},
        {x + width, y + height, 0.0f, 1.0f, color}
    };

    DWORD oldFVF;
    IDirect3DBaseTexture9* oldTex;
    pDevice->GetFVF(&oldFVF);
    pDevice->GetTexture(0, &oldTex);

    pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    pDevice->SetTexture(0, NULL);
    pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));

    pDevice->SetFVF(oldFVF);
    pDevice->SetTexture(0, oldTex);
    if (oldTex) oldTex->Release();
}

// Draw a border
void DrawBorder(IDirect3DDevice9* pDevice, float x, float y, float width, float height, float thickness, DWORD color) {
    DrawSimpleRect(pDevice, x, y, width, thickness, color);
    DrawSimpleRect(pDevice, x, y + height - thickness, width, thickness, color);
    DrawSimpleRect(pDevice, x, y, thickness, height, color);
    DrawSimpleRect(pDevice, x + width - thickness, y, thickness, height, color);
}

// Draw BGM Info overlay
void DrawBGMInfo(IDirect3DDevice9* pDevice) {
    if (!g_bShowBGMInfo || !pDevice) return;

    // Update fade effect
    DWORD currentTime = GetTickCount();
    if (g_currentBGM != 0) {
        auto bgmIt = g_bgmDatabase.find(g_currentBGM);
        bool shouldShow = false;

        if (bgmIt != g_bgmDatabase.end() && bgmIt->second.isMusic) {
            if (g_shownBGMs.find(g_currentBGM) == g_shownBGMs.end()) {
                shouldShow = true;
            }
        }

        if (shouldShow) {
            if (currentTime - g_bgmStartTime < 1000) {
                g_bgmAlpha = min(g_bgmAlpha + BGM_FADE_SPEED, 1.0f);
            } else if (currentTime - g_bgmStartTime < BGM_DISPLAY_TIME) {
                g_bgmAlpha = 1.0f;
            } else {
                g_bgmAlpha = max(g_bgmAlpha - BGM_FADE_SPEED, 0.0f);
            }
        } else {
            g_bgmAlpha = max(g_bgmAlpha - BGM_FADE_SPEED * 2, 0.0f);
        }
    } else {
        g_bgmAlpha = max(g_bgmAlpha - BGM_FADE_SPEED, 0.0f);
    }

    if (g_bgmAlpha <= 0.0f) return;

    D3DVIEWPORT9 viewport;
    pDevice->GetViewport(&viewport);

    float bgmX = viewport.Width - 320.0f;
    float bgmY = viewport.Height - 100.0f;

    BYTE alpha = (BYTE)(200 * g_bgmAlpha);
    BYTE textAlpha = (BYTE)(255 * g_bgmAlpha);

    DrawSimpleRect(pDevice, bgmX, bgmY, 300.0f, 80.0f, D3DCOLOR_ARGB(alpha, 0, 0, 0));
    DrawBorder(pDevice, bgmX, bgmY, 300.0f, 80.0f, 2.0f, D3DCOLOR_ARGB(textAlpha, 100, 100, 255));

    if (g_pFontSmall && g_currentBGM != 0) {
        g_pFontSmall->DrawString(pDevice, bgmX + 10, bgmY + 5, "NOW PLAYING",
                                D3DCOLOR_ARGB(textAlpha * 150 / 255, 150, 150, 255));

        auto it = g_bgmDatabase.find(g_currentBGM);
        if (it != g_bgmDatabase.end()) {
            const auto& info = it->second;

            g_pFont->DrawString(pDevice, bgmX + 10, bgmY + 25, info.name,
                               D3DCOLOR_ARGB(textAlpha, 255, 255, 255));

            std::string composerText = "By: " + info.composer;
            g_pFontSmall->DrawString(pDevice, bgmX + 10, bgmY + 50, composerText,
                                    D3DCOLOR_ARGB(textAlpha * 200 / 255, 200, 200, 200));

            if (info.trackNumber > 0) {
                char trackText[32];
                sprintf_s(trackText, "OST Track #%d", info.trackNumber);
                g_pFontSmall->DrawString(pDevice, bgmX + 200, bgmY + 50, trackText,
                                        D3DCOLOR_ARGB(textAlpha * 200 / 255, 200, 200, 200));
            }
        }
    }
}

// Draw the mod menu
void DrawModMenu(IDirect3DDevice9* pDevice) {
    if (!pDevice || !g_bShowMenu) return;

    float menuX = 50.0f;
    float menuY = 100.0f;
    float menuWidth = 300.0f;
    float menuHeight = 250.0f;

    DrawSimpleRect(pDevice, menuX, menuY, menuWidth, menuHeight, D3DCOLOR_ARGB(220, 20, 20, 20));
    DrawBorder(pDevice, menuX, menuY, menuWidth, menuHeight, 2.0f, D3DCOLOR_ARGB(255, 255, 200, 0));
    DrawSimpleRect(pDevice, menuX, menuY, menuWidth, 30.0f, D3DCOLOR_ARGB(255, 255, 200, 0));

    if (g_pFont) {
        g_pFont->DrawString(pDevice, menuX + 10, menuY + 5, "SHOVEL KNIGHT MOD MENU", D3DCOLOR_ARGB(255, 0, 0, 0));
    }

    float itemY = menuY + 40.0f;
    int numItems = sizeof(g_menuItems) / sizeof(g_menuItems[0]);

    for (int i = 0; i < numItems; i++) {
        if (i == g_nSelectedItem) {
            DrawSimpleRect(pDevice, menuX + 5, itemY, menuWidth - 10, 25, D3DCOLOR_ARGB(100, 255, 255, 0));
        }

        if (g_pFont) {
            char itemText[256];
            if (g_menuItems[i].isToggle) {
                sprintf_s(itemText, "%s: %s", g_menuItems[i].name,
                         *g_menuItems[i].pValue ? "ON" : "OFF");
            } else {
                strcpy_s(itemText, g_menuItems[i].name);
            }

            D3DCOLOR textColor = (i == g_nSelectedItem) ?
                D3DCOLOR_ARGB(255, 255, 255, 0) :
                D3DCOLOR_ARGB(255, 255, 255, 255);

            g_pFont->DrawString(pDevice, menuX + 15, itemY + 3, itemText, textColor);
        }

        itemY += 30.0f;
    }

    if (g_pFontSmall) {
        g_pFontSmall->DrawString(pDevice, menuX + 10, menuY + menuHeight - 30,
                "UP/DOWN: Navigate | ENTER: Toggle | INSERT: Close",
                D3DCOLOR_ARGB(255, 200, 200, 200));
    }
}

// Handle input
void HandleInput() {
    static bool insertPressed = false;
    static bool upPressed = false;
    static bool downPressed = false;
    static bool enterPressed = false;

    if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
        if (!insertPressed) {
            g_bShowMenu = !g_bShowMenu;
            std::cout << "[D3D9 Proxy] Menu toggled: " << (g_bShowMenu ? "ON" : "OFF") << std::endl;
        }
        insertPressed = true;
    } else {
        insertPressed = false;
    }

    if (g_bShowMenu) {
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            if (!upPressed) {
                g_nSelectedItem--;
                int numItems = sizeof(g_menuItems) / sizeof(g_menuItems[0]);
                if (g_nSelectedItem < 0) g_nSelectedItem = numItems - 1;
            }
            upPressed = true;
        } else {
            upPressed = false;
        }

        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            if (!downPressed) {
                g_nSelectedItem++;
                int numItems = sizeof(g_menuItems) / sizeof(g_menuItems[0]);
                if (g_nSelectedItem >= numItems) g_nSelectedItem = 0;
            }
            downPressed = true;
        } else {
            downPressed = false;
        }

        if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
            if (!enterPressed) {
                if (g_menuItems[g_nSelectedItem].pValue) {
                    *g_menuItems[g_nSelectedItem].pValue = !*g_menuItems[g_nSelectedItem].pValue;
                    std::cout << "[D3D9 Proxy] " << g_menuItems[g_nSelectedItem].name
                             << ": " << (*g_menuItems[g_nSelectedItem].pValue ? "ON" : "OFF") << std::endl;
                }
            }
            enterPressed = true;
        } else {
            enterPressed = false;
        }
    }
}

// Hooked EndScene function
HRESULT APIENTRY hkEndScene(IDirect3DDevice9* pDevice) {
    g_frameCount++;
    g_totalFrames++;

    if (g_totalFrames > 480) {
        g_bSafeToRender = true;
    }

    DWORD currentTime = GetTickCount();
    if (currentTime - g_lastLogTime > 1000) {
        if (g_bShowFPS) {
            std::cout << "[D3D9 Proxy] FPS: " << g_frameCount << std::endl;
        }
        g_frameCount = 0;
        g_lastLogTime = currentTime;
    }

    if (g_bSafeToRender && pDevice) {
        if (!g_pFont) {
            InitializeFonts();
        }

        // Hook text interceptor once
        static bool textInterceptorInitialized = false;
        if (!textInterceptorInitialized) {
            HMODULE hTextInterceptor = GetModuleHandleA("d3d9_text_interceptor.dll");
            if (hTextInterceptor) {
                typedef void (*HookTextRendering_t)(IDirect3DDevice9*);
                HookTextRendering_t HookTextRendering = (HookTextRendering_t)GetProcAddress(hTextInterceptor, "HookTextRendering");
                if (HookTextRendering) {
                    HookTextRendering(pDevice);
                    std::cout << "[D3D9 Proxy] Text interceptor hooked!" << std::endl;
                }
            }
            textInterceptorInitialized = true;
        }

        HandleInput();

        DWORD oldColorWriteEnable, oldSrcBlend, oldDestBlend, oldAlphaBlendEnable;
        pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);
        pDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
        pDevice->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlendEnable);

        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

        DrawSimpleRect(pDevice, 10.0f, 10.0f, 200.0f, 25.0f, D3DCOLOR_ARGB(200, 0, 0, 0));
        DrawBorder(pDevice, 10.0f, 10.0f, 200.0f, 25.0f, 2.0f, D3DCOLOR_ARGB(255, 255, 255, 0));

        if (g_pFont) {
            g_pFont->DrawString(pDevice, 15, 12, "SK MOD - INSERT = MENU", D3DCOLOR_ARGB(255, 255, 255, 0));
        }

        if (!g_bShowMenu && g_pFont) {
            int yOffset = 40;
            if (g_bGodMode) {
                g_pFont->DrawString(pDevice, 15, yOffset, "[GOD MODE ON]", D3DCOLOR_ARGB(255, 0, 255, 0));
                yOffset += 20;
            }
            if (g_bSpeedHack) {
                g_pFont->DrawString(pDevice, 15, yOffset, "[SPEED HACK ON]", D3DCOLOR_ARGB(255, 0, 255, 0));
                yOffset += 20;
            }
            if (g_bInfiniteGems) {
                g_pFont->DrawString(pDevice, 15, yOffset, "[INFINITE GEMS ON]", D3DCOLOR_ARGB(255, 0, 255, 0));
            }
        }

        if (g_bShowFPS && g_pFontSmall) {
            char fpsText[32];
            sprintf_s(fpsText, "FPS: %d", g_frameCount);
            g_pFontSmall->DrawString(pDevice, 10, 250, fpsText, D3DCOLOR_ARGB(255, 255, 255, 0));
        }

        DrawBGMInfo(pDevice);
        DrawModMenu(pDevice);

        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWriteEnable);
        pDevice->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
        pDevice->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlendEnable);
    }

    return o_EndScene(pDevice);
}

// Other hook functions remain the same...
HRESULT APIENTRY hkPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect,
                           HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    return o_Present(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT APIENTRY hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    std::cout << "[D3D9 Proxy] Device reset called" << std::endl;
    HRESULT hr = o_Reset(pDevice, pPresentationParameters);
    return hr;
}

HRESULT WINAPI hkCreateDeviceEx(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface) {
    std::cout << "[D3D9 Proxy] CreateDeviceEx called" << std::endl;

    HRESULT hr = o_CreateDeviceEx(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                  pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        std::cout << "[D3D9 Proxy] Device created, hooking EndScene, Present and Reset" << std::endl;

        void** vtable = *(void***)(*ppReturnedDeviceInterface);

        MH_CreateHook(vtable[42], &hkEndScene, (void**)&o_EndScene);
        MH_CreateHook(vtable[17], &hkPresent, (void**)&o_Present);
        MH_CreateHook(vtable[16], &hkReset, (void**)&o_Reset);
        MH_EnableHook(MH_ALL_HOOKS);
    }

    return hr;
}

// Load real D3D9 functions
void LoadRealD3D9Functions() {
    if (g_hRealD3D9) return;

    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    wcscat_s(sysPath, L"\\d3d9.dll");
    g_hRealD3D9 = LoadLibraryW(sysPath);

    if (g_hRealD3D9) {
        real_Direct3DCreate9 = (Direct3DCreate9_t)GetProcAddress(g_hRealD3D9, "Direct3DCreate9");
        real_Direct3DCreate9Ex = (Direct3DCreate9Ex_t)GetProcAddress(g_hRealD3D9, "Direct3DCreate9Ex");
        real_Direct3DShaderValidatorCreate9 = (Direct3DShaderValidatorCreate9_t)GetProcAddress(g_hRealD3D9, "Direct3DShaderValidatorCreate9");
        real_D3DPERF_GetStatus = (D3DPERF_GetStatus_t)GetProcAddress(g_hRealD3D9, "D3DPERF_GetStatus");
        real_D3DPERF_BeginEvent = (D3DPERF_BeginEvent_t)GetProcAddress(g_hRealD3D9, "D3DPERF_BeginEvent");
        real_D3DPERF_EndEvent = (D3DPERF_EndEvent_t)GetProcAddress(g_hRealD3D9, "D3DPERF_EndEvent");
        real_D3DPERF_SetMarker = (D3DPERF_SetMarker_t)GetProcAddress(g_hRealD3D9, "D3DPERF_SetMarker");
        real_D3DPERF_SetRegion = (D3DPERF_SetRegion_t)GetProcAddress(g_hRealD3D9, "D3DPERF_SetRegion");
        real_D3DPERF_QueryRepeatFrame = (D3DPERF_QueryRepeatFrame_t)GetProcAddress(g_hRealD3D9, "D3DPERF_QueryRepeatFrame");
        real_D3DPERF_SetOptions = (D3DPERF_SetOptions_t)GetProcAddress(g_hRealD3D9, "D3DPERF_SetOptions");
    }
}

// Initialize mods
void InitializeMods() {
    auto modsDir = std::filesystem::current_path() / "mods";
    std::cout << "[D3D9 Proxy] Scanning mods in: " << modsDir.string() << std::endl;

    std::error_code ec;
    if (!std::filesystem::exists(modsDir, ec)) {
        std::cout << "[D3D9 Proxy] Mods directory doesn't exist, creating it" << std::endl;
        std::filesystem::create_directory(modsDir, ec);
        return;
    }

    for (auto& f : std::filesystem::directory_iterator(modsDir, ec)) {
        if (f.path().extension() == L".dll") {
            auto wpath = f.path().wstring();
            HMODULE h = LoadLibraryW(wpath.c_str());
            if (h) {
                std::wcout << L"[D3D9 Proxy] Loaded mod: " << wpath << L"\n";
            } else {
                std::wcout << L"[D3D9 Proxy] Failed to load mod: " << wpath << L"\n";
            }
        }
    }
}

// Load BGM database
void LoadBGMDatabase() {
    // Try to load from game directory first
    std::string yamlPath = "bgm_database.yaml";

    if (!std::filesystem::exists(yamlPath)) {
        // Try mods directory
        yamlPath = "mods/bgm_database.yaml";
    }

    if (std::filesystem::exists(yamlPath)) {
        g_bgmDatabase = SimpleYAMLParser::LoadBGMDatabase(yamlPath);
        std::cout << "[D3D9 Proxy] Loaded BGM database from " << yamlPath << std::endl;
    } else {
        std::cout << "[D3D9 Proxy] BGM database not found! Please create bgm_database.yaml" << std::endl;
    }
}

// Try to hook via dummy device
void TryHookViaDevice() {
    std::cout << "[D3D9 Proxy] Attempting to hook via dummy device..." << std::endl;

    HWND hWnd = GetDesktopWindow();

    IDirect3D9Ex* pD3DEx = nullptr;
    if (FAILED(real_Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx))) {
        std::cout << "[D3D9 Proxy] Failed to create D3D9Ex" << std::endl;
        return;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.hDeviceWindow = hWnd;

    IDirect3DDevice9Ex* pDevice = nullptr;
    HRESULT hr = pD3DEx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                        &d3dpp, NULL, &pDevice);

    if (SUCCEEDED(hr) && pDevice) {
        std::cout << "[D3D9 Proxy] Dummy device created" << std::endl;

        void** vtable = *(void***)pDevice;

        if (!o_EndScene) {
            MH_CreateHook(vtable[42], &hkEndScene, (void**)&o_EndScene);
        }
        if (!o_Present) {
            MH_CreateHook(vtable[17], &hkPresent, (void**)&o_Present);
        }
        if (!o_Reset) {
            MH_CreateHook(vtable[16], &hkReset, (void**)&o_Reset);
        }

        MH_EnableHook(MH_ALL_HOOKS);

        pDevice->Release();
    }

    pD3DEx->Release();
}

// Exported functions
extern "C" {

// BGM notification
__declspec(dllexport) void NotifyBGMPlay(uint32_t soundId) {
    auto it = g_bgmDatabase.find(soundId);

    if (it != g_bgmDatabase.end() && it->second.isMusic) {
        g_currentBGM = soundId;
        g_bgmStartTime = GetTickCount();
        g_currentBGMName = it->second.name;

        if (g_shownBGMs.find(soundId) == g_shownBGMs.end()) {
            g_shownBGMs.insert(soundId);
            g_bgmAlpha = 0.0f;

            std::cout << "[D3D9 Proxy] NEW BGM: " << g_currentBGMName
                      << " (ID: 0x" << std::hex << soundId << std::dec << ")"
                      << " [First time this session]" << std::endl;
        } else {
            std::cout << "[D3D9 Proxy] BGM: " << g_currentBGMName
                      << " (ID: 0x" << std::hex << soundId << std::dec << ")"
                      << " [Already shown]" << std::endl;
        }
    } else if (it != g_bgmDatabase.end() && !it->second.isMusic) {
        std::cout << "[D3D9 Proxy] SFX: " << it->second.name
                  << " (ID: 0x" << std::hex << soundId << std::dec << ")" << std::endl;
    } else {
        std::cout << "[D3D9 Proxy] Unknown sound ID: 0x" << std::hex << soundId << std::dec << std::endl;
    }
}

__declspec(dllexport) void NotifyBGMStop(uint32_t soundId) {
    if (g_currentBGM == soundId) {
        std::cout << "[D3D9 Proxy] BGM Stopped: " << g_currentBGMName << std::endl;
        g_currentBGM = 0;
        g_currentBGMName = "None";
    }
}

__declspec(dllexport) void ResetBGMSession() {
    g_shownBGMs.clear();
    std::cout << "[D3D9 Proxy] BGM session reset - all tracks can be shown again" << std::endl;
}

// D3D9 exports
IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVer) {
    LoadRealD3D9Functions();
    return real_Direct3DCreate9 ? real_Direct3DCreate9(sdkVer) : nullptr;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVer, IDirect3D9Ex** ppD3D) {
    std::cout << "[D3D9 Proxy] Direct3DCreate9Ex called" << std::endl;

    LoadRealD3D9Functions();

    if (!real_Direct3DCreate9Ex) return E_FAIL;

    HRESULT hr = real_Direct3DCreate9Ex(sdkVer, ppD3D);

    if (SUCCEEDED(hr) && ppD3D && *ppD3D) {
        void** vtable = *(void***)(*ppD3D);
        MH_CreateHook(vtable[20], &hkCreateDeviceEx, (void**)&o_CreateDeviceEx);
        MH_EnableHook(vtable[20]);

        TryHookViaDevice();
    }

    return hr;
}

HRESULT WINAPI Direct3DShaderValidatorCreate9(DWORD Flags, void** ppValidator) {
    LoadRealD3D9Functions();
    return real_Direct3DShaderValidatorCreate9 ? real_Direct3DShaderValidatorCreate9(Flags, ppValidator) : E_FAIL;
}

DWORD WINAPI D3DPERF_GetStatus() {
    LoadRealD3D9Functions();
    return real_D3DPERF_GetStatus ? real_D3DPERF_GetStatus() : 0;
}

int WINAPI D3DPERF_BeginEvent(DWORD color, LPCWSTR name) {
    LoadRealD3D9Functions();
    return real_D3DPERF_BeginEvent ? real_D3DPERF_BeginEvent(color, name) : 0;
}

int WINAPI D3DPERF_EndEvent() {
    LoadRealD3D9Functions();
    return real_D3DPERF_EndEvent ? real_D3DPERF_EndEvent() : 0;
}

void WINAPI D3DPERF_SetMarker(DWORD color, LPCWSTR name) {
    LoadRealD3D9Functions();
    if (real_D3DPERF_SetMarker) real_D3DPERF_SetMarker(color, name);
}

void WINAPI D3DPERF_SetRegion(DWORD color, LPCWSTR name) {
    LoadRealD3D9Functions();
    if (real_D3DPERF_SetRegion) real_D3DPERF_SetRegion(color, name);
}

BOOL WINAPI D3DPERF_QueryRepeatFrame() {
    LoadRealD3D9Functions();
    return real_D3DPERF_QueryRepeatFrame ? real_D3DPERF_QueryRepeatFrame() : FALSE;
}

void WINAPI D3DPERF_SetOptions(DWORD options) {
    LoadRealD3D9Functions();
    if (real_D3DPERF_SetOptions) real_D3DPERF_SetOptions(options);
}

void WINAPI DebugSetMute(BOOL mute) {}
void WINAPI DebugSetLevel(DWORD level) {}
void WINAPI PSGPError() {}
void WINAPI PSGPSampleTexture() {}

} // extern "C"

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        SetConsoleTitleA("Shovel Knight D3D9 Proxy");

        std::cout << "============================================" << std::endl;
        std::cout << "[D3D9 Proxy] WITH BITMAP FONT TEXT" << std::endl;
        std::cout << "[D3D9 Proxy] Using custom bitmap font renderer" << std::endl;
        std::cout << "[D3D9 Proxy] Process ID: " << GetCurrentProcessId() << std::endl;
        std::cout << "============================================" << std::endl;

        if (MH_Initialize() != MH_OK) {
            std::cout << "[D3D9 Proxy] Failed to initialize MinHook!" << std::endl;
            return FALSE;
        }

        std::cout << "[D3D9 Proxy] MinHook initialized" << std::endl;

        // Load BGM database
        LoadBGMDatabase();

        InitializeMods();

        std::cout << "[D3D9 Proxy] Ready!" << std::endl;
        std::cout << "============================================" << std::endl;
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_pFont) {
            delete g_pFont;
            g_pFont = nullptr;
        }
        if (g_pFontSmall) {
            delete g_pFontSmall;
            g_pFontSmall = nullptr;
        }

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    return TRUE;
}