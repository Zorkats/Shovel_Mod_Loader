// library.cpp - Complete sound hook with discovery mode
#include <windows.h>
#include <iostream>
#include <thread>
#include <iomanip>
#include <string>
#include <map>
#include <set>
#include "MinHook.h"

// --- Globals for our mod ---
static bool g_bModRunning = true;
static HMODULE g_hThisModule = nullptr;
static HMODULE g_hD3D9Proxy = nullptr;

// --- Function pointers to D3D9 proxy exports ---
typedef void (*NotifyBGMPlay_t)(uint32_t soundId);
typedef void (*NotifyBGMStop_t)(uint32_t soundId);
typedef void (*ResetBGMSession_t)();
static NotifyBGMPlay_t NotifyBGMPlay = nullptr;
static NotifyBGMStop_t NotifyBGMStop = nullptr;
static ResetBGMSession_t ResetBGMSession = nullptr;

// --- Discovery mode ---
static bool g_discoveryMode = false;
static std::map<uint32_t, int> g_soundFrequency;
static uint32_t g_lastBGM = 0;

// --- Sound ID Lookup Table (will be replaced by YAML eventually) ---
std::map<uint32_t, std::string> g_SoundNameMap = {
    // This is now just for quick reference in discovery mode
    {0x34D, "Yacht Club Games Logo"},
    {0x34,  "Title Screen Music"},
    {0x49,  "Start Game Sound"},
    {0x38,  "Menu Click"},
    {0x39,  "Menu Back"},
};

// --- Game Function Hooking ---
typedef void (*t_SoundPlayerFunction)();
t_SoundPlayerFunction o_SoundPlayerFunction = nullptr;

// Enhanced IsBGMSound function
bool IsBGMSound(uint32_t soundId) {
    // Known music ranges based on patterns
    return (soundId == 0x34 || soundId == 0x34D) ||           // Title music
           (soundId >= 0x100 && soundId <= 0x108) ||          // Stage themes
           (soundId >= 0x200 && soundId <= 0x209) ||          // Boss themes
           (soundId >= 0x300 && soundId <= 0x305) ||          // Special music
           (soundId >= 0x400 && soundId <= 0x402);            // Village themes
}

// This is our C++ function that handles the logging
void LogSoundId(uint32_t soundId) {
    if (soundId == 0) return;

    // Track frequency
    g_soundFrequency[soundId]++;

    // In discovery mode, log everything
    if (g_discoveryMode) {
        std::cout << "[Sound Discovery] ID: 0x" << std::hex << soundId << std::dec
                  << " (Count: " << g_soundFrequency[soundId] << ")";

        // Add category hints
        if (soundId < 0x30) {
            std::cout << " [Player/System SFX]";
        } else if (soundId >= 0x30 && soundId < 0x100) {
            std::cout << " [Menu/UI]";
        } else if (soundId >= 0x100 && soundId < 0x200) {
            std::cout << " [Stage Music?]";
        } else if (soundId >= 0x200 && soundId < 0x300) {
            std::cout << " [Boss Music?]";
        } else if (soundId >= 0x300 && soundId < 0x400) {
            std::cout << " [Special Music?]";
        } else if (soundId >= 0x400 && soundId < 0x500) {
            std::cout << " [Village/Shop?]";
        } else if (soundId >= 0x500 && soundId < 0x600) {
            std::cout << " [Enemy SFX?]";
        } else if (soundId >= 0x600 && soundId < 0x700) {
            std::cout << " [Environment SFX?]";
        }

        std::cout << std::endl;
    }

    // Always notify D3D9 proxy (it handles filtering)
    if (NotifyBGMPlay) {
        NotifyBGMPlay(soundId);
    }

    // Check for title screen to reset session
    if (soundId == 0x34 && g_lastBGM != 0x34 && ResetBGMSession) {
        std::cout << "[Sound Hook] Title screen detected - resetting BGM session" << std::endl;
        ResetBGMSession();
    }

    // Update last BGM if it's music
    if (IsBGMSound(soundId)) {
        g_lastBGM = soundId;
    }
}

// Naked detour function
__declspec(naked) void Detour_SoundPlayerFunction()
{
    __asm {
        // Save all registers
        pushad

        // Pass sound ID to our function
        push ecx
        call LogSoundId
        add esp, 4

        // Restore all registers
        popad

        // Jump to original function
        jmp o_SoundPlayerFunction
    }
}

