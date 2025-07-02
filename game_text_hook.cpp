// enhanced_game_text_hook.cpp - Hook game's text rendering with font texture capture
#pragma once

#include <windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "MinHook.h"

class EnhancedGameTextHook {
private:
    // Font texture info
    struct FontTexture {
        IDirect3DTexture9* pTexture;
        UINT width;
        UINT height;
        D3DFORMAT format;
        void* creationAddress;
        bool isActive;
    };

    static FontTexture g_fontSmall;     // 256x256 font
    static FontTexture g_fontLarge;     // 512x512 font
    static IDirect3DDevice9* g_pDevice;

    // Text rendering intercept
    struct TextDrawCall {
        std::string text;
        float x, y;
        uint32_t color;
        IDirect3DTexture9* fontUsed;
    };
    static std::vector<TextDrawCall> g_interceptedText;

    // CreateTexture hook for font capture
    typedef HRESULT (STDMETHODCALLTYPE *CreateTexture_t)(
        IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
    static CreateTexture_t o_CreateTexture;

    // DrawPrimitiveUP hook for text rendering
    typedef HRESULT (STDMETHODCALLTYPE *DrawPrimitiveUP_t)(
        IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
    static DrawPrimitiveUP_t o_DrawPrimitiveUP;

    // SetTexture hook to track active font
    typedef HRESULT (STDMETHODCALLTYPE *SetTexture_t)(
        IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
    static SetTexture_t o_SetTexture;

    // Current rendering state
    static IDirect3DTexture9* g_currentTexture;
    static bool g_isRenderingText;

public:
    // Initialize hooks
    static bool Initialize(IDirect3DDevice9* pDevice) {
        if (!pDevice) return false;

        g_pDevice = pDevice;
        void** vtable = *(void***)pDevice;

        std::cout << "[GameTextHook] Initializing enhanced text hooks..." << std::endl;

        // Hook CreateTexture (index 23)
        MH_CreateHook(vtable[23], &hkCreateTexture, (void**)&o_CreateTexture);

        // Hook DrawPrimitiveUP (index 83)
        MH_CreateHook(vtable[83], &hkDrawPrimitiveUP, (void**)&o_DrawPrimitiveUP);

        // Hook SetTexture (index 65)
        MH_CreateHook(vtable[65], &hkSetTexture, (void**)&o_SetTexture);

        MH_EnableHook(MH_ALL_HOOKS);

        // Also hook libGLESv2.dll font creation directly
        HMODULE hLibGLES = GetModuleHandleA("libGLESv2.dll");
        if (hLibGLES) {
            // Hook at the known RVA
            void* pFontCreate = (BYTE*)hLibGLES + 0x349BE;
            std::cout << "[GameTextHook] Found libGLESv2.dll font creation at: 0x"
                      << std::hex << pFontCreate << std::dec << std::endl;

            // Could add specific hook here if needed
        }

        return true;
    }

    // CreateTexture hook - capture font textures
    static HRESULT STDMETHODCALLTYPE hkCreateTexture(
        IDirect3DDevice9* pDevice,
        UINT Width, UINT Height,
        UINT Levels, DWORD Usage,
        D3DFORMAT Format, D3DPOOL Pool,
        IDirect3DTexture9** ppTexture,
        HANDLE* pSharedHandle) {

        HRESULT hr = o_CreateTexture(pDevice, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

        if (SUCCEEDED(hr) && ppTexture && *ppTexture) {
            // Check if this is from libGLESv2.dll at our known RVA
            void* returnAddr = _ReturnAddress();
            MEMORY_BASIC_INFORMATION mbi;

            if (VirtualQuery(returnAddr, &mbi, sizeof(mbi))) {
                HMODULE hModule = (HMODULE)mbi.AllocationBase;
                char moduleName[MAX_PATH];

                if (GetModuleFileNameA(hModule, moduleName, MAX_PATH)) {
                    if (strstr(moduleName, "libGLESv2.dll")) {
                        DWORD rva = (DWORD)returnAddr - (DWORD)hModule;

                        // Check if it's our font creation RVA
                        if (rva >= 0x349B0 && rva <= 0x349D0) {  // Small range around 0x349BE
                            // This is a font texture!
                            if (Width == 256 && Height == 256 && Format == D3DFMT_A8R8G8B8) {
                                g_fontSmall.pTexture = *ppTexture;
                                g_fontSmall.width = Width;
                                g_fontSmall.height = Height;
                                g_fontSmall.format = Format;
                                g_fontSmall.creationAddress = returnAddr;
                                g_fontSmall.isActive = true;
                                (*ppTexture)->AddRef();  // Keep reference

                                std::cout << "[GameTextHook] Captured 256x256 font texture!" << std::endl;
                                AnalyzeFontTexture(*ppTexture, 256);
                            }
                            else if (Width == 512 && Height == 512 && Format == D3DFMT_A8R8G8B8) {
                                g_fontLarge.pTexture = *ppTexture;
                                g_fontLarge.width = Width;
                                g_fontLarge.height = Height;
                                g_fontLarge.format = Format;
                                g_fontLarge.creationAddress = returnAddr;
                                g_fontLarge.isActive = true;
                                (*ppTexture)->AddRef();  // Keep reference

                                std::cout << "[GameTextHook] Captured 512x512 font texture!" << std::endl;
                                AnalyzeFontTexture(*ppTexture, 512);
                            }
                        }
                    }
                }
            }
        }

        return hr;
    }

    // SetTexture hook - detect when font is being used
    static HRESULT STDMETHODCALLTYPE hkSetTexture(
        IDirect3DDevice9* pDevice,
        DWORD Stage,
        IDirect3DBaseTexture9* pTexture) {

        if (Stage == 0) {
            g_currentTexture = (IDirect3DTexture9*)pTexture;

            // Check if this is one of our font textures
            if (pTexture == g_fontSmall.pTexture || pTexture == g_fontLarge.pTexture) {
                g_isRenderingText = true;
            } else {
                g_isRenderingText = false;
            }
        }

        return o_SetTexture(pDevice, Stage, pTexture);
    }

    // DrawPrimitiveUP hook - intercept text rendering
    static HRESULT STDMETHODCALLTYPE hkDrawPrimitiveUP(
        IDirect3DDevice9* pDevice,
        D3DPRIMITIVETYPE PrimitiveType,
        UINT PrimitiveCount,
        const void* pVertexStreamZeroData,
        UINT VertexStreamZeroStride) {

        // If we're rendering with a font texture, analyze the call
        if (g_isRenderingText && PrimitiveType == D3DPT_TRIANGLESTRIP) {
            static int textCallCount = 0;
            if (textCallCount++ < 100) {  // Log first 100 to avoid spam

                // Analyze vertex data to get position
                struct Vertex {
                    float x, y, z, rhw;
                    DWORD color;
                    float u, v;
                };

                if (VertexStreamZeroStride >= sizeof(Vertex)) {
                    const Vertex* vertices = (const Vertex*)pVertexStreamZeroData;

                    float x = vertices[0].x;
                    float y = vertices[0].y;
                    DWORD color = vertices[0].color;

                    std::cout << "[GameTextHook] Text render at " << x << "," << y
                              << " using " << (g_currentTexture == g_fontSmall.pTexture ? "small" : "large")
                              << " font" << std::endl;

                    // Store for our overlay to use
                    TextDrawCall call;
                    call.x = x;
                    call.y = y;
                    call.color = color;
                    call.fontUsed = g_currentTexture;
                    g_interceptedText.push_back(call);
                }
            }
        }

        return o_DrawPrimitiveUP(pDevice, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
    }

    // Analyze font texture to understand character layout
    static void AnalyzeFontTexture(IDirect3DTexture9* pTexture, UINT size) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(pTexture->LockRect(0, &lr, NULL, D3DLOCK_READONLY))) {
            DWORD* pixels = (DWORD*)lr.pBits;

            // Analyze for character grid
            int gridSize = 16;  // Assume 16x16 grid
            int charSize = size / gridSize;

            std::cout << "[GameTextHook] Analyzing " << size << "x" << size
                      << " font texture (grid: " << gridSize << "x" << gridSize
                      << ", char size: " << charSize << "x" << charSize << ")" << std::endl;

            // Sample first few characters
            for (int gridY = 0; gridY < 4; gridY++) {
                for (int gridX = 0; gridX < 8; gridX++) {
                    // Sample center of character cell
                    int px = gridX * charSize + charSize/2;
                    int py = gridY * charSize + charSize/2;

                    DWORD pixel = pixels[py * (lr.Pitch/4) + px];
                    BYTE alpha = (pixel >> 24) & 0xFF;

                    if (alpha > 128) {  // Has content
                        int charCode = gridY * gridSize + gridX;
                        if (charCode >= 32 && charCode <= 126) {  // Printable ASCII
                            std::cout << "  Char '" << (char)charCode << "' at grid "
                                      << gridX << "," << gridY << std::endl;
                        }
                    }
                }
            }

            pTexture->UnlockRect(0);
        }
    }

    // Draw text using captured game font
    static void DrawTextWithGameFont(const std::string& text, float x, float y, float scale, D3DCOLOR color) {
        if (!g_pDevice || !g_fontSmall.isActive) return;

        // Use the game's font texture
        IDirect3DTexture9* fontTex = (scale > 1.5f && g_fontLarge.isActive) ?
                                     g_fontLarge.pTexture : g_fontSmall.pTexture;
        UINT texSize = (fontTex == g_fontLarge.pTexture) ? 512 : 256;

        struct VERTEX {
            float x, y, z, rhw;
            DWORD color;
            float u, v;
        };

        std::vector<VERTEX> vertices;

        // Character grid settings (determined from analysis)
        int gridSize = 16;
        float charSize = (float)texSize / gridSize;
        float uvSize = 1.0f / gridSize;

        float currentX = x;
        float screenCharSize = charSize * scale;

        for (char c : text) {
            if (c < 32 || c > 127) continue;

            // Calculate UV coordinates
            int charIndex = c - 32;  // Assuming ASCII starts at space (32)
            int gridX = charIndex % gridSize;
            int gridY = charIndex / gridSize;

            float u0 = gridX * uvSize;
            float v0 = gridY * uvSize;
            float u1 = u0 + uvSize;
            float v1 = v0 + uvSize;

            // Create quad
            VERTEX quad[4] = {
                {currentX, y, 0, 1, color, u0, v0},
                {currentX + screenCharSize, y, 0, 1, color, u1, v0},
                {currentX, y + screenCharSize, 0, 1, color, u0, v1},
                {currentX + screenCharSize, y + screenCharSize, 0, 1, color, u1, v1}
            };

            // Add to vertex buffer with degenerate triangles
            if (!vertices.empty()) {
                vertices.push_back(vertices.back());
                vertices.push_back(quad[0]);
            }

            vertices.push_back(quad[0]);
            vertices.push_back(quad[1]);
            vertices.push_back(quad[2]);
            vertices.push_back(quad[3]);

            currentX += screenCharSize * 0.7f;  // Spacing
        }

        if (!vertices.empty()) {
            // Save states
            IDirect3DBaseTexture9* oldTex;
            DWORD oldFVF;
            g_pDevice->GetTexture(0, &oldTex);
            g_pDevice->GetFVF(&oldFVF);

            // Set states
            g_pDevice->SetTexture(0, fontTex);
            g_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

            // Draw
            g_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, vertices.size() - 2,
                                      vertices.data(), sizeof(VERTEX));

            // Restore
            g_pDevice->SetTexture(0, oldTex);
            g_pDevice->SetFVF(oldFVF);
            if (oldTex) oldTex->Release();
        }
    }

    static bool HasGameFont() {
        return g_fontSmall.isActive || g_fontLarge.isActive;
    }

    static void GetInterceptedText(std::vector<TextDrawCall>& outText) {
        outText = g_interceptedText;
        g_interceptedText.clear();
    }
};

// Static member definitions
EnhancedGameTextHook::FontTexture EnhancedGameTextHook::g_fontSmall = {};
EnhancedGameTextHook::FontTexture EnhancedGameTextHook::g_fontLarge = {};
IDirect3DDevice9* EnhancedGameTextHook::g_pDevice = nullptr;
std::vector<EnhancedGameTextHook::TextDrawCall> EnhancedGameTextHook::g_interceptedText;
EnhancedGameTextHook::CreateTexture_t EnhancedGameTextHook::o_CreateTexture = nullptr;
EnhancedGameTextHook::DrawPrimitiveUP_t EnhancedGameTextHook::o_DrawPrimitiveUP = nullptr;
EnhancedGameTextHook::SetTexture_t EnhancedGameTextHook::o_SetTexture = nullptr;
IDirect3DTexture9* EnhancedGameTextHook::g_currentTexture = nullptr;
bool EnhancedGameTextHook::g_isRenderingText = false;