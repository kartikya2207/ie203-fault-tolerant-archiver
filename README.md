# Fault-Tolerant Data Archiver

> A from-scratch C++ archiving tool that **compresses** a file with Lempel–Ziv–Welch (LZW) and then **armours it against corruption** with Reed–Solomon error-correcting codes — so you can flip random bytes in a hex editor and still recover the exact original.

<p align="center">
  <img alt="Language" src="https://img.shields.io/badge/language-C%2B%2B17-00599C">
  <img alt="Build" src="https://img.shields.io/badge/build-make-informational">
  <img alt="License" src="https://img.shields.io/badge/license-MIT-green">
  <img alt="Course" src="https://img.shields.io/badge/IE203-IIT%20Bombay-blue">
</p>

---

## Course context

Course project for **IE203 — Data Structures & Algorithms**, **IIT Bombay** (Nov 2025).

**Authors:** Adit Prabhu · Kartikya Saini (24B4519) · Darshan Jain

The full write-up — intuition, worked examples, algorithms, complexity analysis, and results — is in **[`report/DSA_PROJECT_Report.pdf`](report/DSA_PROJECT_Report.pdf)**.

---

## Problem statement & motivation

Storage and transmission media are neither free nor perfectly reliable. Two things we usually want at once are in tension:

- **Compression** removes redundancy to make files smaller.
- **Error correction** *adds* redundancy so that damage can be repaired.

This project builds a single tool that does both, in the right order: compress first to shrink the payload, then add a controlled, mathematically structured layer of redundancy on top so the compressed stream can survive corruption. Both algorithms are implemented **from scratch in C++ — no external libraries** — with the explicit goal of understanding their internal mechanics.

---

## Approach / methodology

The pipeline has two stages, run back-to-back.

### Stage 1 — LZW compression (lossless)

LZW builds a dictionary of repeated byte patterns on the fly and emits a short integer *code* for each pattern instead of the pattern itself. The decoder rebuilds the identical dictionary in the same order, so no dictionary needs to be stored.

- Dictionary seeded with all 256 single-byte values.
- **Variable-width codes:** output starts at 9 bits and widens to 10, 11, 12… bits as the dictionary grows, bit-packed to disk for maximum density (`bitio.h`).
- Handles the classic LZW *edge case* where a code refers to a string the decoder hasn't finished building yet (`prev + prev[0]`).
- Runs in **O(n)** for both compression and decompression.

### Stage 2 — Reed–Solomon error correction: RS(255, 223)

Reed–Solomon treats blocks of bytes as coefficients of a polynomial over the Galois field **GF(2⁸)** (primitive polynomial `0x11D`), where addition is XOR and multiplication uses log/antilog lookup tables for speed. A valid codeword is constructed to be exactly divisible by a generator polynomial, so evaluating a received block at the generator's roots reveals whether — and how — it was corrupted.

- **RS(255, 223):** every 255-byte block = **223 data bytes + 32 parity bytes**.
- Corrects up to **t = 16 corrupted bytes per block** (redundancy is `2t` because each error hides *two* unknowns — its location and its value).
- **Systematic encoding** — the original data stays readable in the codeword, parity is appended.
- **Decoder pipeline:** Syndrome calculation → **Berlekamp–Massey** (error-locator polynomial) → **Chien search** (error positions) → **Forney algorithm** (error magnitudes).
- Linear **O(N)** in file size overall.

### Integration: bridging a bitstream to a byte-block codec

LZW emits a *variable-width, non-byte-aligned* bitstream; Reed–Solomon consumes *fixed 8-bit bytes in blocks of 223*. The two stages are decoupled cleanly rather than bit-buffered together:

1. LZW codes are bit-packed to a temporary file (`.tmp.lzw`), which absorbs all bit-alignment/padding concerns.
2. RS reads that file as an opaque byte stream — it neither knows nor cares that the bytes represent variable-width integers.
3. A **4-byte length header** is prepended before RS framing so the decoder can tell real compressed data from the zero-padding RS adds to fill its final 255-byte block.

---

## Repository structure

