// font_hook_finder.cpp - Find and hook Shovel Knight's text rendering functions
// This mod will help us locate the game's font rendering functions

#include <windows.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <thread>
#include <algorithm>
#include <cstdio>
#include <string>
#include <d3d9.h>
#include <map>

#include "MinHook/include/MinHook.h"
#include <Psapi.h>
#include <set>

// Common function signatures for text rendering
// Track unique strings and their sources
std::set<std::string> g_uniqueStrings;
std::map<void*, std::string> g_functionMap;

// Hook for monitoring string usage with better filtering
void LogStringAccess(const char* str, void* returnAddress) {
    if (!str) return;

    size_t len = strlen(str);
    if (len == 0 || len > 100) return;  // Skip empty or too long strings

    std::string strCopy(str);

    // Skip common system strings
    if (strCopy.find("\\") != std::string::npos ||  // File paths
        strCopy.find(".dll") != std::string::npos ||
        strCopy.find(".exe") != std::string::npos ||
        strCopy.find("CONOUT$") != std::string::npos ||
        strCopy.find("Microsoft") != std::string::npos) {
        return;
    }

    // Check if this is new
    if (g_uniqueStrings.find(strCopy) == g_uniqueStrings.end()) {
        g_uniqueStrings.insert(strCopy);

        // Calculate RVA
        HMODULE hModule = GetModuleHandle(NULL);
        DWORD rva = (DWORD)returnAddress - (DWORD)hModule;

        // Log all new strings (we'll filter interesting ones later)
        std::cout << "[Font Finder] String at RVA 0x" << std::hex << rva << std::dec
                  << ": \"" << strCopy << "\"" << std::endl;

        g_functionMap[returnAddress] = strCopy;
    }
}

// Enhanced strlen hook
typedef size_t (__cdecl* strlen_t)(const char*);
strlen_t o_strlen = nullptr;

size_t __cdecl hk_strlen(const char* str) {
    // Get the return address to see who called strlen
    void* caller = _ReturnAddress();

    // Only log from game code, not system DLLs
    HMODULE hModule = GetModuleHandle(NULL);
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(caller, &mbi, sizeof(mbi))) {
        if (mbi.AllocationBase == hModule) {
            LogStringAccess(str, caller);
        }
    }

    return o_strlen(str);
}

// Hook additional string functions
typedef int (__cdecl* sprintf_t)(char*, const char*, ...);
typedef int (__cdecl* vsprintf_t)(char*, const char*, va_list);
typedef wchar_t* (__cdecl* wcscpy_t)(wchar_t*, const wchar_t*);

sprintf_t o_sprintf = nullptr;
vsprintf_t o_vsprintf = nullptr;
wcscpy_t o_wcscpy = nullptr;

int __cdecl hk_sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = o_vsprintf(buffer, format, args);
    va_end(args);

    // Log the formatted string
    void* caller = _ReturnAddress();
    LogStringAccess(buffer, caller);

    return result;
}

// Hook string functions
void HookStringFunctions() {
    std::cout << "[Font Finder] Hooking string functions..." << std::endl;

    // Get module handles
    HMODULE hMsvcrt = GetModuleHandleA("msvcrt.dll");
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");

    if (hMsvcrt) {
        // Hook strlen
        void* pStrlen = GetProcAddress(hMsvcrt, "strlen");
        if (pStrlen) {
            MH_CreateHook(pStrlen, &hk_strlen, (void**)&o_strlen);
            MH_EnableHook(pStrlen);
        }

        // Hook sprintf
        void* pSprintf = GetProcAddress(hMsvcrt, "sprintf");
        void* pVsprintf = GetProcAddress(hMsvcrt, "vsprintf");
        if (pSprintf && pVsprintf) {
            o_vsprintf = (vsprintf_t)pVsprintf;
            MH_CreateHook(pSprintf, &hk_sprintf, (void**)&o_sprintf);
            MH_EnableHook(pSprintf);
        }
    }

    std::cout << "[Font Finder] String hooks installed" << std::endl;
}

// Pattern scanning for DrawText-like functions
void ScanForTextFunctions() {
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) return;

    MODULEINFO modInfo = {0};
    GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));

    std::cout << "[Font Finder] Scanning for text rendering functions..." << std::endl;

    // Common patterns in games for text rendering
    // Pattern: PUSH string, PUSH Y, PUSH X, CALL function
    BYTE pushStringPattern[] = "\x68\x00\x00\x00\x00\x6A\x00\x6A\x00\xE8";

    DWORD baseAddr = (DWORD)hModule;
    DWORD endAddr = baseAddr + modInfo.SizeOfImage;

    for (DWORD addr = baseAddr; addr < endAddr - 10; addr++) {
        BYTE* bytes = (BYTE*)addr;

        // Look for PUSH immediate (0x68) followed by address in .rdata
        if (bytes[0] == 0x68) {
            DWORD* stringAddr = (DWORD*)(bytes + 1);

            // Verify it points to readable memory
            if (IsBadReadPtr((void*)*stringAddr, 1)) continue;

            // Check if it's a string
            const char* possibleString = (const char*)*stringAddr;
            if (IsBadStringPtrA(possibleString, 100)) continue;

            // Check if next instructions look like coordinate pushes
            if ((bytes[5] == 0x68 || bytes[5] == 0x6A) &&  // PUSH
                (bytes[7] == 0x68 || bytes[7] == 0x6A || bytes[9] == 0x68 || bytes[9] == 0x6A)) {

                // This might be a text drawing call
                std::cout << "[Font Finder] Possible text call at RVA 0x"
                          << std::hex << (addr - baseAddr) << std::dec
                          << " String: \"" << possibleString << "\"" << std::endl;
            }
        }
    }
}

// Main thread
DWORD WINAPI MainThread(LPVOID lpParam) {
    // Initialize console
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Shovel Knight Font Finder");

    std::cout << "==================================================" << std::endl;
    std::cout << "    SHOVEL KNIGHT FONT FINDER V2" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Monitoring string usage in game code..." << std::endl;
    std::cout << "Press F10 for summary, F11 to scan for patterns" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        std::cout << "[Font Finder] Failed to initialize MinHook!" << std::endl;
        return 0;
    }

    // Hook string functions
    HookStringFunctions();

    // Monitor loop
    while (true) {
        if (GetAsyncKeyState(VK_F10) & 1) {
            std::cout << "\n=== STRING SUMMARY ===" << std::endl;
            std::cout << "Unique strings found: " << g_uniqueStrings.size() << std::endl;

            // Show game-related strings
            std::cout << "\nGame strings found:" << std::endl;
            for (const auto& str : g_uniqueStrings) {
                // Filter for likely game strings
                if (str.find("GOLD") != std::string::npos ||
                    str.find("HP") != std::string::npos ||
                    str.find("MP") != std::string::npos ||
                    str.find("Level") != std::string::npos ||
                    str.find("Stage") != std::string::npos ||
                    str.find("Options") != std::string::npos ||
                    str.find("Start Game") != std::string::npos ||
                    str.find("Feats") != std::string::npos ||
                    str.find("Shovel") != std::string::npos ||
                    str.find("Knight") != std::string::npos ||
                    (str.length() > 3 && str.length() < 50 &&
                     str.find_first_of("0123456789") == std::string::npos)) {
                    std::cout << "  \"" << str << "\"" << std::endl;
                }
            }
        }

        if (GetAsyncKeyState(VK_F11) & 1) {
            ScanForTextFunctions();
        }

        Sleep(100);
    }

    return 0;
}

// DLL Entry
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}