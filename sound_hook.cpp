// sound_hook.cpp - Enhanced sound hook with BGM System integration
#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <iomanip>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <vector>

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
static std::set<uint32_t> g_recentSounds;
static DWORD g_lastClearTime = 0;

// --- Game Function Hooking ---
typedef void (*t_SoundPlayerFunction)();
t_SoundPlayerFunction o_SoundPlayerFunction = nullptr;

// This is our C++ function that handles the logging
void LogSoundId(uint32_t soundId) {
    if (soundId == 0) return;

    // Track frequency
    g_soundFrequency[soundId]++;

    // Track recent sounds (for pattern detection)
    g_recentSounds.insert(soundId);

    // Clear recent sounds every 5 seconds
    DWORD currentTime = GetTickCount();
    if (currentTime - g_lastClearTime > 5000) {
        g_recentSounds.clear();
        g_lastClearTime = currentTime;
    }

    // Always notify D3D9 proxy - it has the enhanced BGM system
    if (NotifyBGMPlay) {
        NotifyBGMPlay(soundId);
    }

    // In discovery mode, log with more context
    if (g_discoveryMode) {
        std::cout << "[Sound Discovery] ID: 0x" << std::hex << soundId << std::dec
                  << " (Count: " << g_soundFrequency[soundId] << ")";

        // Pattern detection
        if (g_soundFrequency[soundId] == 1) {
            std::cout << " [FIRST TIME]";
        } else if (g_soundFrequency[soundId] <= 3) {
            std::cout << " [RARE - Possibly Music]";
        } else if (g_soundFrequency[soundId] <= 10) {
            std::cout << " [UNCOMMON]";
        } else if (g_soundFrequency[soundId] <= 50) {
            std::cout << " [COMMON]";
        } else {
            std::cout << " [VERY COMMON - Likely SFX]";
        }

        // Context hints based on recent sounds
        if (g_recentSounds.size() <= 3 && g_soundFrequency[soundId] <= 2) {
            std::cout << " [Isolated play - Strong music candidate]";
        }

        std::cout << std::endl;
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

// Pattern analysis for discovery mode
void AnalyzePatterns() {
    std::cout << "\n=== PATTERN ANALYSIS ===" << std::endl;

    // Find sounds that play in isolation (likely music)
    std::cout << "\nISOLATED SOUNDS (likely music):" << std::endl;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second >= 1 && pair.second <= 3) {
            std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                      << " - Played " << pair.second << " times" << std::endl;
        }
    }

    // Find sounds that play in specific ranges
    std::cout << "\nSOUNDS BY ID RANGE:" << std::endl;
    std::map<std::string, std::vector<uint32_t>> rangeMap;

    for (const auto& pair : g_soundFrequency) {
        uint32_t id = pair.first;
        if (id < 0x100) {
            rangeMap["System/Menu (0x000-0x0FF)"].push_back(id);
        } else if (id >= 0x100 && id < 0x200) {
            rangeMap["Stage Music (0x100-0x1FF)"].push_back(id);
        } else if (id >= 0x200 && id < 0x300) {
            rangeMap["Boss Music (0x200-0x2FF)"].push_back(id);
        } else if (id >= 0x300 && id < 0x400) {
            rangeMap["Special Music (0x300-0x3FF)"].push_back(id);
        } else if (id >= 0x400 && id < 0x500) {
            rangeMap["Location Music (0x400-0x4FF)"].push_back(id);
        } else if (id >= 0x500 && id < 0x600) {
            rangeMap["Enemy SFX (0x500-0x5FF)"].push_back(id);
        } else if (id >= 0x600 && id < 0x700) {
            rangeMap["Environment SFX (0x600-0x6FF)"].push_back(id);
        } else {
            rangeMap["Other"].push_back(id);
        }
    }

    for (const auto& [range, ids] : rangeMap) {
        if (!ids.empty()) {
            std::cout << "\n" << range << ":" << std::endl;
            for (uint32_t id : ids) {
                std::cout << "  0x" << std::hex << id << std::dec
                          << " (" << g_soundFrequency[id] << " plays)" << std::endl;
            }
        }
    }
}

