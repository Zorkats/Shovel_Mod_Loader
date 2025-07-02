// font_texture_capture.h - Capture and use game font textures
#pragma once

#include <windows.h>
#include <d3d9.h>
#include <vector>
#include <iostream>

class FontTextureCapture {
private:
    struct CapturedTexture {
        IDirect3DTexture9* pTexture;
        UINT width;
        UINT height;
        void* creationAddress;
        bool isGameFont;
    };

    static std::vector<CapturedTexture> g_capturedTextures;
    static IDirect3DTexture9* g_primaryFont;
    static IDirect3DTexture9* g_secondaryFont;

public:
    // Called from CreateTexture hook
    static void OnTextureCreated(IDirect3DTexture9* pTexture, UINT width, UINT height, D3DFORMAT format, void* returnAddr) {
        // Check if it's from libGLESv2.dll
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(returnAddr, &mbi, sizeof(mbi))) {
            HMODULE hModule = (HMODULE)mbi.AllocationBase;
            char moduleName[MAX_PATH];
            if (GetModuleFileNameA(hModule, moduleName, MAX_PATH)) {
                if (strstr(moduleName, "libGLESv2.dll")) {
                    // Check if it's a likely font texture
                    if (format == D3DFMT_A8R8G8B8 &&
                        ((width == 256 && height == 256) || (width == 512 && height == 512))) {

                        CapturedTexture capture;
                        capture.pTexture = pTexture;
                        capture.width = width;
                        capture.height = height;
                        capture.creationAddress = returnAddr;
                        capture.isGameFont = true;

                        pTexture->AddRef();  // Keep a reference
                        g_capturedTextures.push_back(capture);

                        // Assign to primary/secondary font
                        if (width == 256) {
                            g_primaryFont = pTexture;
                            std::cout << "[Font Capture] Got 256x256 game font!" << std::endl;
                        } else if (width == 512) {
                            g_secondaryFont = pTexture;
                            std::cout << "[Font Capture] Got 512x512 game font!" << std::endl;
                        }

                        // Try to analyze the texture
                        AnalyzeTexture(pTexture, width, height);
                    }
                }
            }
        }
    }

    // Simple texture analysis to understand layout
    static void AnalyzeTexture(IDirect3DTexture9* pTexture, UINT width, UINT height) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(pTexture->LockRect(0, &lr, NULL, D3DLOCK_READONLY))) {
            DWORD* pixels = (DWORD*)lr.pBits;

            // Sample the texture to detect character grid
            int samplesPerRow = 16;
            int sampleSize = width / samplesPerRow;

            std::cout << "[Font Analysis] Checking " << width << "x" << height << " texture" << std::endl;

            // Look for the first non-empty cell
            bool foundContent = false;
            for (int y = 0; y < samplesPerRow && !foundContent; y++) {
                for (int x = 0; x < samplesPerRow && !foundContent; x++) {
                    // Sample center of each cell
                    int px = x * sampleSize + sampleSize / 2;
                    int py = y * sampleSize + sampleSize / 2;

                    DWORD pixel = pixels[py * (lr.Pitch / 4) + px];
                    BYTE alpha = (pixel >> 24) & 0xFF;

                    if (alpha > 0) {
                        std::cout << "[Font Analysis] Found content at grid position "
                                  << x << "," << y << std::endl;
                        foundContent = true;
                    }
                }
            }

            pTexture->UnlockRect(0);
        }
    }

    // Draw text using captured font
    static void DrawTextWithGameFont(IDirect3DDevice9* pDevice, const std::string& text,
                                    float x, float y, float scale, D3DCOLOR color) {
        if (!g_primaryFont) return;

        // This is a simplified version - in reality, we'd need the exact character mapping
        // For now, let's assume a standard ASCII grid layout

        struct VERTEX {
            float x, y, z, rhw;
            DWORD color;
            float u, v;
        };

        std::vector<VERTEX> vertices;

        float charSize = 16.0f * scale;  // Assume 16x16 characters in a 256x256 texture
        float uvSize = 1.0f / 16.0f;     // 16x16 grid

        float currentX = x;

        for (char c : text) {
            if (c < 32 || c > 127) continue;  // ASCII printable range

            // Calculate UV coordinates (simple grid mapping)
            int gridX = (c - 32) % 16;
            int gridY = (c - 32) / 16;

            float u0 = gridX * uvSize;
            float v0 = gridY * uvSize;
            float u1 = u0 + uvSize;
            float v1 = v0 + uvSize;

            // Create quad
            VERTEX quad[4] = {
                {currentX, y, 0, 1, color, u0, v0},
                {currentX + charSize, y, 0, 1, color, u1, v0},
                {currentX, y + charSize, 0, 1, color, u0, v1},
                {currentX + charSize, y + charSize, 0, 1, color, u1, v1}
            };

            // Add to vertex buffer
            if (!vertices.empty()) {
                // Degenerate triangles for strip
                vertices.push_back(vertices.back());
                vertices.push_back(quad[0]);
            }

            vertices.push_back(quad[0]);
            vertices.push_back(quad[1]);
            vertices.push_back(quad[2]);
            vertices.push_back(quad[3]);

            currentX += charSize * 0.6f;  // Adjust spacing
        }

        if (!vertices.empty()) {
            // Save states
            IDirect3DBaseTexture9* oldTex;
            DWORD oldFVF, oldAlphaBlend, oldSrcBlend, oldDestBlend;

            pDevice->GetTexture(0, &oldTex);
            pDevice->GetFVF(&oldFVF);
            pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
            pDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
            pDevice->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);

            // Set states
            pDevice->SetTexture(0, g_primaryFont);
            pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

            // Draw
            pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, vertices.size() - 2,
                                    vertices.data(), sizeof(VERTEX));

            // Restore
            pDevice->SetTexture(0, oldTex);
            pDevice->SetFVF(oldFVF);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
            pDevice->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
            pDevice->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);

            if (oldTex) oldTex->Release();
        }
    }

    static bool HasGameFont() {
        return g_primaryFont != nullptr;
    }

    static void SaveFontTextures() {
        // Save textures for analysis (requires D3DX)
        if (g_primaryFont) {
            std::cout << "[Font Capture] TODO: Save primary font texture" << std::endl;
            // D3DXSaveTextureToFile(L"font_256.png", D3DXIFF_PNG, g_primaryFont, NULL);
        }
        if (g_secondaryFont) {
            std::cout << "[Font Capture] TODO: Save secondary font texture" << std::endl;
            // D3DXSaveTextureToFile(L"font_512.png", D3DXIFF_PNG, g_secondaryFont, NULL);
        }
    }
};

// Static definitions
std::vector<FontTextureCapture::CapturedTexture> FontTextureCapture::g_capturedTextures;
IDirect3DTexture9* FontTextureCapture::g_primaryFont = nullptr;
IDirect3DTexture9* FontTextureCapture::g_secondaryFont = nullptr;