// debug_console.h - Enhanced debug console with filtering and commands
#pragma once

#include <windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>

class DebugConsole {
public:
    enum LogLevel {
        LOG_DEBUG = 0,
        LOG_INFO = 1,
        LOG_WARNING = 2,
        LOG_ERROR = 3
    };

    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string module;
        std::string message;
    };

private:
    HANDLE m_hConsole;
    std::deque<LogEntry> m_logBuffer;
    std::mutex m_logMutex;
    LogLevel m_minLevel;
    std::vector<std::string> m_enabledModules;
    bool m_showTimestamps;
    bool m_useColors;
    std::thread m_inputThread;
    bool m_running;

    // Command handlers
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> m_commands;

public:
    DebugConsole() : m_minLevel(LOG_INFO), m_showTimestamps(true),
                     m_useColors(true), m_running(true) {
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);

        m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        SetupConsole();
        RegisterCommands();
        StartInputThread();
    }

    ~DebugConsole() {
        m_running = false;
        if (m_inputThread.joinable()) {
            m_inputThread.join();
        }
        FreeConsole();
    }

    void SetupConsole() {
        SetConsoleTitleA("Shovel Knight Mod Loader - Debug Console");

        // Set console size
        SMALL_RECT windowSize = {0, 0, 119, 29};
        SetConsoleWindowInfo(m_hConsole, TRUE, &windowSize);

        // Set buffer size
        COORD bufferSize = {120, 1000};
        SetConsoleScreenBufferSize(m_hConsole, bufferSize);

        // Enable ANSI escape codes on Windows 10+
        DWORD mode;
        GetConsoleMode(m_hConsole, &mode);
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(m_hConsole, mode);

        PrintHeader();
    }

    void PrintHeader() {
        SetTextColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << R"(
==============================================================
   _____ _                     _   _  __      _       _     _
  / ____| |                   | | | |/ /     (_)     | |   | |
 | (___ | |__   _____   _____| | | ' / _ __  _  __ _| |__ | |_
  \___ \| '_ \ / _ \ \ / / _ \ | |  < | '_ \| |/ _` | '_ \| __|
  ____) | | | | (_) \ V /  __/ | | . \| | | | | (_| | | | | |_
 |_____/|_| |_|\___/ \_/ \___|_| |_|\_\_| |_|_|\__, |_| |_|\__|
                                                 __/ |
  MOD LOADER v2.0 - Enhanced Debug Console      |___/
==============================================================
)" << std::endl;

        SetTextColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << "Type 'help' for available commands" << std::endl;
        std::cout << "==============================================================\n" << std::endl;
    }

    void RegisterCommands() {
        // Help command
        m_commands["help"] = [this](const std::vector<std::string>& args) {
            std::cout << "\nAvailable commands:" << std::endl;
            std::cout << "  help              - Show this help" << std::endl;
            std::cout << "  clear             - Clear console" << std::endl;
            std::cout << "  level <level>     - Set log level (debug/info/warning/error)" << std::endl;
            std::cout << "  filter <module>   - Filter logs by module" << std::endl;
            std::cout << "  timestamp on/off  - Toggle timestamps" << std::endl;
            std::cout << "  colors on/off     - Toggle colors" << std::endl;
            std::cout << "  mods              - List loaded mods" << std::endl;
            std::cout << "  bgm               - Show current BGM info" << std::endl;
            std::cout << "  cheats            - List available cheats" << std::endl;
            std::cout << "  toggle <cheat>    - Toggle a cheat" << std::endl;
            std::cout << "  reload <mod>      - Reload a mod" << std::endl;
            std::cout << "  dump              - Dump game memory info" << std::endl;
            std::cout << "  exit              - Exit mod loader" << std::endl;
        };

        // Clear command
        m_commands["clear"] = [this](const std::vector<std::string>& args) {
            system("cls");
            PrintHeader();
        };

        // Level command
        m_commands["level"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                std::cout << "Usage: level <debug/info/warning/error>" << std::endl;
                return;
            }

            std::string level = args[1];
            if (level == "debug") m_minLevel = LOG_DEBUG;
            else if (level == "info") m_minLevel = LOG_INFO;
            else if (level == "warning") m_minLevel = LOG_WARNING;
            else if (level == "error") m_minLevel = LOG_ERROR;
            else {
                std::cout << "Invalid level. Use: debug, info, warning, or error" << std::endl;
                return;
            }

            std::cout << "Log level set to: " << level << std::endl;
        };

        // Add more commands as needed...
    }

    void StartInputThread() {
        m_inputThread = std::thread([this]() {
            std::string line;
            while (m_running) {
                std::getline(std::cin, line);
                ProcessCommand(line);
            }
        });
    }

    void ProcessCommand(const std::string& line) {
        std::vector<std::string> args;
        std::stringstream ss(line);
        std::string arg;

        while (ss >> arg) {
            args.push_back(arg);
        }

        if (args.empty()) return;

        auto it = m_commands.find(args[0]);
        if (it != m_commands.end()) {
            it->second(args);
        } else {
            std::cout << "Unknown command: " << args[0] << std::endl;
        }
    }

    void Log(LogLevel level, const std::string& module, const std::string& message) {
        if (level < m_minLevel) return;

        // Check module filter
        if (!m_enabledModules.empty()) {
            if (std::find(m_enabledModules.begin(), m_enabledModules.end(), module) == m_enabledModules.end()) {
                return;
            }
        }

        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = level;
        entry.module = module;
        entry.message = message;

        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            m_logBuffer.push_back(entry);

            // Keep buffer size reasonable
            if (m_logBuffer.size() > 1000) {
                m_logBuffer.pop_front();
            }
        }

        PrintLogEntry(entry);
    }

    void PrintLogEntry(const LogEntry& entry) {
        // Set color based on level
        if (m_useColors) {
            switch (entry.level) {
                case LOG_DEBUG:
                    SetTextColor(FOREGROUND_INTENSITY);
                    break;
                case LOG_INFO:
                    SetTextColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                    break;
                case LOG_WARNING:
                    SetTextColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                    break;
                case LOG_ERROR:
                    SetTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
                    break;
            }
        }

        // Print timestamp
        if (m_showTimestamps) {
            auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm tm = *std::localtime(&time_t);
            std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] ";
        }

        // Print level
        const char* levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        std::cout << "[" << levelStr[entry.level] << "] ";

        // Print module
        SetTextColor(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        std::cout << "[" << entry.module << "] ";

        // Print message
        SetTextColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << entry.message << std::endl;
    }

    void SetTextColor(WORD color) {
        SetConsoleTextAttribute(m_hConsole, color);
    }

    void Show() {
        ShowWindow(GetConsoleWindow(), SW_SHOW);
    }

    void Hide() {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }

    // Convenience logging functions
    void Debug(const std::string& module, const std::string& message) {
        Log(LOG_DEBUG, module, message);
    }

    void Info(const std::string& module, const std::string& message) {
        Log(LOG_INFO, module, message);
    }

    void Warning(const std::string& module, const std::string& message) {
        Log(LOG_WARNING, module, message);
    }

    void Error(const std::string& module, const std::string& message) {
        Log(LOG_ERROR, module, message);
    }
};

// Global instance
extern DebugConsole* g_pDebugConsole;

// Initialize global instance in source file
inline DebugConsole* g_pDebugConsole = nullptr;

// Macros for easy logging
#define LOG_DEBUG(module, msg) if(g_pDebugConsole) g_pDebugConsole->Debug(module, msg)
#define LOG_INFO(module, msg) if(g_pDebugConsole) g_pDebugConsole->Info(module, msg)
#define LOG_WARNING(module, msg) if(g_pDebugConsole) g_pDebugConsole->Warning(module, msg)
#define LOG_ERROR(module, msg) if(g_pDebugConsole) g_pDebugConsole->Error(module, msg)