// --- Main Mod Logic ---
void InitializeMod()
{
    std::cout << "\n==============================================\n"
              << "  SHOVEL KNIGHT SOUND HOOK V3\n"
              << "  Enhanced with Pattern Detection\n"
              << "==============================================\n";

    // Try to get D3D9 proxy exports
    g_hD3D9Proxy = GetModuleHandleA("d3d9.dll");
    if (g_hD3D9Proxy) {
        NotifyBGMPlay = (NotifyBGMPlay_t)GetProcAddress(g_hD3D9Proxy, "NotifyBGMPlay");
        NotifyBGMStop = (NotifyBGMStop_t)GetProcAddress(g_hD3D9Proxy, "NotifyBGMStop");
        ResetBGMSession = (ResetBGMSession_t)GetProcAddress(g_hD3D9Proxy, "ResetBGMSession");

        if (NotifyBGMPlay && NotifyBGMStop) {
            std::cout << "[Sound Hook] Successfully linked to D3D9 proxy!\n";
            std::cout << "[Sound Hook] BGM System integration active\n";
        } else {
            std::cout << "[Sound Hook] D3D9 proxy found but BGM functions not available\n";
        }
    } else {
        std::cout << "[Sound Hook] D3D9 proxy not found, running standalone\n";
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
    std::cout << "\n=== ENHANCED FEATURES ===\n";
    std::cout << "- BGM info appears in overlay (bottom-right)\n";
    std::cout << "- Press F6 for permanent BGM display\n";
    std::cout << "- Press F7 for discovery mode\n";
    std::cout << "- Press F8 for statistics\n";
    std::cout << "\n=== SOUND HOOK CONTROLS ===\n";
    std::cout << "F7 - Toggle discovery mode (logs all sounds)\n";
    std::cout << "F8 - Show sound frequency report\n";
    std::cout << "F9 - Pattern analysis\n";
    std::cout << "F10 - Export discovered sounds\n";
    std::cout << "F11 - Unload mod\n";
    std::cout << "==============================================\n\n";
}

void ShowFrequencyReport() {
    std::cout << "\n=== SOUND FREQUENCY REPORT ===" << std::endl;
    std::cout << "Total unique sounds: " << g_soundFrequency.size() << std::endl;

    // Separate sounds by frequency
    std::cout << "\nRARELY PLAYED (1-3 times) - Likely music or special sounds:" << std::endl;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second >= 1 && pair.second <= 3) {
            std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                      << ": " << pair.second << " times" << std::endl;
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
    int sfxCount = 0;
    for (const auto& pair : g_soundFrequency) {
        if (pair.second > 50) {
            if (sfxCount < 20) {  // Limit output
                std::cout << "  ID 0x" << std::hex << pair.first << std::dec
                          << ": " << pair.second << " times" << std::endl;
            }
            sfxCount++;
        }
    }
    if (sfxCount > 20) {
        std::cout << "  ... and " << (sfxCount - 20) << " more" << std::endl;
    }

    std::cout << "================================\n" << std::endl;
}

void ExportDiscoveredSounds() {
    std::ofstream file("discovered_sounds.yaml");
    if (!file.is_open()) {
        std::cout << "[Sound Hook] Failed to create export file!" << std::endl;
        return;
    }

    file << "# Discovered Sounds - Generated by Sound Hook\n";
    file << "# " << g_soundFrequency.size() << " unique sounds found\n\n";

    // Export rare sounds as potential music
    file << "# POTENTIAL MUSIC (played 1-3 times)\n";
    for (const auto& pair : g_soundFrequency) {
        if (pair.second >= 1 && pair.second <= 3) {
            file << std::hex << "0x" << pair.first << ":\n";
            file << "  name: \"Unknown Music " << pair.first << "\"\n";
            file << "  composer: \"Jake Kaufman\"\n";
            file << "  track_number: -1\n";
            file << "  is_music: true\n";
            file << "  play_count: " << std::dec << pair.second << "\n\n";
        }
    }

    // Export common sounds as SFX
    file << "\n# SOUND EFFECTS (played >10 times)\n";
    for (const auto& pair : g_soundFrequency) {
        if (pair.second > 10 && pair.second <= 100) {
            file << std::hex << "0x" << pair.first << ":\n";
            file << "  name: \"SFX " << pair.first << "\"\n";
            file << "  composer: \"\"\n";
            file << "  track_number: -1\n";
            file << "  is_music: false\n";
            file << "  play_count: " << std::dec << pair.second << "\n\n";
        }
    }

    file.close();
    std::cout << "[Sound Hook] Exported discovered sounds to discovered_sounds.yaml" << std::endl;
}

void CleanupMod()
{
    std::cout << "\n[Sound Hook] Unloading...\n";

    // Export discoveries before closing
    if (!g_soundFrequency.empty()) {
        ExportDiscoveredSounds();
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

    g_lastClearTime = GetTickCount();

    while (g_bModRunning) {
        Sleep(100);

        // F7 - Toggle discovery mode
        if (GetAsyncKeyState(VK_F7) & 1) {
            g_discoveryMode = !g_discoveryMode;
            std::cout << "\n[Sound Hook] Discovery mode: " << (g_discoveryMode ? "ON" : "OFF") << std::endl;
            if (g_discoveryMode) {
                std::cout << "All sounds will now be logged with enhanced analysis." << std::endl;
            }
        }

        // F8 - Show frequency report
        if (GetAsyncKeyState(VK_F8) & 1) {
            ShowFrequencyReport();
        }

        // F9 - Pattern analysis
        if (GetAsyncKeyState(VK_F9) & 1) {
            AnalyzePatterns();
        }

        // F10 - Export discovered sounds
        if (GetAsyncKeyState(VK_F10) & 1) {
            ExportDiscoveredSounds();
        }

        // F11 - Unload mod
        if (GetAsyncKeyState(VK_F11) & 1) {
            std::cout << "[Sound Hook] F11 pressed, unloading mod...\n";
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