// --- Main Mod Logic ---
void InitializeMod()
{
    std::cout << "\n==============================================\n"
              << "  SHOVEL KNIGHT SOUND HOOK V2\n"
              << "==============================================\n";

    // Try to get D3D9 proxy exports
    g_hD3D9Proxy = GetModuleHandleA("d3d9.dll");
    if (g_hD3D9Proxy) {
        NotifyBGMPlay = (NotifyBGMPlay_t)GetProcAddress(g_hD3D9Proxy, "NotifyBGMPlay");
        NotifyBGMStop = (NotifyBGMStop_t)GetProcAddress(g_hD3D9Proxy, "NotifyBGMStop");
        ResetBGMSession = (ResetBGMSession_t)GetProcAddress(g_hD3D9Proxy, "ResetBGMSession");

        if (NotifyBGMPlay && NotifyBGMStop) {
            std::cout << "[Sound Hook] Successfully linked to D3D9 proxy overlay!\n";
        } else {
            std::cout << "[Sound Hook] D3D9 proxy found but BGM functions not available\n";
        }
    } else {
        std::cout << "[Sound Hook] D3D9 proxy not found, overlay integration disabled\n";
    }

    if (MH_Initialize() != MH_OK) {
        std::cout << "[Sound Hook] Failed to initialize MinHook!\n";
        return;
    }
    std::cout << "[Sound Hook] MinHook Initialized.\n";

    HMODULE hShovelKnight = GetModuleHandleA("ShovelKnight.exe");
    if (!hShovelKnight) {
        std::cout << "[Sound Hook] Failed to get ShovelKnight.exe module!\n";
        return;
    }

    // RVA for sound function
    constexpr uintptr_t SOUND_PLAYER_RVA = 0xA580;
    LPVOID pTarget = reinterpret_cast<LPVOID>(reinterpret_cast<uintptr_t>(hShovelKnight) + SOUND_PLAYER_RVA);
    std::cout << "[Sound Hook] Target address: 0x" << std::hex << pTarget << std::dec << "\n";

    if (MH_CreateHook(pTarget, &Detour_SoundPlayerFunction, reinterpret_cast<LPVOID*>(&o_SoundPlayerFunction)) != MH_OK) {
        std::cout << "[Sound Hook] Failed to create hook!\n";
        return;
    }
    if (MH_EnableHook(pTarget) != MH_OK) {
        std::cout << "[Sound Hook] Failed to enable hook!\n";
        return;
    }

    std::cout << "[Sound Hook] Sound function hooked successfully!\n";
    std::cout << "[Sound Hook] BGM info will appear in the overlay (bottom-right)\n";
    std::cout << "[Sound Hook] Press INSERT to open the mod menu\n";
    std::cout << "\n=== DISCOVERY MODE CONTROLS ===\n";
    std::cout << "F7 - Toggle discovery mode (log all sounds)\n";
    std::cout << "F8 - Show sound frequency report\n";
    std::cout << "F9 - Unload mod\n";
    std::cout << "==============================================\n\n";
}

void ShowFrequencyReport() {
    std::cout << "\n=== SOUND FREQUENCY REPORT ===" << std::endl;

    // Separate sounds by frequency
    std::cout << "\nRARELY PLAYED (1-3 times) - Likely music or special sounds:" << std::endl;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second >= 1 && pair.second <= 3) {
            std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                      << ": " << pair.second << " times";
            if (g_SoundNameMap.count(pair.first)) {
                std::cout << " [" << g_SoundNameMap[pair.first] << "]";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\nOCCASIONALLY PLAYED (4-10 times) - Likely menu sounds or rare SFX:" << std::endl;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second >= 4 && pair.second <= 10) {
            std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                      << ": " << pair.second << " times" << std::endl;
        }
    }

    std::cout << "\nFREQUENTLY PLAYED (11-50 times) - Likely common SFX:" << std::endl;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second >= 11 && pair.second <= 50) {
            std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                      << ": " << pair.second << " times" << std::endl;
        }
    }

    std::cout << "\nVERY FREQUENTLY PLAYED (>50 times) - Definitely SFX:" << std::endl;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second > 50) {
            std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                      << ": " << pair.second << " times" << std::endl;
        }
    }

    std::cout << "\nTOTAL UNIQUE SOUNDS: " << g_soundFrequency.size() << std::endl;
    std::cout << "================================\n" << std::endl;
}

void CleanupMod()
{
    std::cout << "\n[Sound Hook] Unloading...\n";

    // Notify proxy that music stopped
    if (g_lastBGM != 0 && NotifyBGMStop) {
        NotifyBGMStop(g_lastBGM);
        g_lastBGM = 0;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    std::cout << "[Sound Hook] Cleanup complete.\n";
}

// --- Main Thread and DLL Entry Point ---
DWORD WINAPI ModMainThread(LPVOID lpParam)
{
    g_hThisModule = reinterpret_cast<HMODULE>(lpParam);
    InitializeMod();

    while (g_bModRunning) {
        Sleep(100);

        // F7 - Toggle discovery mode
        if (GetAsyncKeyState(VK_F7) & 1) {
            g_discoveryMode = !g_discoveryMode;
            std::cout << "\n[Sound Hook] Discovery mode: " << (g_discoveryMode ? "ON" : "OFF") << std::endl;
            if (g_discoveryMode) {
                std::cout << "All sounds will now be logged with frequency tracking." << std::endl;
            } else {
                std::cout << "Only BGM changes will be logged." << std::endl;
            }
        }

        // F8 - Show frequency report
        if (GetAsyncKeyState(VK_F8) & 1) {
            ShowFrequencyReport();
        }

        // F9 - Unload mod
        if (GetAsyncKeyState(VK_F9) & 1) {
            std::cout << "[Sound Hook] F9 pressed, unloading mod...\n";
            g_bModRunning = false;
        }
    }

    CleanupMod();
    FreeLibraryAndExitThread(g_hThisModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, ModMainThread, hModule, 0, nullptr));
    }
    return TRUE;
}