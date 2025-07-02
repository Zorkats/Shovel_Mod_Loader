// d3d9_text_interceptor.cpp - Enhanced version with detailed font info
#include <algorithm>
#include <windows.h>
#include <d3d9.h>
#include <iostream>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include "minhook/include/MinHook.h"

#pragma comment(lib, "d3d9.lib")

// Text rendering tracking
struct TextureInfo {
    IDirect3DTexture9* pTexture;
    DWORD width;
    DWORD height;
    D3DFORMAT format;
    DWORD firstSeen;
    int useCount;
    bool likelyFont;
    void* creationCallstack[5];  // Track where it was created
};

struct DrawCall {
    IDirect3DTexture9* pTexture;
    D3DPRIMITIVETYPE primitiveType;
    UINT primitiveCount;
    DWORD fvf;
    void* returnAddress;
    float minX, minY, maxX, maxY;  // Bounding box
};

std::map<IDirect3DTexture9*, TextureInfo> g_textureMap;
std::vector<DrawCall> g_recentDrawCalls;
std::map<void*, int> g_drawCallAddresses;  // Track unique draw call locations

// D3D9 function types
typedef HRESULT (STDMETHODCALLTYPE *DrawPrimitiveUP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
typedef HRESULT (STDMETHODCALLTYPE *DrawIndexedPrimitiveUP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);
typedef HRESULT (STDMETHODCALLTYPE *SetTexture_t)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
typedef HRESULT (STDMETHODCALLTYPE *SetFVF_t)(IDirect3DDevice9*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *CreateTexture_t)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);

// Original functions
DrawPrimitiveUP_t o_DrawPrimitiveUP = nullptr;
DrawIndexedPrimitiveUP_t o_DrawIndexedPrimitiveUP = nullptr;
SetTexture_t o_SetTexture = nullptr;
SetFVF_t o_SetFVF = nullptr;
CreateTexture_t o_CreateTexture = nullptr;

// State tracking
IDirect3DTexture9* g_currentTexture = nullptr;
DWORD g_currentFVF = 0;
bool g_detailedLogging = false;
bool g_logNextDraw = false;
DWORD g_frameCount = 0;

// Helper to get module and RVA
void GetAddressInfo(void* address, std::string& module, DWORD& rva) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi))) {
        HMODULE hModule = (HMODULE)mbi.AllocationBase;
        char moduleName[MAX_PATH] = {0};
        if (GetModuleFileNameA(hModule, moduleName, MAX_PATH)) {
            // Get just the filename
            char* lastSlash = strrchr(moduleName, '\\');
            module = lastSlash ? lastSlash + 1 : moduleName;
            rva = (DWORD)address - (DWORD)hModule;
        }
    }
}

// Format helper
std::string FormatD3DFormat(D3DFORMAT format) {
    switch(format) {
        case D3DFMT_A8R8G8B8: return "A8R8G8B8 (32-bit ARGB)";
        case D3DFMT_X8R8G8B8: return "X8R8G8B8 (32-bit RGB)";
        case D3DFMT_R5G6B5: return "R5G6B5 (16-bit)";
        case D3DFMT_A8: return "A8 (8-bit alpha)";
        case D3DFMT_L8: return "L8 (8-bit luminance)";
        case D3DFMT_DXT1: return "DXT1 (compressed)";
        case D3DFMT_DXT3: return "DXT3 (compressed)";
        case D3DFMT_DXT5: return "DXT5 (compressed)";
        default: return "Format " + std::to_string(format);
    }
}

