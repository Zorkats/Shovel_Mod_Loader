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
#include "enhanced_bgm_system.hpp"
#include "mod_manager.hpp"
#include "debug_console.hpp"

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
static bool g_bPermanentBGM = false;

// Enhanced systems
static BGMSystem* g_pBGMSystem = nullptr;
static ModManager* g_pModManager = nullptr;


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
    std::function<void()> action;
};

static int g_nSelectedItem = 0;

// Menu items (initialized after ModManager is created)
static std::vector<MenuItem> g_menuItems;

// Initialize menu items
void InitializeMenuItems() {
    g_menuItems.clear();
    g_menuItems.push_back({"God Mode", g_pModManager->GetCheatEnabled("God Mode"), true, nullptr});
    g_menuItems.push_back({"Speed Hack (2x)", g_pModManager->GetCheatEnabled("Speed Hack"), true, nullptr});
    g_menuItems.push_back({"Infinite Gems", g_pModManager->GetCheatEnabled("Infinite Gems"), true, nullptr});
    g_menuItems.push_back({"Show FPS", &g_bShowFPS, true, nullptr});
    g_menuItems.push_back({"Show BGM Info", &g_bShowBGMInfo, true, nullptr});
    g_menuItems.push_back({"Permanent BGM Display", &g_bPermanentBGM, true, nullptr});
    g_menuItems.push_back({"Open Mod Manager", nullptr, false, []() { g_pModManager->ShowUI(); }});
    g_menuItems.push_back({"Show Debug Console", nullptr, false, []() { g_pDebugConsole->Show(); }});
}

