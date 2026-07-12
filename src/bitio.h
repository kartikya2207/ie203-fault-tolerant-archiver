#ifndef BITIO_H
#define BITIO_H

#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <iterator>

// Constants from original file
static const int START_BITS = 9;
static const int MAX_BITS   = 32;

// Read raw file to string
inline std::string read_binary_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open input file");

    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

// Write raw binary data to file
inline void write_binary_file(const std::string& data, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open output file");

    f.write(data.data(), data.size());
}

// Write vector<uint32_t> codes to file with bit-packing
inline void write_codes_to_file(const std::vector<uint32_t>& codes,
                                const std::string& path)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output file");

    uint64_t buf = 0;
    int bitsInBuf = 0;

    int codeWidth = START_BITS;
    uint32_t nextCodeBoundary = 1u << codeWidth;

    for (size_t i = 0; i < codes.size(); i++) {
        uint32_t code = codes[i];

        if ((i + 256) == nextCodeBoundary && codeWidth < MAX_BITS) {
            codeWidth++;
            nextCodeBoundary = (1u << codeWidth);
        }

        buf |= (uint64_t(code) << bitsInBuf);
        bitsInBuf += codeWidth;

        while (bitsInBuf >= 8) {
            out.put(char(buf & 0xFF));
            buf >>= 8;
            bitsInBuf -= 8;
        }
    }

    while (bitsInBuf > 0) {
        out.put(char(buf & 0xFF));
        buf >>= 8;
        bitsInBuf -= 8;
    }
}

// Read vector<uint32_t> codes from bit-packed file
inline std::vector<uint32_t> read_codes_from_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file");

    std::vector<uint32_t> codes;

    uint64_t buf = 0;
    int bitsInBuf = 0;

    int codeWidth = START_BITS;
    uint32_t nextCodeBoundary = 1u << codeWidth;
    int i = 0;

    while (true) {
        while (bitsInBuf < codeWidth) {
            int c = in.get();
            if (c == EOF) {
                if (bitsInBuf == 0)
                    return codes;
                break;
            }
            buf |= (uint64_t(uint8_t(c)) << bitsInBuf);
            bitsInBuf += 8;
        }

        if (bitsInBuf < codeWidth)
            return codes;

        uint32_t code = uint32_t(buf & ((1ull << codeWidth) - 1));
        buf >>= codeWidth;
        bitsInBuf -= codeWidth;

        codes.push_back(code);
        i += 1;

        if ((i + 256) == nextCodeBoundary && codeWidth < MAX_BITS) {
            codeWidth++;
            nextCodeBoundary = (1u << codeWidth);
        }
    }

    return codes;
}

#endif // BITIO_H