// Analyze vertex data for text characteristics
bool AnalyzeTextVertices(const void* pVertexData, UINT PrimitiveCount, UINT VertexStreamZeroStride, DrawCall& call) {
    if (!pVertexData || !g_currentTexture) return false;

    // Check FVF
    bool has2DPos = (g_currentFVF & D3DFVF_XYZRHW) != 0;
    bool hasTex = (g_currentFVF & D3DFVF_TEX1) != 0;
    bool hasColor = (g_currentFVF & D3DFVF_DIFFUSE) != 0;

    if (!has2DPos || !hasTex) return false;

    // Calculate vertex offsets
    DWORD posOffset = 0;
    DWORD colorOffset = has2DPos ? 16 : 12;  // After XYZRHW
    DWORD texOffset = colorOffset + (hasColor ? 4 : 0);

    // Get bounding box
    call.minX = call.minY = 99999.0f;
    call.maxX = call.maxY = -99999.0f;

    const BYTE* vertices = (const BYTE*)pVertexData;
    UINT vertexCount = (PrimitiveCount + 2);  // For triangle strip

    for (UINT i = 0; i < vertexCount && i < 4; i++) {
        const BYTE* vertex = vertices + (i * VertexStreamZeroStride);
        float x = *(float*)(vertex + posOffset);
        float y = *(float*)(vertex + posOffset + 4);

        call.minX = min(call.minX, x);
        call.minY = min(call.minY, y);
        call.maxX = max(call.maxX, x);
        call.maxY = max(call.maxY, y);
    }

    // Check if size is reasonable for text (not too big, not too small)
    float width = call.maxX - call.minX;
    float height = call.maxY - call.minY;

    if (width > 0 && width < 500 && height > 0 && height < 100) {
        // Reasonable size for text
        return true;
    }

    return false;
}

