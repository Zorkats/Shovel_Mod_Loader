// simple_overlay_no_fonts.cpp - Alternative overlay using only rectangles
// This version avoids D3DX fonts entirely to prevent crashes

#include <windows.h>
#include <d3d9.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#pragma comment(lib, "d3d9.lib")

// Simple vertex structure
struct CUSTOMVERTEX {
    float x, y, z, rhw;
    DWORD color;
};

// Letter definitions using 5x7 grid
struct LetterDef {
    bool pixels[7][5];  // 7 rows, 5 columns
};

// Bitmap font class - draws letters using rectangles
class BitmapFont {
private:
    std::map<char, LetterDef> m_letters;
    float m_scale;

    void InitializeLetters() {
        // Define some basic letters and numbers using pixel patterns
        // Letter 'A'
        m_letters['A'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1}
        }};

        // Letter 'B'
        m_letters['B'] = {{
            {1,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,0}
        }};

        // Letter 'C'
        m_letters['C'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,1},
            {0,1,1,1,0}
        }};

        // Letter 'D'
        m_letters['D'] = {{
            {1,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,0}
        }};

        // Letter 'E'
        m_letters['E'] = {{
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,1,1,1,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,1,1,1,1}
        }};

        // Letter 'G'
        m_letters['G'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,0},
            {1,0,1,1,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        }};

        // Letter 'H'
        m_letters['H'] = {{
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1}
        }};

        // Letter 'I'
        m_letters['I'] = {{
            {1,1,1,1,1},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {1,1,1,1,1}
        }};

        // Letter 'K'
        m_letters['K'] = {{
            {1,0,0,0,1},
            {1,0,0,1,0},
            {1,0,1,0,0},
            {1,1,0,0,0},
            {1,0,1,0,0},
            {1,0,0,1,0},
            {1,0,0,0,1}
        }};

        // Letter 'L'
        m_letters['L'] = {{
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,1,1,1,1}
        }};

        // Letter 'M'
        m_letters['M'] = {{
            {1,0,0,0,1},
            {1,1,0,1,1},
            {1,0,1,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1}
        }};

        // Letter 'N'
        m_letters['N'] = {{
            {1,0,0,0,1},
            {1,1,0,0,1},
            {1,0,1,0,1},
            {1,0,0,1,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1}
        }};

        // Letter 'O'
        m_letters['O'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        }};

        // Letter 'P'
        m_letters['P'] = {{
            {1,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,0},
            {1,0,0,0,0},
            {1,0,0,0,0},
            {1,0,0,0,0}
        }};

        // Letter 'R'
        m_letters['R'] = {{
            {1,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,0},
            {1,0,1,0,0},
            {1,0,0,1,0},
            {1,0,0,0,1}
        }};

        // Letter 'S'
        m_letters['S'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,0},
            {0,1,1,1,0},
            {0,0,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        }};

        // Letter 'T'
        m_letters['T'] = {{
            {1,1,1,1,1},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0}
        }};

        // Letter 'U'
        m_letters['U'] = {{
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        }};

        // Letter 'V'
        m_letters['V'] = {{
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {0,1,0,1,0},
            {0,0,1,0,0}
        }};

        // Letter 'W'
        m_letters['W'] = {{
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,1,0,1},
            {1,0,1,0,1},
            {1,1,0,1,1},
            {1,0,0,0,1}
        }};

        // Number '0'
        m_letters['0'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,1,1},
            {1,0,1,0,1},
            {1,1,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        }};

        // Number '1'
        m_letters['1'] = {{
            {0,0,1,0,0},
            {0,1,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {1,1,1,1,1}
        }};

        // Number '2'
        m_letters['2'] = {{
            {0,1,1,1,0},
            {1,0,0,0,1},
            {0,0,0,0,1},
            {0,0,0,1,0},
            {0,0,1,0,0},
            {0,1,0,0,0},
            {1,1,1,1,1}
        }};

        // Space
        m_letters[' '] = {{
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0}
        }};

        // Hyphen
        m_letters['-'] = {{
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0},
            {1,1,1,1,1},
            {0,0,0,0,0},
            {0,0,0,0,0},
            {0,0,0,0,0}
        }};

        // Colon
        m_letters[':'] = {{
            {0,0,0,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,0,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,0,0,0}
        }};

        // Left bracket
        m_letters['['] = {{
            {0,0,1,1,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,1,0}
        }};

        // Right bracket
        m_letters[']'] = {{
            {0,1,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,0,1,0,0},
            {0,1,1,0,0}
        }};
    }

