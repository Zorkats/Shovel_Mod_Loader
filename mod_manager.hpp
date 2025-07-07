// mod_manager.h - Enhanced mod management system
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <Psapi.h>
//#include <json/json.h> // You'll need a JSON library or use simple INI

class ModManager {
public:
    struct CheatInfo {
        std::string name;
        std::string description;
        bool enabled;  // Changed from pointer to direct bool
        std::function<void(bool)> onToggle;
        std::string hotkey;
        bool * pEnabled;
    };

    struct ModInfo {
        std::string name;
        std::string version;
        std::string author;
        std::string description;
        HMODULE hModule;
        bool enabled;
        std::map<std::string, std::string> config;
    };

private:
    std::vector<ModInfo> m_loadedMods;
    std::vector<CheatInfo> m_cheats;
    std::map<std::string, std::string> m_globalConfig;
    std::string m_configPath;

    // Store cheat states
    bool m_godMode = false;
    bool m_infiniteGems = false;
    bool m_speedHack = false;
    bool m_noClip = false;

public:
    ModManager() : m_configPath("mods/config.ini") {
        LoadConfig();
        RegisterDefaultCheats();
    }

    void RegisterDefaultCheats() {
        // God Mode
        RegisterCheat("God Mode", "Invincibility",
            [this](bool enabled) {
                m_godMode = enabled;
                if (enabled) {
                    // Hook health functions
                    std::cout << "[Cheat] God Mode enabled" << std::endl;
                }
            }, "F1");

        // Infinite Gems
        RegisterCheat("Infinite Gems", "Unlimited currency",
            [this](bool enabled) {
                m_infiniteGems = enabled;
                if (enabled) {
                    // Hook gem functions
                    std::cout << "[Cheat] Infinite Gems enabled" << std::endl;
                }
            }, "F2");

        // Speed Hack
        RegisterCheat("Speed Hack", "2x movement speed",
            [this](bool enabled) {
                m_speedHack = enabled;
                if (enabled) {
                    // Hook movement functions
                    std::cout << "[Cheat] Speed Hack enabled" << std::endl;
                }
            }, "F3");

        // No Clip
        RegisterCheat("No Clip", "Walk through walls",
            [this](bool enabled) {
                m_noClip = enabled;
                if (enabled) {
                    // Hook collision functions
                    std::cout << "[Cheat] No Clip enabled" << std::endl;
                }
            }, "F4");
    }

    void RegisterCheat(const std::string& name, const std::string& desc,
                      std::function<void(bool)> onToggle,
                      const std::string& hotkey = "") {
        CheatInfo cheat;
        cheat.name = name;
        cheat.description = desc;
        cheat.enabled = false;
        cheat.onToggle = onToggle;
        cheat.hotkey = hotkey;
        m_cheats.push_back(cheat);
    }

    void LoadMods() {
        std::filesystem::path modsDir = "mods";

        if (!std::filesystem::exists(modsDir)) {
            std::filesystem::create_directory(modsDir);
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(modsDir)) {
            if (entry.path().extension() == ".dll") {
                LoadMod(entry.path());
            }
        }
    }

    bool LoadMod(const std::filesystem::path& modPath) {
        // Check if already loaded
        for (const auto& mod : m_loadedMods) {
            if (mod.name == modPath.filename().string()) {
                return false;
            }
        }

        HMODULE hMod = LoadLibraryW(modPath.wstring().c_str());
        if (!hMod) {
            std::cout << "[ModManager] Failed to load: " << modPath << std::endl;
            return false;
        }

        // Get mod info if available
        typedef const char* (*GetModInfo_t)();
        GetModInfo_t GetModInfo = (GetModInfo_t)GetProcAddress(hMod, "GetModInfo");

        ModInfo mod;
        mod.hModule = hMod;
        mod.name = modPath.filename().string();
        mod.enabled = true;

        if (GetModInfo) {
            // Parse mod info JSON or simple format
            std::string info = GetModInfo();
            // Parse info...
        }

        m_loadedMods.push_back(mod);
        std::cout << "[ModManager] Loaded: " << mod.name << std::endl;

        // Call mod initialization
        typedef void (*InitMod_t)();
        InitMod_t InitMod = (InitMod_t)GetProcAddress(hMod, "InitMod");
        if (InitMod) {
            InitMod();
        }

        return true;
    }

    void UnloadMod(const std::string& modName) {
        auto it = std::find_if(m_loadedMods.begin(), m_loadedMods.end(),
            [&](const ModInfo& mod) { return mod.name == modName; });

        if (it != m_loadedMods.end()) {
            // Call cleanup
            typedef void (*CleanupMod_t)();
            CleanupMod_t CleanupMod = (CleanupMod_t)GetProcAddress(it->hModule, "CleanupMod");
            if (CleanupMod) {
                CleanupMod();
            }

            FreeLibrary(it->hModule);
            m_loadedMods.erase(it);
            std::cout << "[ModManager] Unloaded: " << modName << std::endl;
        }
    }