// Initialize fonts
void InitializeFonts() {
    if (!g_pFont) {
        g_pFont = new CompactBitmapFont(2.0f);
        g_pFontSmall = new CompactBitmapFont(1.5f);
        LOG_INFO("D3D9", "Bitmap fonts initialized!");
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

// Enhanced BGM Info overlay
void DrawBGMInfo(IDirect3DDevice9* pDevice) {
    if (!g_bShowBGMInfo || !pDevice || !g_pBGMSystem) return;

    // Ensure fonts are initialized before drawing
    if (!g_pFont || !g_pFontSmall) {
        InitializeFonts();
        if (!g_pFont || !g_pFontSmall) return; // Still not initialized, skip drawing
    }

    // Get current track info
    const BGMSystem::TrackInfo* currentTrack = g_pBGMSystem->GetCurrentTrack();

    if (currentTrack) {
        DWORD currentTime = GetTickCount();
        auto playTime = g_pBGMSystem->GetCurrentTrackTime();

        // Fade logic
        if (g_bPermanentBGM) {
            g_bgmAlpha = 1.0f;
        } else {
            if (playTime.count() < 1000) {
                // Fade in for 1 second
                g_bgmAlpha = min(g_bgmAlpha + BGM_FADE_SPEED * 2, 1.0f);
            } else if (playTime.count() < BGM_DISPLAY_TIME) {
                // Stay fully visible
                g_bgmAlpha = 1.0f;
            } else if (playTime.count() < BGM_DISPLAY_TIME + 1000) {
                // Fade out for 1 second
                g_bgmAlpha = max(g_bgmAlpha - BGM_FADE_SPEED, 0.0f);
            } else {
                // Hidden
                g_bgmAlpha = 0.0f;
            }
        }
    } else {
        g_bgmAlpha = max(g_bgmAlpha - BGM_FADE_SPEED * 2, 0.0f);
    }

    if (g_bgmAlpha <= 0.0f) return;

    D3DVIEWPORT9 viewport;
    pDevice->GetViewport(&viewport);

    float bgmX = viewport.Width - 320.0f;
    float bgmY = viewport.Height - 100.0f;

    BYTE alpha = (BYTE)(200 * g_bgmAlpha);
    BYTE textAlpha = (BYTE)(255 * g_bgmAlpha);

    // Background
    DrawSimpleRect(pDevice, bgmX, bgmY, 300.0f, 80.0f, D3DCOLOR_ARGB(alpha, 0, 0, 0));

    // Border color based on track type
    DWORD borderColor = D3DCOLOR_ARGB(textAlpha, 100, 100, 255);
    if (currentTrack) {
        if (currentTrack->isBoss) {
            borderColor = D3DCOLOR_ARGB(textAlpha, 255, 100, 100);
        } else if (currentTrack->isSpecial) {
            borderColor = D3DCOLOR_ARGB(textAlpha, 255, 200, 100);
        }
    }

    DrawBorder(pDevice, bgmX, bgmY, 300.0f, 80.0f, 2.0f, borderColor);

    if (g_pFontSmall && g_pFont && currentTrack) {
        try {
            g_pFontSmall->DrawString(pDevice, bgmX + 10, bgmY + 5, "NOW PLAYING",
                                    D3DCOLOR_ARGB(textAlpha * 150 / 255, 150, 150, 255));

            // Track name
            std::string displayName = currentTrack->name;
            if (displayName.length() > 25) {
                displayName = displayName.substr(0, 22) + "...";
            }

            g_pFont->DrawString(pDevice, bgmX + 10, bgmY + 25, displayName,
                               D3DCOLOR_ARGB(textAlpha, 255, 255, 255));

            // Composer
            std::string composerText = "By: " + currentTrack->composer;
            g_pFontSmall->DrawString(pDevice, bgmX + 10, bgmY + 50, composerText,
                                    D3DCOLOR_ARGB(textAlpha * 200 / 255, 200, 200, 200));

            // Track number and stage
            if (currentTrack->trackNumber > 0) {
                char trackText[64];
                sprintf_s(trackText, "#%d - %s", currentTrack->trackNumber, currentTrack->stage.c_str());
                g_pFontSmall->DrawString(pDevice, bgmX + 10, bgmY + 65, trackText,
                                        D3DCOLOR_ARGB(textAlpha * 180 / 255, 180, 180, 180));
            }

            
        } catch (...) {
            LOG_ERROR("D3D9", "Exception in DrawBGMInfo text rendering");
        }
    }
}

// Draw the mod menu
void DrawModMenu(IDirect3DDevice9* pDevice) {
    if (!pDevice || !g_bShowMenu) return;

    // Ensure fonts are initialized
    if (!g_pFont || !g_pFontSmall) {
        InitializeFonts();
        if (!g_pFont || !g_pFontSmall) return;
    }

    float menuX = 50.0f;
    float menuY = 100.0f;
    float menuWidth = 350.0f;
    float menuHeight = 300.0f;

    DrawSimpleRect(pDevice, menuX, menuY, menuWidth, menuHeight, D3DCOLOR_ARGB(220, 20, 20, 20));
    DrawBorder(pDevice, menuX, menuY, menuWidth, menuHeight, 2.0f, D3DCOLOR_ARGB(255, 255, 200, 0));
    DrawSimpleRect(pDevice, menuX, menuY, menuWidth, 30.0f, D3DCOLOR_ARGB(255, 255, 200, 0));

    if (g_pFont) {
        g_pFont->DrawString(pDevice, menuX + 10, menuY + 5, "SHOVEL KNIGHT MOD MENU V2", D3DCOLOR_ARGB(255, 0, 0, 0));
    }

    float itemY = menuY + 40.0f;
    int numItems = g_menuItems.size();

    for (int i = 0; i < numItems; i++) {
        if (i == g_nSelectedItem) {
            DrawSimpleRect(pDevice, menuX + 5, itemY, menuWidth - 10, 25, D3DCOLOR_ARGB(100, 255, 255, 0));
        }

        if (g_pFont) {
            char itemText[256];
            if (g_menuItems[i].isToggle && g_menuItems[i].pValue) {
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
            LOG_INFO("D3D9", "Menu toggled: " + std::string(g_bShowMenu ? "ON" : "OFF"));
        }
        insertPressed = true;
    } else {
        insertPressed = false;
    }

    // F6 - Toggle permanent BGM display
    if (GetAsyncKeyState(VK_F6) & 1) {
        g_bPermanentBGM = !g_bPermanentBGM;
        LOG_INFO("D3D9", "Permanent BGM display: " + std::string(g_bPermanentBGM ? "ON" : "OFF"));
    }

    // F7 - Toggle BGM discovery mode
    if (GetAsyncKeyState(VK_F7) & 1) {
        g_pBGMSystem->SetDiscoveryMode(!g_pBGMSystem->IsDiscoveryMode());
        LOG_INFO("D3D9", "BGM discovery mode: " + std::string(g_pBGMSystem->IsDiscoveryMode() ? "ON" : "OFF"));
    }

    // F8 - Show BGM statistics
    if (GetAsyncKeyState(VK_F8) & 1) {
        g_pBGMSystem->PrintStatistics();
    }

    // Process mod manager hotkeys
    g_pModManager->ProcessHotkeys();

    if (g_bShowMenu) {
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            if (!upPressed) {
                g_nSelectedItem--;
                int numItems = g_menuItems.size();
                if (g_nSelectedItem < 0) g_nSelectedItem = numItems - 1;
            }
            upPressed = true;
        } else {
            upPressed = false;
        }

        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            if (!downPressed) {
                g_nSelectedItem++;
                int numItems = g_menuItems.size();
                if (g_nSelectedItem >= numItems) g_nSelectedItem = 0;
            }
            downPressed = true;
        } else {
            downPressed = false;
        }

        if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
            if (!enterPressed) {
                if (g_menuItems[g_nSelectedItem].action) {
                    g_menuItems[g_nSelectedItem].action();
                } else if (g_menuItems[g_nSelectedItem].pValue) {
                    *g_menuItems[g_nSelectedItem].pValue = !*g_menuItems[g_nSelectedItem].pValue;
                    LOG_INFO("D3D9", std::string(g_menuItems[g_nSelectedItem].name) + ": " +
                            (*g_menuItems[g_nSelectedItem].pValue ? "ON" : "OFF"));
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
    // Prevent crashes during early frames
    if (!pDevice) return o_EndScene ? o_EndScene(pDevice) : D3D_OK;

    g_frameCount++;
    g_totalFrames++;

    // Initialize fonts early to prevent crashes
    static bool fontsInitAttempted = false;
    if (!fontsInitAttempted && g_totalFrames > 60) {
        InitializeFonts();
        fontsInitAttempted = true;
    }

    if (g_totalFrames > 480) {
        g_bSafeToRender = true;
    }

    DWORD currentTime = GetTickCount();
    if (currentTime - g_lastLogTime > 1000) {
        if (g_bShowFPS) {
            LOG_DEBUG("FPS", "FPS: " + std::to_string(g_frameCount));
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
                    LOG_INFO("D3D9", "Text interceptor hooked!");
                }
            }
            textInterceptorInitialized = true;
        }

        HandleInput();

        // Save ALL render states before drawing
        DWORD oldColorWriteEnable, oldSrcBlend, oldDestBlend, oldAlphaBlendEnable;
        DWORD oldLighting, oldZEnable, oldCullMode, oldFillMode;
        DWORD oldVertexProcessing, oldFVF;
        IDirect3DVertexBuffer9* oldStreamData = nullptr;
        UINT oldStreamOffset, oldStreamStride;
        IDirect3DBaseTexture9* oldTexture = nullptr;

        pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);
        pDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
        pDevice->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlendEnable);
        pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
        pDevice->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
        pDevice->GetRenderState(D3DRS_CULLMODE, &oldCullMode);
        pDevice->GetRenderState(D3DRS_FILLMODE, &oldFillMode);
        pDevice->GetFVF(&oldFVF);
        pDevice->GetStreamSource(0, &oldStreamData, &oldStreamOffset, &oldStreamStride);
        pDevice->GetTexture(0, &oldTexture);

        // Set render states for our UI
        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        pDevice->SetTexture(0, NULL);
        pDevice->SetStreamSource(0, NULL, 0, 0);
        pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

        // Status bar
        DrawSimpleRect(pDevice, 10.0f, 10.0f, 200.0f, 25.0f, D3DCOLOR_ARGB(200, 0, 0, 0));
        DrawBorder(pDevice, 10.0f, 10.0f, 200.0f, 25.0f, 2.0f, D3DCOLOR_ARGB(255, 255, 255, 0));

        if (g_pFont) {
            g_pFont->DrawString(pDevice, 15, 12, "SK MOD V2 - INSERT = MENU", D3DCOLOR_ARGB(255, 255, 255, 0));
        }

        // Active cheats display
        if (!g_bShowMenu && g_pFont) {
            int yOffset = 40;
            for (const auto& cheat : g_pModManager->GetCheats()) {
                if (cheat.enabled) {
                    std::string display = "[" + cheat.name + " ON]";
                    g_pFont->DrawString(pDevice, 15, yOffset, display, D3DCOLOR_ARGB(255, 0, 255, 0));
                    yOffset += 20;
                }
            }
        }

        // FPS display
        if (g_bShowFPS && g_pFontSmall) {
            char fpsText[32];
            sprintf_s(fpsText, "FPS: %d", g_frameCount);
            g_pFontSmall->DrawString(pDevice, 10, 250, fpsText, D3DCOLOR_ARGB(255, 255, 255, 0));
        }

        DrawBGMInfo(pDevice);
        DrawModMenu(pDevice);

        // Restore ALL render states
        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWriteEnable);
        pDevice->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
        pDevice->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlendEnable);
        pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
        pDevice->SetRenderState(D3DRS_ZENABLE, oldZEnable);
        pDevice->SetRenderState(D3DRS_CULLMODE, oldCullMode);
        pDevice->SetRenderState(D3DRS_FILLMODE, oldFillMode);
        pDevice->SetFVF(oldFVF);
        if (oldStreamData) {
            pDevice->SetStreamSource(0, oldStreamData, oldStreamOffset, oldStreamStride);
            oldStreamData->Release();
        }
        if (oldTexture) {
            pDevice->SetTexture(0, oldTexture);
            oldTexture->Release();
        }
    }

    return o_EndScene(pDevice);
}