public:
    BitmapFont(float scale = 2.0f) : m_scale(scale) {
        InitializeLetters();
    }

    void DrawPixel(IDirect3DDevice9* pDevice, float x, float y, float size, DWORD color) {
        CUSTOMVERTEX vertices[] = {
            {x, y, 0.0f, 1.0f, color},
            {x + size, y, 0.0f, 1.0f, color},
            {x, y + size, 0.0f, 1.0f, color},
            {x + size, y + size, 0.0f, 1.0f, color}
        };

        pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
    }

    void DrawChar(IDirect3DDevice9* pDevice, float x, float y, char c, DWORD color) {
        // Convert to uppercase
        c = toupper(c);

        // Check if we have this character
        if (m_letters.find(c) == m_letters.end()) {
            // Draw a box for unknown characters
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if (row == 0 || row == 6 || col == 0 || col == 4) {
                        DrawPixel(pDevice, x + col * m_scale, y + row * m_scale, m_scale, color);
                    }
                }
            }
            return;
        }

        // Draw the character
        const LetterDef& letter = m_letters[c];
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (letter.pixels[row][col]) {
                    DrawPixel(pDevice, x + col * m_scale, y + row * m_scale, m_scale, color);
                }
            }
        }
    }

    void DrawString(IDirect3DDevice9* pDevice, float x, float y, const std::string& text, DWORD color) {
        float currentX = x;
        for (char c : text) {
            DrawChar(pDevice, currentX, y, c, color);
            currentX += (6 * m_scale);  // Character width + spacing
        }
    }

    float GetCharWidth() const { return 6 * m_scale; }
    float GetCharHeight() const { return 7 * m_scale; }
};

// Simple overlay class using only rectangles
class SimpleOverlay {
private:
    IDirect3DDevice9* m_pDevice;
    BitmapFont m_font;
    BitmapFont m_fontSmall;
    bool m_bInitialized;

    // Menu state
    bool m_bShowMenu;
    int m_nSelectedItem;
    std::vector<std::string> m_menuItems;

    // Cheat states
    bool m_bGodMode;
    bool m_bInfiniteGems;
    bool m_bSpeedHack;
    bool m_bShowFPS;
    bool m_bShowBGM;

    // BGM info
    uint32_t m_currentBGM;
    std::string m_currentBGMName;
    DWORD m_bgmStartTime;

public:
    SimpleOverlay() : m_pDevice(nullptr), m_font(2.0f), m_fontSmall(1.5f),
                      m_bInitialized(false), m_bShowMenu(false), m_nSelectedItem(0),
                      m_bGodMode(false), m_bInfiniteGems(false), m_bSpeedHack(false),
                      m_bShowFPS(false), m_bShowBGM(true), m_currentBGM(0),
                      m_currentBGMName("NONE"), m_bgmStartTime(0) {

        m_menuItems.push_back("GOD MODE");
        m_menuItems.push_back("INFINITE GEMS");
        m_menuItems.push_back("SPEED HACK");
        m_menuItems.push_back("SHOW FPS");
        m_menuItems.push_back("SHOW BGM");
        m_menuItems.push_back("EXIT MENU");
    }

    bool Initialize(IDirect3DDevice9* pDevice) {
        if (!pDevice) return false;
        m_pDevice = pDevice;
        m_bInitialized = true;
        std::cout << "[Simple Overlay] Initialized successfully!" << std::endl;
        return true;
    }

    void DrawRect(float x, float y, float width, float height, DWORD color) {
        if (!m_pDevice) return;

        CUSTOMVERTEX vertices[] = {
            {x, y, 0.0f, 1.0f, color},
            {x + width, y, 0.0f, 1.0f, color},
            {x, y + height, 0.0f, 1.0f, color},
            {x + width, y + height, 0.0f, 1.0f, color}
        };

        m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        m_pDevice->SetTexture(0, NULL);
        m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
    }

    void DrawBorder(float x, float y, float width, float height, float thickness, DWORD color) {
        DrawRect(x, y, width, thickness, color);  // Top
        DrawRect(x, y + height - thickness, width, thickness, color);  // Bottom
        DrawRect(x, y, thickness, height, color);  // Left
        DrawRect(x + width - thickness, y, thickness, height, color);  // Right
    }

