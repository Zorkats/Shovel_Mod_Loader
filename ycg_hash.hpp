// ycg_hash.hpp - Yacht Club Games hash function implementation
#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include <iostream>
#include <vector>

class YCGHash {
public:
    // Rotate left
    static uint32_t ROL(uint32_t n, uint32_t d) {
        return ((n << d) | (n >> (32 - d))) & 0xFFFFFFFF;
    }

    // Rotate right
    static uint32_t ROR(uint32_t n, uint32_t d) {
        return ((n >> d) | (n << (32 - d))) & 0xFFFFFFFF;
    }

    // Main hash function - implementation of Bob Jenkins' one-at-a-time hash
    // Used by Yacht Club Games for various identifiers
    static uint32_t Hash(const std::string& str, uint32_t initialHash = 123456789) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
        size_t length = str.length();
        size_t index = 0;

        uint32_t hashA = (length + initialHash + 0xDEADBEEF) & 0xFFFFFFFF;
        uint32_t hashB = (length + initialHash + 0xDEADBEEF) & 0xFFFFFFFF;
        uint32_t hashC = (length + initialHash + 0xDEADBEEF) & 0xFFFFFFFF;

        // Process 12-byte chunks
        while (length > 12) {
            uint32_t sHashA = hashA;
            uint32_t sHashB = hashB;
            uint32_t sHashC = hashC;

            // Read 4 bytes into C
            for (int i = 0; i < 4 && length > 0; i++) {
                sHashC += static_cast<uint32_t>(data[index++]) << ((i & 3) << 3);
                sHashC &= 0xFFFFFFFF;
                length--;
            }

            // Read 4 bytes into B
            for (int i = 0; i < 4 && length > 0; i++) {
                sHashB += static_cast<uint32_t>(data[index++]) << ((i & 3) << 3);
                sHashB &= 0xFFFFFFFF;
                length--;
            }

            // Read 4 bytes into A
            for (int i = 0; i < 4 && length > 0; i++) {
                sHashA += static_cast<uint32_t>(data[index++]) << ((i & 3) << 3);
                sHashA &= 0xFFFFFFFF;
                length--;
            }

            // Mix
            uint32_t a = ((sHashC - sHashA + 0x100000000) & 0xFFFFFFFF) ^ ROL(sHashA, 4);
            uint32_t a1 = sHashB + sHashA;
            uint32_t b = ((sHashB - a + 0x100000000) & 0xFFFFFFFF) ^ ROL(a, 6);
            uint32_t b1 = a1 + a;
            uint32_t c = ((a1 - b + 0x100000000) & 0xFFFFFFFF) ^ ROL(b, 8);
            uint32_t c1 = b1 + b;
            uint32_t d = ((b1 - c + 0x100000000) & 0xFFFFFFFF) ^ ROL(c, 16);
            uint32_t d1 = c1 + c;
            uint32_t e = ((c1 - d + 0x100000000) & 0xFFFFFFFF) ^ ROR(d, 13);
            hashC = d1 + d;
            hashA = ((d1 - e + 0x100000000) & 0xFFFFFFFF) ^ ROL(e, 4);
            hashB = hashC + e;
        }

        // Process remaining bytes (< 12)
        if (length > 0) {
            // Read remaining into C
            for (int i = 0; i < 4 && length > 0; i++) {
                hashC += static_cast<uint32_t>(data[index++]) << ((i & 3) << 3);
                hashC &= 0xFFFFFFFF;
                length--;
            }

            // Read remaining into B
            for (int i = 0; i < 4 && length > 0; i++) {
                hashB += static_cast<uint32_t>(data[index++]) << ((i & 3) << 3);
                hashB &= 0xFFFFFFFF;
                length--;
            }

            // Read remaining into A
            for (int i = 0; i < 4 && length > 0; i++) {
                hashA += static_cast<uint32_t>(data[index++]) << ((i & 3) << 3);
                hashA &= 0xFFFFFFFF;
                length--;
            }
        }

        // Final mix
        uint32_t a = ((hashB ^ hashA) - ROL(hashB, 14) + 0x100000000) & 0xFFFFFFFF;
        uint32_t b = ((hashC ^ a) - ROL(a, 11) + 0x100000000) & 0xFFFFFFFF;
        uint32_t c = ((b ^ hashB) - ROR(b, 7) + 0x100000000) & 0xFFFFFFFF;
        uint32_t d = ((c ^ a) - ROL(c, 16) + 0x100000000) & 0xFFFFFFFF;
        uint32_t e = ((((b ^ d) - ROL(d, 4) + 0x100000000) & 0xFFFFFFFF) ^ c) -
                      ROL(((b ^ d) - ROL(d, 4) + 0x100000000) & 0xFFFFFFFF, 14) + 0x100000000;
        e &= 0xFFFFFFFF;
        uint32_t f = ((e ^ d) - ROR(e, 8) + 0x100000000) & 0xFFFFFFFF;

        return f;
    }

    // Common hashes used in Shovel Knight
    struct CommonHashes {
        // Layer names
        static constexpr uint32_t COLLISION = 0x8B7F4E6A;
        static constexpr uint32_t BOUNDS = 0x1A2B3C4D;  // Example - need real values

        // Object properties
        static constexpr uint32_t POSITION = 0x12345678;  // Example
        static constexpr uint32_t ROTATION = 0x87654321;  // Example

        // Add more as discovered
    };

    // Helper to generate hash lookup table
    static void GenerateHashTable(const std::vector<std::string>& strings) {
        std::cout << "// YCG Hash Table" << std::endl;
        std::cout << "std::map<uint32_t, std::string> g_hashLookup = {" << std::endl;

        for (const auto& str : strings) {
            uint32_t hash = Hash(str);
            std::cout << "    {0x" << std::hex << hash << ", \"" << str << "\"}," << std::endl;
        }

        std::cout << "};" << std::endl;
    }
};
