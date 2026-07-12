#ifndef LZW_HPP
#define LZW_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>

namespace LZW {

    // LZW Compression
    // Input : String, Output : Vector of integers
    
    inline std::vector<uint32_t> compress(const std::string& input) {
        std::unordered_map<std::string, uint32_t> dict;

        // Initialize dictionary with single characters (0-255)
        for (int i = 0; i < 256; i++)
            dict[std::string(1, char(i))] = i;

        uint32_t nextCode = 256;
        std::vector<uint32_t> out;
        std::string w;

        for (unsigned char uc : input) {
            char c = char(uc);
            std::string wc = w + c;

            if (dict.count(wc)) {
                w = wc;
            } else {
                out.push_back(dict[w]);

                // Add new substring to dictionary
                dict[wc] = nextCode++;
                w = std::string(1, c);
            }
        }

        if (!w.empty()) {
            out.push_back(dict[w]);
        }

        return out;
    }

    // LZW Decompression
    // Input : Vector of integers, Output : String
    
    inline std::string decompress(const std::vector<uint32_t>& codes) {
        if (codes.empty()) return "";

        std::unordered_map<uint32_t, std::string> dict;

        // Initialize dictionary with single characters (0-255)
        for (int i = 0; i < 256; i++)
            dict[i] = std::string(1, char(i));

        uint32_t nextCode = 256;

        std::string prev = dict.at(codes[0]);
        std::string out = prev;

        for (size_t i = 1; i < codes.size(); i++) {
            uint32_t code = codes[i];
            std::string entry;

            if (dict.count(code)) {
                entry = dict[code];
            } else if (code == nextCode) {
                // Special case: code is for the string we are currently building
                entry = prev + prev[0];
            } else {
                throw std::runtime_error("Corrupt LZW code sequence: Invalid code found");
            }

            out += entry;

            // Add new entry to dictionary
            dict[nextCode++] = prev + entry[0];

            prev = entry;
        }

        return out;
    }
}

#endif // LZW_HPP