    void HandleInput() {
        static bool insertPressed = false;
        static bool upPressed = false;
        static bool downPressed = false;
        static bool enterPressed = false;

        // Toggle menu
        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertPressed) {
                m_bShowMenu = !m_bShowMenu;
                std::cout << "[Overlay] Menu toggled: " << (m_bShowMenu ? "ON" : "OFF") << std::endl;
            }
            insertPressed = true;
        } else {
            insertPressed = false;
        }

        if (!m_bShowMenu) return;

        // Navigate up
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            if (!upPressed) {
                m_nSelectedItem--;
                if (m_nSelectedItem < 0) m_nSelectedItem = m_menuItems.size() - 1;
            }
            upPressed = true;
        } else {
            upPressed = false;
        }

        // Navigate down
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            if (!downPressed) {
                m_nSelectedItem++;
                if (m_nSelectedItem >= m_menuItems.size()) m_nSelectedItem = 0;
            }
            downPressed = true;
        } else {
            downPressed = false;
        }

        // Select item
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
            if (!enterPressed) {
                switch (m_nSelectedItem) {
                    case 0: m_bGodMode = !m_bGodMode; break;
                    case 1: m_bInfiniteGems = !m_bInfiniteGems; break;
                    case 2: m_bSpeedHack = !m_bSpeedHack; break;
                    case 3: m_bShowFPS = !m_bShowFPS; break;
                    case 4: m_bShowBGM = !m_bShowBGM; break;
                    case 5: m_bShowMenu = false; break;
                }
            }
            enterPressed = true;
        } else {
            enterPressed = false;
        }
    }

    void Render() {
        if (!m_bInitialized || !m_pDevice) return;

        HandleInput();

        // Save render states
        DWORD oldColorWriteEnable, oldSrcBlend, oldDestBlend, oldAlphaBlendEnable;
        m_pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);
        m_pDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
        m_pDevice->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);
        m_pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlendEnable);

        // Set render states
        m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
        m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

        // Always show status
        DrawRect(10, 10, 200, 25, D3DCOLOR_ARGB(200, 0, 0, 0));
        DrawBorder(10, 10, 200, 25, 2, D3DCOLOR_ARGB(255, 255, 255, 0));
        m_fontSmall.DrawString(m_pDevice, 15, 14, "SK MOD - INSERT MENU", D3DCOLOR_ARGB(255, 255, 255, 0));

        // Show active cheats
        int yOffset = 40;
        if (m_bGodMode) {
            m_fontSmall.DrawString(m_pDevice, 15, yOffset, "[GOD MODE]", D3DCOLOR_ARGB(255, 0, 255, 0));
            yOffset += 15;
        }
        if (m_bInfiniteGems) {
            m_fontSmall.DrawString(m_pDevice, 15, yOffset, "[INFINITE GEMS]", D3DCOLOR_ARGB(255, 0, 255, 0));
            yOffset += 15;
        }
        if (m_bSpeedHack) {
            m_fontSmall.DrawString(m_pDevice, 15, yOffset, "[SPEED HACK]", D3DCOLOR_ARGB(255, 0, 255, 0));
            yOffset += 15;
        }

        // Draw BGM info
        if (m_bShowBGM) {
            D3DVIEWPORT9 viewport;
            m_pDevice->GetViewport(&viewport);

            float bgmX = viewport.Width - 250.0f;
            float bgmY = viewport.Height - 80.0f;

            DrawRect(bgmX, bgmY, 240, 70, D3DCOLOR_ARGB(200, 0, 0, 0));
            DrawBorder(bgmX, bgmY, 240, 70, 2, D3DCOLOR_ARGB(255, 100, 100, 255));

            m_fontSmall.DrawString(m_pDevice, bgmX + 5, bgmY + 5, "NOW PLAYING", D3DCOLOR_ARGB(255, 150, 150, 255));

            if (m_currentBGM != 0) {
                m_font.DrawString(m_pDevice, bgmX + 5, bgmY + 20, m_currentBGMName, D3DCOLOR_ARGB(255, 255, 255, 255));

                char idText[32];
                sprintf_s(idText, "ID: %04X", m_currentBGM);
                m_fontSmall.DrawString(m_pDevice, bgmX + 5, bgmY + 45, idText, D3DCOLOR_ARGB(255, 200, 200, 200));

                DWORD playTime = (GetTickCount() - m_bgmStartTime) / 1000;
                sprintf_s(idText, "TIME: %d:%02d", playTime / 60, playTime % 60);
                m_fontSmall.DrawString(m_pDevice, bgmX + 150, bgmY + 45, idText, D3DCOLOR_ARGB(255, 200, 200, 200));
            }
        }

        // Draw menu
        if (m_bShowMenu) {
            D3DVIEWPORT9 viewport;
            m_pDevice->GetViewport(&viewport);

            float menuX = (viewport.Width - 300) / 2;
            float menuY = (viewport.Height - 250) / 2;

            // Background
            DrawRect(menuX, menuY, 300, 250, D3DCOLOR_ARGB(220, 20, 20, 20));
            DrawBorder(menuX, menuY, 300, 250, 2, D3DCOLOR_ARGB(255, 255, 200, 0));

            // Title bar
            DrawRect(menuX, menuY, 300, 30, D3DCOLOR_ARGB(255, 255, 200, 0));
            m_font.DrawString(m_pDevice, menuX + 50, menuY + 8, "SHOVEL KNIGHT MOD", D3DCOLOR_ARGB(255, 0, 0, 0));

            // Menu items
            for (int i = 0; i < m_menuItems.size(); i++) {
                float itemY = menuY + 50 + (i * 25);

                if (i == m_nSelectedItem) {
                    DrawRect(menuX + 5, itemY - 2, 290, 20, D3DCOLOR_ARGB(100, 255, 255, 0));
                }

                DWORD color = (i == m_nSelectedItem) ?
                    D3DCOLOR_ARGB(255, 255, 255, 0) :
                    D3DCOLOR_ARGB(255, 200, 200, 200);

                std::string item = m_menuItems[i];
                if (i == 0 && m_bGodMode) item += " [ON]";
                if (i == 1 && m_bInfiniteGems) item += " [ON]";
                if (i == 2 && m_bSpeedHack) item += " [ON]";
                if (i == 3 && m_bShowFPS) item += " [ON]";
                if (i == 4 && m_bShowBGM) item += " [ON]";

                m_font.DrawString(m_pDevice, menuX + 20, itemY, item, color);
            }

            // Instructions
            m_fontSmall.DrawString(m_pDevice, menuX + 20, menuY + 220, "UP DOWN ENTER", D3DCOLOR_ARGB(255, 150, 150, 150));
        }

        // Restore render states
        m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWriteEnable);
        m_pDevice->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
        m_pDevice->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
        m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlendEnable);
    }

    // BGM functions
    void SetBGM(uint32_t id, const std::string& name) {
        m_currentBGM = id;
        m_currentBGMName = name;
        m_bgmStartTime = GetTickCount();
    }

    void ClearBGM() {
        m_currentBGM = 0;
        m_currentBGMName = "NONE";
    }

    // Getters
    bool IsGodMode() const { return m_bGodMode; }
    bool IsInfiniteGems() const { return m_bInfiniteGems; }
    bool IsSpeedHack() const { return m_bSpeedHack; }
};

