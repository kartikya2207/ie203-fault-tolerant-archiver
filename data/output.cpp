#include <iostream>
#include <vector>
#include <string>
#include <cstdio> // for std::remove
#include <filesystem>

// Include libraries
#include "bitio.h"
#include "LZW.hpp"
#include "Reed_Solomon.hpp"

// RS Configuration: (255, 223)
// 223 bytes data + 32 bytes parity = 255 bytes total
const int N = 255;
const int K = 223;

// Helpers

std::vector<int> str_to_poly(const std::string& s) {
    std::vector<int> p;
    p.reserve(s.size());
    for (unsigned char c : s) p.push_back(c);
    return p;
}

std::string poly_to_str(const std::vector<int>& p) {
    std::string s;
    s.reserve(p.size());
    for (int x : p) s.push_back((char)x);
    return s;
}

void print_usage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " encode <input> <output>\n"
              << "  " << prog << " decode <input> <output>\n";
}

// --- Pipelines ---

void run_encode_pipeline(const std::string& in_path, const std::string& out_path) {
    std::cout << "[1/4] Reading and Compressing (LZW)..." << std::endl;
    std::string raw_data = read_binary_file(in_path);
    std::vector<uint32_t> lzw_codes = LZW::compress(raw_data);

    // Serialize LZW codes to a temporary binary file
    std::string temp_file = out_path + ".tmp.lzw";
    write_codes_to_file(lzw_codes, temp_file);

    std::cout << "[2/4] Reading compressed stream..." << std::endl;
    std::string compressed_data = read_binary_file(temp_file);
    
    // Clean up temp file immediately
    std::remove(temp_file.c_str());

    std::cout << "[3/4] Applying Error Correction (Reed-Solomon)..." << std::endl;
    
    // Prepare data for RS: [4-byte Size Header] + [Data] + [Zero Padding]
    uint32_t comp_size = compressed_data.size();
    std::string buffer;
    buffer.reserve(comp_size + 4 + K);

    // Header: Store length of the COMPRESSED stream
    buffer.push_back((comp_size >> 24) & 0xFF);
    buffer.push_back((comp_size >> 16) & 0xFF);
    buffer.push_back((comp_size >> 8)  & 0xFF);
    buffer.push_back((comp_size)       & 0xFF);
    
    buffer += compressed_data;

    // Padding: Ensure total size is a multiple of K
    int padding = K - (buffer.size() % K);
    if (padding != K) {
        buffer.append(padding, '\0');
    }

    // Encode block by block
    RS rs(N, K);
    int num_blocks = buffer.size() / K;
    std::string final_output;
    final_output.reserve(num_blocks * N);

    for (int i = 0; i < num_blocks; ++i) {
        std::string block_str = buffer.substr(i * K, K);
        std::vector<int> msg = str_to_poly(block_str);
        std::vector<int> code = rs.encode(msg);
        final_output += poly_to_str(code);
    }

    std::cout << "[4/4] Writing " << final_output.size() << " bytes to output..." << std::endl;
    write_binary_file(final_output, out_path);
    
    std::cout << "Done.\n"
              << "  Original Size: " << raw_data.size() << " bytes\n"
              << "  LZW Size:      " << comp_size << " bytes\n"
              << "  Final Size:    " << final_output.size() << " bytes (Protected)" << std::endl;
}

void run_decode_pipeline(const std::string& in_path, const std::string& out_path) {
    std::cout << "[1/4] Reading protected file..." << std::endl;
    std::string raw = read_binary_file(in_path);

    // Basic validation
    if (raw.size() % N != 0) {
        std::cerr << "Warning: File size is not a multiple of " << N << ". Padding..." << std::endl;
        int padding = N - (raw.size() % N);
        raw.append(padding, '\0');
    }

    std::cout << "[2/4] Correcting Errors (Reed-Solomon)..." << std::endl;
    RS rs(N, K);
    int num_blocks = raw.size() / N;
    std::string recovered_buffer;
    int blocks_failed = 0;

    for (int i = 0; i < num_blocks; ++i) {
        std::string block_str = raw.substr(i * N, N);
        std::vector<int> recv = str_to_poly(block_str);
        
        auto result = rs.decode(recv);
        
        if (!result.second) {
            blocks_failed++;
            // Fallback: Keep the data part even if correction failed
            std::vector<int> fallback(recv.begin() + (N - K), recv.end());
            recovered_buffer += poly_to_str(fallback);
        } else {
            recovered_buffer += poly_to_str(result.first);
        }
    }

    if (blocks_failed > 0) {
        std::cerr << "WARNING: " << blocks_failed << " blocks had uncorrectable errors!" << std::endl;
    }

    // Parse Header to find exact LZW stream length
    if (recovered_buffer.size() < 4) throw std::runtime_error("Decoded data too small");
    
    uint32_t comp_size = 
        ((uint8_t)recovered_buffer[0] << 24) | 
        ((uint8_t)recovered_buffer[1] << 16) | 
        ((uint8_t)recovered_buffer[2] << 8)  | 
        ((uint8_t)recovered_buffer[3]);

    if (comp_size > recovered_buffer.size() - 4) {
        std::cerr << "Warning: Header size mismatch. Using available data." << std::endl;
        comp_size = recovered_buffer.size() - 4;
    }

    std::string lzw_stream = recovered_buffer.substr(4, comp_size);

    std::cout << "[3/4] Decompressing (LZW)..." << std::endl;
    
    // Write LZW stream to temp file for bitio.h to read
    std::string temp_file = in_path + ".tmp.lzw";
    write_binary_file(lzw_stream, temp_file);

    auto codes = read_codes_from_file(temp_file);
    std::string original_data = LZW::decompress(codes);
    
    std::remove(temp_file.c_str());

    std::cout << "[4/4] Writing recovered data..." << std::endl;
    write_binary_file(original_data, out_path);

    std::cout << "Done. Recovered " << original_data.size() << " bytes." << std::endl;
}

// --- Main ---

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    std::string in_path = argv[2];
    std::string out_path = argv[3];

    try {
        if (mode == "encode") {
            run_encode_pipeline(in_path, out_path);
        } else if (mode == "decode") {
            run_decode_pipeline(in_path, out_path);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
