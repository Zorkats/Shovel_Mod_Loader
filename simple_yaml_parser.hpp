// simple_yaml_parser.h - Simple YAML parser for BGM database
#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

class SimpleYAMLParser {
public:
    struct BGMEntry {
        std::string name;
        std::string composer;
        int trackNumber;
        bool isMusic;
    };

    static std::map<uint32_t, BGMEntry> LoadBGMDatabase(const std::string& filename) {
        std::map<uint32_t, BGMEntry> database;
        std::ifstream file(filename);

        if (!file.is_open()) {
            std::cout << "[YAML Parser] Failed to open " << filename << std::endl;
            return database;
        }

        std::string line;
        uint32_t currentId = 0;
        BGMEntry currentEntry;
        bool inTrack = false;
        int indentLevel = 0;

        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Count indent level
            int indent = 0;
            while (indent < line.length() && (line[indent] == ' ' || line[indent] == '\t')) {
                indent++;
            }

            // Remove leading/trailing whitespace
            line = line.substr(indent);
            size_t endPos = line.find_last_not_of(" \t\r\n");
            if (endPos != std::string::npos) {
                line = line.substr(0, endPos + 1);
            }

            // Parse hex ID (format: 0x123:)
            if (line.find("0x") == 0 && line.find(':') != std::string::npos) {
                // Save previous entry if exists
                if (inTrack && currentId != 0) {
                    database[currentId] = currentEntry;
                }

                // Parse new ID
                size_t colonPos = line.find(':');
                std::string idStr = line.substr(2, colonPos - 2);  // Skip "0x"
                currentId = std::stoul(idStr, nullptr, 16);

                // Reset entry
                currentEntry = BGMEntry();
                inTrack = true;
            }
            // Parse properties
            else if (inTrack && line.find(':') != std::string::npos) {
                size_t colonPos = line.find(':');
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                // Remove quotes and trim
                size_t firstQuote = value.find('"');
                size_t lastQuote = value.find_last_of('"');
                if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote) {
                    value = value.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                } else {
                    // Trim whitespace for non-quoted values
                    size_t start = value.find_first_not_of(" \t");
                    if (start != std::string::npos) {
                        value = value.substr(start);
                    }
                }

                // Assign to appropriate field
                if (key == "name") {
                    currentEntry.name = value;
                } else if (key == "composer") {
                    currentEntry.composer = value;
                } else if (key == "track_number") {
                    currentEntry.trackNumber = std::stoi(value);
                } else if (key == "is_music") {
                    currentEntry.isMusic = (value == "true");
                }
            }
        }

        // Save last entry
        if (inTrack && currentId != 0) {
            database[currentId] = currentEntry;
        }

        file.close();
        std::cout << "[YAML Parser] Loaded " << database.size() << " BGM entries from " << filename << std::endl;

        return database;
    }
};