// Global instance
static SimpleOverlay* g_pOverlay = nullptr;

// Export functions
extern "C" {
    __declspec(dllexport) bool InitializeSimpleOverlay(IDirect3DDevice9* pDevice) {
        if (!g_pOverlay) {
            g_pOverlay = new SimpleOverlay();
        }
        return g_pOverlay->Initialize(pDevice);
    }

    __declspec(dllexport) void RenderSimpleOverlay() {
        if (g_pOverlay) {
            g_pOverlay->Render();
        }
    }

    __declspec(dllexport) void ReleaseSimpleOverlay() {
        if (g_pOverlay) {
            delete g_pOverlay;
            g_pOverlay = nullptr;
        }
    }

    __declspec(dllexport) void SetOverlayBGM(uint32_t id, const char* name) {
        if (g_pOverlay) {
            g_pOverlay->SetBGM(id, name);
        }
    }

    __declspec(dllexport) void ClearOverlayBGM() {
        if (g_pOverlay) {
            g_pOverlay->ClearBGM();
        }
    }

    __declspec(dllexport) bool IsGodModeOn() {
        return g_pOverlay ? g_pOverlay->IsGodMode() : false;
    }

    __declspec(dllexport) bool IsInfiniteGemsOn() {
        return g_pOverlay ? g_pOverlay->IsInfiniteGems() : false;
    }

    __declspec(dllexport) bool IsSpeedHackOn() {
        return g_pOverlay ? g_pOverlay->IsSpeedHack() : false;
    }
}