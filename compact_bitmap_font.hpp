// compact_bitmap_font.h - Compact bitmap font using only rectangles
#pragma once

#include <d3d9.h>
#include <string>
#include <cstring>

class CompactBitmapFont {
private:
    float m_scale;

    struct CUSTOMVERTEX {
        float x, y, z, rhw;
        DWORD color;
    };

    void DrawPixel(IDirect3DDevice9* pDevice, float x, float y, float size, DWORD color) {
        CUSTOMVERTEX vertices[] = {
            {x, y, 0.0f, 1.0f, color},
            {x + size, y, 0.0f, 1.0f, color},
            {x, y + size, 0.0f, 1.0f, color},
            {x + size, y + size, 0.0f, 1.0f, color}
        };

        pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        pDevice->SetTexture(0, NULL);
        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
    }

    // Get font data for a character
    unsigned char GetCharData(char c, int row) {
        // Define character patterns inline
        switch(c) {
            // Letters A-Z
            case 'A': case 'a': {
                static const unsigned char data[] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'B': case 'b': {
                static const unsigned char data[] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
                return row < 7 ? data[row] : 0;
            }
            case 'C': case 'c': {
                static const unsigned char data[] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case 'D': case 'd': {
                static const unsigned char data[] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
                return row < 7 ? data[row] : 0;
            }
            case 'E': case 'e': {
                static const unsigned char data[] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
                return row < 7 ? data[row] : 0;
            }
            case 'F': case 'f': {
                static const unsigned char data[] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
                return row < 7 ? data[row] : 0;
            }
            case 'G': case 'g': {
                static const unsigned char data[] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case 'H': case 'h': {
                static const unsigned char data[] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'I': case 'i': {
                static const unsigned char data[] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case 'K': case 'k': {
                static const unsigned char data[] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'L': case 'l': {
                static const unsigned char data[] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
                return row < 7 ? data[row] : 0;
            }
            case 'M': case 'm': {
                static const unsigned char data[] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'N': case 'n': {
                static const unsigned char data[] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'O': case 'o': {
                static const unsigned char data[] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case 'P': case 'p': {
                static const unsigned char data[] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
                return row < 7 ? data[row] : 0;
            }
            case 'R': case 'r': {
                static const unsigned char data[] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'S': case 's': {
                static const unsigned char data[] = {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case 'T': case 't': {
                static const unsigned char data[] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
                return row < 7 ? data[row] : 0;
            }
            case 'U': case 'u': {
                static const unsigned char data[] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case 'V': case 'v': {
                static const unsigned char data[] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
                return row < 7 ? data[row] : 0;
            }
            case 'W': case 'w': {
                static const unsigned char data[] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'X': case 'x': {
                static const unsigned char data[] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
                return row < 7 ? data[row] : 0;
            }
            case 'Y': case 'y': {
                static const unsigned char data[] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
                return row < 7 ? data[row] : 0;
            }

            // Numbers
            case '0': {
                static const unsigned char data[] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '1': {
                static const unsigned char data[] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '2': {
                static const unsigned char data[] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
                return row < 7 ? data[row] : 0;
            }
            case '3': {
                static const unsigned char data[] = {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '4': {
                static const unsigned char data[] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
                return row < 7 ? data[row] : 0;
            }
            case '5': {
                static const unsigned char data[] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '6': {
                static const unsigned char data[] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '7': {
                static const unsigned char data[] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
                return row < 7 ? data[row] : 0;
            }
            case '8': {
                static const unsigned char data[] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '9': {
                static const unsigned char data[] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};
                return row < 7 ? data[row] : 0;
            }

            // Special characters
            case ' ': {
                static const unsigned char data[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00};
                return row < 7 ? data[row] : 0;
            }
            case '-': {
                static const unsigned char data[] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00};
                return row < 7 ? data[row] : 0;
            }
            case ':': {
                static const unsigned char data[] = {0x00,0x04,0x00,0x00,0x04,0x00,0x00};
                return row < 7 ? data[row] : 0;
            }
            case '=': {
                static const unsigned char data[] = {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00};
                return row < 7 ? data[row] : 0;
            }
            case '[': {
                static const unsigned char data[] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case ']': {
                static const unsigned char data[] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E};
                return row < 7 ? data[row] : 0;
            }
            case '/': {
                static const unsigned char data[] = {0x01,0x02,0x04,0x08,0x10,0x00,0x00};
                return row < 7 ? data[row] : 0;
            }
            case '|': {
                static const unsigned char data[] = {0x04,0x04,0x04,0x00,0x04,0x04,0x04};
                return row < 7 ? data[row] : 0;
            }

            // Default - return a box for unknown characters
            default: {
                static const unsigned char data[] = {0x1F,0x11,0x11,0x11,0x11,0x11,0x1F};
                return row < 7 ? data[row] : 0;
            }
        }
    }

public:
    CompactBitmapFont(float scale = 2.0f) : m_scale(scale) {}

    void DrawChar(IDirect3DDevice9* pDevice, float x, float y, char c, DWORD color) {
        for (int row = 0; row < 7; row++) {
            unsigned char charData = GetCharData(c, row);
            for (int col = 0; col < 5; col++) {
                if (charData & (1 << (4 - col))) {
                    DrawPixel(pDevice, x + col * m_scale, y + row * m_scale, m_scale, color);
                }
            }
        }
    }

    void DrawString(IDirect3DDevice9* pDevice, float x, float y, const std::string& text, DWORD color) {
        float currentX = x;

        // Save render states
        DWORD oldLighting, oldZEnable, oldAlphaBlend;
        pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
        pDevice->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);

        // Set states for 2D rendering
        pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

        for (char c : text) {
            DrawChar(pDevice, currentX, y, c, color);
            currentX += 6 * m_scale;  // Character width + spacing
        }

        // Restore states
        pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
        pDevice->SetRenderState(D3DRS_ZENABLE, oldZEnable);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
    }

    float GetCharWidth() const { return 6 * m_scale; }
    float GetCharHeight() const { return 7 * m_scale; }
};