// Other hook functions
HRESULT APIENTRY hkPresent(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect,
                           HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    return o_Present(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT APIENTRY hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    LOG_INFO("D3D9", "Device reset called");
    HRESULT hr = o_Reset(pDevice, pPresentationParameters);
    return hr;
}

HRESULT WINAPI hkCreateDeviceEx(IDirect3D9Ex* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface) {
    LOG_INFO("D3D9", "CreateDeviceEx called");

    HRESULT hr = o_CreateDeviceEx(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                  pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        LOG_INFO("D3D9", "Device created, hooking EndScene, Present and Reset");

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

// Try to hook via dummy device
void TryHookViaDevice() {
    LOG_INFO("D3D9", "Attempting to hook via dummy device...");

    HWND hWnd = GetDesktopWindow();

    IDirect3D9Ex* pD3DEx = nullptr;
    if (FAILED(real_Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx))) {
        LOG_ERROR("D3D9", "Failed to create D3D9Ex");
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
        LOG_INFO("D3D9", "Dummy device created");

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
    if (g_pBGMSystem) {
        g_pBGMSystem->OnSoundPlay(soundId);
    }
}

__declspec(dllexport) void NotifyBGMStop(uint32_t soundId) {
    // BGM system handles this internally now
}

__declspec(dllexport) void ResetBGMSession() {
    LOG_INFO("BGM", "BGM session reset");
}

// D3D9 exports
IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVer) {
    LoadRealD3D9Functions();
    return real_Direct3DCreate9 ? real_Direct3DCreate9(sdkVer) : nullptr;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVer, IDirect3D9Ex** ppD3D) {
    LOG_INFO("D3D9", "Direct3DCreate9Ex called");

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
        // Initialize debug console
        g_pDebugConsole = new DebugConsole();

        LOG_INFO("D3D9", "============================================");
        LOG_INFO("D3D9", "SHOVEL KNIGHT MOD LOADER V2.0");
        LOG_INFO("D3D9", "Enhanced with BGM System, Mod Manager, and Debug Console");
        LOG_INFO("D3D9", "Process ID: " + std::to_string(GetCurrentProcessId()));
        LOG_INFO("D3D9", "============================================");

        if (MH_Initialize() != MH_OK) {
            LOG_ERROR("D3D9", "Failed to initialize MinHook!");
            return FALSE;
        }

        LOG_INFO("D3D9", "MinHook initialized");

        // Initialize systems
        g_pBGMSystem = new BGMSystem("bgm_database.yaml");
        g_pBGMSystem->SetOnTrackStart([](const BGMSystem::TrackInfo& track) {
            LOG_INFO("BGM", "Started: " + track.name + " (Stage: " + track.stage + ")");
        });

        g_pModManager = new ModManager();
        g_pModManager->LoadMods();

        // Initialize menu items after ModManager is created
        InitializeMenuItems();

        // Initialize fonts early
        InitializeFonts();

        LOG_INFO("D3D9", "Ready!");
        LOG_INFO("D3D9", "Press F6 for permanent BGM display");
        LOG_INFO("D3D9", "Press F7 for BGM discovery mode");
        LOG_INFO("D3D9", "Press F8 for BGM statistics");
        LOG_INFO("D3D9", "============================================");
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_pFont) {
            delete g_pFont;
            g_pFont = nullptr;
        }
        if (g_pFontSmall) {
            delete g_pFontSmall;
            g_pFontSmall = nullptr;
        }
        if (g_pBGMSystem) {
            g_pBGMSystem->SaveDatabase();
            delete g_pBGMSystem;
            g_pBGMSystem = nullptr;
        }
        if (g_pModManager) {
            g_pModManager->SaveConfig();
            delete g_pModManager;
            g_pModManager = nullptr;
        }
        if (g_pDebugConsole) {
            delete g_pDebugConsole;
            g_pDebugConsole = nullptr;
        }

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    return TRUE;
}