```
ie203-fault-tolerant-archiver/
├── src/                       # Source (header-only algorithms + CLI driver)
│   ├── main.cpp               #   CLI + encode/decode pipeline orchestration
│   ├── bitio.h                #   Binary file I/O + variable-width bit-packing
│   ├── LZW.hpp                #   LZW compress() / decompress()
│   └── Reed_Solomon.hpp       #   GF(2^8) arithmetic + RS(255,223) encode/decode
├── report/
│   └── DSA_PROJECT_Report.pdf # Full project report (methods, proofs, results)
├── data/                      # Demo / test fixtures
│   ├── input.cpp              #   Sample input (a copy of the tool's own source)
│   ├── output.cpp             #   Recovered output — byte-identical to input.cpp
│   ├── encoded.bin            #   input.cpp after compress + RS encode
│   └── encoded_corrupted.bin  #   encoded.bin with bytes flipped in a hex editor
├── Makefile                   # build / clean / demo targets
├── README.md
├── LICENSE                    # MIT
└── .gitignore
```

> The `data/` files are a self-contained proof of the tool: `encoded_corrupted.bin` is `encoded.bin` with bytes deliberately damaged, yet decoding it still reproduces `input.cpp` exactly (`input.cpp` and `output.cpp` are identical).

---

## Build & run

### Requirements
- A C++17 compiler (`g++` or `clang++`)
- `make` (optional — a one-line compile works too)

### Build

```bash
make
# or, without make:
g++ -std=c++17 -O2 src/main.cpp -o coding_tool
```

This produces the `coding_tool` executable. (The prebuilt Linux binary is intentionally **not** committed — build it for your own platform.)

### Usage

```bash
# Compress + protect a file
./coding_tool encode <input_file> <output_file>

# Recover + decompress a (possibly corrupted) protected file
./coding_tool decode <protected_file> <recovered_file>
```

### End-to-end demo

```bash
# Encode the sample file
./coding_tool encode data/input.cpp encoded.bin

# (Optional) flip some bytes with a hex editor — keep ≤16 errors per 255-byte block —
# or just use the pre-corrupted fixture that ships in data/:
./coding_tool decode data/encoded_corrupted.bin recovered.cpp

# Verify perfect recovery
diff data/input.cpp recovered.cpp   # no output = identical
```

Or simply run the bundled round-trip target:

```bash
make demo
```

---

## Key results

Measured on a range of input types (report §5):

| Input type | Original | After compress + RS | Notes |
|---|---:|---:|---|
| Jupyter notebook (`.ipynb`, JSON text) | 182 KB | **50 KB** | ~3.6× smaller — text/raw data compresses well |
| Executable (`.out`) | 72 KB | **45 KB** | still a solid reduction |
| PDF (`.pdf`) | 2.0 MB | 2.4 MB | *grows* — already compressed |
| Image (`.png`) | 25 KB | 35 KB | *grows* — already compressed |

**Error resilience.** Random byte edits made with `hexedit` were fully repaired on decode. The tool recovers the exact original as long as **no 255-byte block has more than 16 corrupted bytes** — i.e. up to **≈ 6.27 % of all bytes** can be damaged and still be corrected.

**Takeaways.**
- Compression pays off strongly for plaintext/raw formats and, as expected, offers nothing for already-compressed formats (PDF/PNG), which grow once RS parity is added.
- Both stages are **O(N)** in file size for encode and decode, so the tool scales linearly and stays practical on large files.

---

## Tech stack

- **Language:** C++17 (standard library only — no third-party dependencies)
- **Algorithms (all hand-implemented):** LZW compression with variable-width bit-packing; Reed–Solomon RS(255,223) over GF(2⁸); Berlekamp–Massey, Chien search, Forney algorithm
- **Build:** GNU Make / `g++`
- **Testing during development:** `hexedit` for injecting corruption

---

## Report

See **[`report/DSA_PROJECT_Report.pdf`](report/DSA_PROJECT_Report.pdf)** for the complete treatment: intuition and worked examples, all algorithm pseudocode, Galois-field foundations, best/worst-case complexity analysis, the LZW↔RS integration design, and full results.

## License

Released under the [MIT License](LICENSE) © 2025 Adit Prabhu, Kartikya Saini, Darshan Jain.