// Hook CreateTexture to track texture creation
HRESULT STDMETHODCALLTYPE hkCreateTexture(
    IDirect3DDevice9* pDevice,
    UINT Width,
    UINT Height,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle) {

    HRESULT hr = o_CreateTexture(pDevice, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

    if (SUCCEEDED(hr) && ppTexture && *ppTexture) {
        TextureInfo info;
        info.pTexture = *ppTexture;
        info.width = Width;
        info.height = Height;
        info.format = Format;
        info.firstSeen = GetTickCount();
        info.useCount = 0;

        // Check if it's likely a font texture
        bool isPowerOf2 = (Width & (Width - 1)) == 0 && (Height & (Height - 1)) == 0;
        bool isAlphaFormat = (Format == D3DFMT_A8R8G8B8 || Format == D3DFMT_A8 || Format == D3DFMT_L8);
        info.likelyFont = isPowerOf2 && isAlphaFormat && Width >= 128 && Width <= 1024;

        // Capture callstack
        void* returnAddress = _ReturnAddress();
        info.creationCallstack[0] = returnAddress;

        g_textureMap[*ppTexture] = info;

        if (info.likelyFont || g_detailedLogging) {
            std::string module;
            DWORD rva;
            GetAddressInfo(returnAddress, module, rva);

            std::cout << "[Text Interceptor] Texture created: "
                      << Width << "x" << Height
                      << ", " << FormatD3DFormat(Format)
                      << ", Pool: " << Pool
                      << ", Likely font: " << (info.likelyFont ? "YES" : "NO")
                      << "\n                 Created from: " << module << "+0x" << std::hex << rva << std::dec
                      << " (Address: 0x" << std::hex << returnAddress << std::dec << ")"
                      << std::endl;
        }
    }

    return hr;
}

// Hook DrawPrimitiveUP
HRESULT STDMETHODCALLTYPE hkDrawPrimitiveUP(
    IDirect3DDevice9* pDevice,
    D3DPRIMITIVETYPE PrimitiveType,
    UINT PrimitiveCount,
    const void* pVertexStreamZeroData,
    UINT VertexStreamZeroStride) {

    void* returnAddress = _ReturnAddress();

    // Analyze draw call
    if (g_currentTexture && (PrimitiveType == D3DPT_TRIANGLESTRIP || PrimitiveType == D3DPT_TRIANGLELIST)) {
        DrawCall call;
        call.pTexture = g_currentTexture;
        call.primitiveType = PrimitiveType;
        call.primitiveCount = PrimitiveCount;
        call.fvf = g_currentFVF;
        call.returnAddress = returnAddress;

        bool isTextLike = AnalyzeTextVertices(pVertexStreamZeroData, PrimitiveCount, VertexStreamZeroStride, call);

        if (isTextLike || g_logNextDraw) {
            g_recentDrawCalls.push_back(call);
            g_drawCallAddresses[returnAddress]++;

            // Check if texture is known
            auto texIt = g_textureMap.find(g_currentTexture);
            if (texIt != g_textureMap.end()) {
                texIt->second.useCount++;

                if (texIt->second.likelyFont || g_detailedLogging) {
                    std::string module;
                    DWORD rva;
                    GetAddressInfo(returnAddress, module, rva);

                    std::cout << "[Text Interceptor] Potential text rendering:"
                              << "\n  Texture: " << g_currentTexture
                              << " (" << texIt->second.width << "x" << texIt->second.height << ")"
                              << "\n  Position: " << call.minX << "," << call.minY
                              << " Size: " << (call.maxX - call.minX) << "x" << (call.maxY - call.minY)
                              << "\n  FVF: 0x" << std::hex << g_currentFVF << std::dec
                              << " (2D:" << ((g_currentFVF & D3DFVF_XYZRHW) ? "Y" : "N")
                              << " Tex:" << ((g_currentFVF & D3DFVF_TEX1) ? "Y" : "N")
                              << " Color:" << ((g_currentFVF & D3DFVF_DIFFUSE) ? "Y" : "N") << ")"
                              << "\n  Called from: " << module << "+0x" << std::hex << rva << std::dec
                              << " (Address: 0x" << std::hex << returnAddress << std::dec << ")"
                              << "\n  Call count from this address: " << g_drawCallAddresses[returnAddress]
                              << std::endl;
                }
            }

            g_logNextDraw = false;
        }
    }

    return o_DrawPrimitiveUP(pDevice, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

// Hook SetTexture
HRESULT STDMETHODCALLTYPE hkSetTexture(
    IDirect3DDevice9* pDevice,
    DWORD Stage,
    IDirect3DBaseTexture9* pTexture) {

    if (Stage == 0) {
        g_currentTexture = (IDirect3DTexture9*)pTexture;
    }

    return o_SetTexture(pDevice, Stage, pTexture);
}

// Hook SetFVF
HRESULT STDMETHODCALLTYPE hkSetFVF(IDirect3DDevice9* pDevice, DWORD FVF) {
    g_currentFVF = FVF;
    return o_SetFVF(pDevice, FVF);
}

// Hook device methods
void HookDeviceMethods(IDirect3DDevice9* pDevice) {
    if (!pDevice) return;

    void** vtable = *(void***)pDevice;

    std::cout << "[Text Interceptor] Hooking device methods..." << std::endl;
    std::cout << "[Text Interceptor] Device vtable at: 0x" << std::hex << vtable << std::dec << std::endl;

    // Hook CreateTexture (23)
    MH_CreateHook(vtable[23], &hkCreateTexture, (void**)&o_CreateTexture);
    MH_EnableHook(vtable[23]);

    // Hook DrawPrimitiveUP (83)
    MH_CreateHook(vtable[83], &hkDrawPrimitiveUP, (void**)&o_DrawPrimitiveUP);
    MH_EnableHook(vtable[83]);

    // Hook SetTexture (65)
    MH_CreateHook(vtable[65], &hkSetTexture, (void**)&o_SetTexture);
    MH_EnableHook(vtable[65]);

    // Hook SetFVF (89)
    MH_CreateHook(vtable[89], &hkSetFVF, (void**)&o_SetFVF);
    MH_EnableHook(vtable[89]);

    std::cout << "[Text Interceptor] Device methods hooked!" << std::endl;
}

// Print summary
void PrintSummary() {
    std::cout << "\n=== TEXT INTERCEPTOR SUMMARY ===" << std::endl;
    std::cout << "Textures tracked: " << g_textureMap.size() << std::endl;
    std::cout << "Recent draw calls: " << g_recentDrawCalls.size() << std::endl;
    std::cout << "Unique draw locations: " << g_drawCallAddresses.size() << std::endl;

    std::cout << "\n--- LIKELY FONT TEXTURES ---" << std::endl;
    for (const auto& pair : g_textureMap) {
        const auto& info = pair.second;
        if (info.likelyFont && info.useCount > 0) {
            std::cout << "Texture: 0x" << std::hex << info.pTexture << std::dec
                      << " | " << info.width << "x" << info.height
                      << " | " << FormatD3DFormat(info.format)
                      << " | Used: " << info.useCount << " times" << std::endl;

            // Show creation location
            std::string module;
            DWORD rva;
            GetAddressInfo(info.creationCallstack[0], module, rva);
            std::cout << "  Created from: " << module << "+0x" << std::hex << rva << std::dec << std::endl;
        }
    }

    std::cout << "\n--- FREQUENT DRAW LOCATIONS ---" << std::endl;
    std::vector<std::pair<void*, int>> sortedCalls;
    for (const auto& pair : g_drawCallAddresses) {
        sortedCalls.push_back(pair);
    }
    std::sort(sortedCalls.begin(), sortedCalls.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    int count = 0;
    for (const auto& pair : sortedCalls) {
        if (count++ >= 10) break;  // Top 10

        std::string module;
        DWORD rva;
        GetAddressInfo(pair.first, module, rva);

        std::cout << module << "+0x" << std::hex << rva << std::dec
                  << " (0x" << std::hex << pair.first << std::dec << ")"
                  << " | Calls: " << pair.second << std::endl;
    }
}

// Export function
extern "C" {
    __declspec(dllexport) void HookTextRendering(IDirect3DDevice9* pDevice) {
        static bool hooked = false;
        if (!hooked && pDevice) {
            HookDeviceMethods(pDevice);
            hooked = true;
        }
    }

    __declspec(dllexport) void EnableDetailedLogging(bool enable) {
        g_detailedLogging = enable;
        std::cout << "[Text Interceptor] Detailed logging: " << (enable ? "ON" : "OFF") << std::endl;
    }

    __declspec(dllexport) void LogNextDrawCall() {
        g_logNextDraw = true;
        std::cout << "[Text Interceptor] Will log next draw call..." << std::endl;
    }
}

// Main thread
DWORD WINAPI MainThread(LPVOID) {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Shovel Knight Text Interceptor");

    std::cout << "==================================================" << std::endl;
    std::cout << "    SHOVEL KNIGHT TEXT INTERCEPTOR V2" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "F3 - Toggle detailed logging" << std::endl;
    std::cout << "F4 - Log next draw call" << std::endl;
    std::cout << "F5 - Show summary" << std::endl;
    std::cout << "==================================================" << std::endl;

    if (MH_Initialize() != MH_OK) {
        std::cout << "[Text Interceptor] Failed to initialize MinHook!" << std::endl;
        return 0;
    }

    std::cout << "[Text Interceptor] Ready. Waiting for device..." << std::endl;

    while (true) {
        if (GetAsyncKeyState(VK_F3) & 1) {
            g_detailedLogging = !g_detailedLogging;
            std::cout << "[Text Interceptor] Detailed logging: "
                      << (g_detailedLogging ? "ON" : "OFF") << std::endl;
        }

        if (GetAsyncKeyState(VK_F4) & 1) {
            g_logNextDraw = true;
            std::cout << "[Text Interceptor] Will log next draw call..." << std::endl;
        }

        if (GetAsyncKeyState(VK_F5) & 1) {
            PrintSummary();
        }

        Sleep(100);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}