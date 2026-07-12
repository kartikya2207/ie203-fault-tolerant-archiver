# Fault-Tolerant Data Archiver — build file
#
#   make           build the ./coding_tool executable
#   make clean     remove the executable and any temp streams
#   make demo      build, then encode -> corrupt-tolerant decode round-trip

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
TARGET    = coding_tool
SRC       = src/main.cpp
HEADERS   = src/bitio.h src/LZW.hpp src/Reed_Solomon.hpp

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

# Round-trip on the corrupted fixture: RS should repair it and recover the original.
.PHONY: demo
demo: $(TARGET)
	./$(TARGET) decode data/encoded_corrupted.bin demo_recovered.cpp
	@echo "--- diff (empty = perfect recovery from corrupted input) ---"
	@diff data/input.cpp demo_recovered.cpp && echo "OK: recovered file is identical to the original"

.PHONY: clean
clean:
	rm -f $(TARGET) demo_recovered.cpp *.tmp.lzw