    void SaveConfig() {
        // Simple config saving without JSON library
        std::ofstream file(m_configPath);
        if (!file.is_open()) return;

        file << "# Mod Loader Configuration\n";
        file << "version=2.0\n\n";

        file << "[Global]\n";
        for (const auto& [key, value] : m_globalConfig) {
            file << key << "=" << value << "\n";
        }

        file << "\n[Cheats]\n";
        for (const auto& cheat : m_cheats) {
            file << cheat.name << "=" << (cheat.enabled ? "true" : "false") << "\n";
        }

        file << "\n[Mods]\n";
        for (const auto& mod : m_loadedMods) {
            file << mod.name << "=" << (mod.enabled ? "enabled" : "disabled") << "\n";
        }
    }

    void LoadConfig() {
        // Simple config loading without JSON library
        std::ifstream file(m_configPath);
        if (!file.is_open()) return;

        std::string line;
        std::string section;

        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;

            // Check for section
            if (line[0] == '[' && line.back() == ']') {
                section = line.substr(1, line.length() - 2);
                continue;
            }

            // Parse key=value
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                if (section == "Global") {
                    m_globalConfig[key] = value;
                } else if (section == "Cheats") {
                    for (auto& cheat : m_cheats) {
                        if (cheat.name == key) {
                            cheat.enabled = (value == "true");
                        }
                    }
                }
            }
        }
    }

    // UI Helpers
    void DrawModMenu(IDirect3DDevice9* pDevice, CompactBitmapFont* pFont,
                     void (*DrawRectFunc)(IDirect3DDevice9*, float, float, float, float, DWORD),
                     void (*DrawBorderFunc)(IDirect3DDevice9*, float, float, float, float, float, DWORD)) {
        float x = 50, y = 150;

        // Background
        DrawRectFunc(pDevice, x, y, 400, 300, D3DCOLOR_ARGB(220, 20, 20, 20));
        DrawBorderFunc(pDevice, x, y, 400, 300, 2, D3DCOLOR_ARGB(255, 100, 200, 100));

        pFont->DrawString(pDevice, x + 10, y + 10, "MOD MANAGER", D3DCOLOR_ARGB(255, 100, 255, 100));

        // List loaded mods
        float modY = y + 40;
        pFont->DrawString(pDevice, x + 10, modY, "LOADED MODS:", D3DCOLOR_ARGB(255, 200, 200, 200));
        modY += 20;

        for (const auto& mod : m_loadedMods) {
            std::string status = mod.enabled ? "[ON]" : "[OFF]";
            pFont->DrawString(pDevice, x + 20, modY, mod.name + " " + status,
                            mod.enabled ? D3DCOLOR_ARGB(255, 100, 255, 100) : D3DCOLOR_ARGB(255, 255, 100, 100));
            modY += 15;
        }

        // List cheats
        modY += 20;
        pFont->DrawString(pDevice, x + 10, modY, "CHEATS:", D3DCOLOR_ARGB(255, 200, 200, 200));
        modY += 20;

        for (const auto& cheat : m_cheats) {
            std::string status = cheat.enabled ? "[ON]" : "[OFF]";
            std::string line = cheat.name + " " + status;
            if (!cheat.hotkey.empty()) {
                line += " (" + cheat.hotkey + ")";
            }

            pFont->DrawString(pDevice, x + 20, modY, line,
                            cheat.enabled ? D3DCOLOR_ARGB(255, 100, 255, 100) : D3DCOLOR_ARGB(255, 200, 200, 200));
            modY += 15;
        }
    }

    void ProcessHotkeys() {
        for (auto& cheat : m_cheats) {
            if (!cheat.hotkey.empty()) {
                // Convert hotkey string to VK code
                int vkCode = 0;
                if (cheat.hotkey.length() == 2 && cheat.hotkey[0] == 'F') {
                    int fNum = cheat.hotkey[1] - '0';
                    if (fNum >= 1 && fNum <= 9) {
                        vkCode = VK_F1 + fNum - 1;
                    }
                }

                if (vkCode && GetAsyncKeyState(vkCode) & 1) {
                    cheat.enabled = !cheat.enabled;
                    if (cheat.onToggle) {
                        cheat.onToggle(cheat.enabled);
                    }
                    SaveConfig();
                }
            }
        }
    }

    CheatInfo* GetCheat(const std::string& name) {
        for (auto& cheat : m_cheats) {
            if (cheat.name == name) {
                return &cheat;
            }
        }
        // Return nullptr if not found
        return nullptr;
    }

    bool* GetCheatEnabled(const std::string& name) {
        for (auto& cheat : m_cheats) {
            if (cheat.name == name) {
                return &cheat.enabled;
            }
        }
        // Return pointer to a static false if not found
        static bool falseBool = false;
        return &falseBool;
    }

    void ShowUI() {
        // This would show the mod manager UI
        std::cout << "[ModManager] Showing UI..." << std::endl;
    }

    const std::vector<ModInfo>& GetLoadedMods() const { return m_loadedMods; }
    const std::vector<CheatInfo>& GetCheats() const { return m_cheats; }
};