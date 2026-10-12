# Fault-Tolerant Data Archiver

A from-scratch C++ tool that **compresses** a file with LZW and then **armours it** with
Reed–Solomon error correction, so you can flip bytes in a hex editor and still get the exact
original back.

![C++17](https://img.shields.io/badge/language-C%2B%2B17-00599C) ![build: make](https://img.shields.io/badge/build-make-informational) ![MIT](https://img.shields.io/badge/license-MIT-green)

Course project for **IE203 — Data Structures & Algorithms, IIT Bombay** (Nov 2025), built with
Adit Prabhu and Darshan Jain. Full write-up with worked examples, pseudocode
and complexity proofs: **[`report/DSA_PROJECT_Report.pdf`](report/DSA_PROJECT_Report.pdf)**.

## What it is

Compression and error correction pull in opposite directions: one removes redundancy, the other
adds it back. This tool does both in the right order — compress first to shrink the payload, then
add a controlled, mathematically structured layer of redundancy so the compressed stream survives
corruption. Both algorithms are hand-written in C++17 using only the standard library; the point was
to understand how they work inside, not to call a library.

## What I did

**Stage 1 — LZW compression (lossless).** The dictionary is seeded with all 256 byte values and
grows as patterns repeat; the decoder rebuilds the same dictionary, so nothing extra is stored.
Codes are **variable-width** (9 bits, widening to 10, 11, 12…) and bit-packed to disk by
`bitio.h`. The decoder handles the classic edge case where a code refers to an entry it hasn't
finished building yet (`prev + prev[0]`).

**Stage 2 — Reed–Solomon RS(255, 223) over GF(2⁸).** Bytes are treated as polynomial coefficients
over GF(2⁸) with primitive polynomial `0x11D`; addition is XOR and multiplication uses log/antilog
tables. Each 255-byte block is **223 data + 32 parity bytes** (systematic encoding), which
corrects up to **t = 16 bad bytes per block** — 2t parity because each error has two unknowns,
its position and its value. Decoding runs Syndromes → **Berlekamp–Massey** (error locator) →
**Chien search** (positions) → **Forney** (magnitudes).

**Gluing them together.** LZW produces a non-byte-aligned bitstream; RS wants fixed 223-byte blocks.
Rather than bit-buffering one into the other, we decoupled them: LZW codes are bit-packed to a
temporary `.tmp.lzw` file, RS reads that as plain bytes, and a **4-byte length header** is
prepended so the decoder can tell real data from the zero padding in the last block. If a block is
uncorrectable, the decoder warns and falls back to its raw data bytes rather than aborting.

### Results (from report §5)

| Input type | Original | After compress + RS | |
|---|---:|---:|---|
| Jupyter notebook (`.ipynb`, JSON) | 182 KB | **50 KB** | text compresses well |
| Executable (`.out`) | 72 KB | **45 KB** | still a solid reduction |
| PDF | 2.0 MB | 2.4 MB | *grows* — already compressed |
| PNG | 25 KB | 35 KB | *grows* — already compressed |

- Random byte edits made with `hexedit` were fully repaired. Recovery is exact as long as no
  255-byte block has more than 16 corrupted bytes, i.e. up to **≈ 6.27 %** of bytes.
- The fixture in `data/` shows it end to end: `input.cpp` (6227 bytes) encodes to a 3570-byte
  `encoded.bin`; `encoded_corrupted.bin` differs from it in 33 bytes, and decoding it reproduces
  `input.cpp` byte-for-byte.
- Both stages are O(N) in file size for encode and decode.

## What I learnt

- **Finite-field arithmetic stops being scary once it's code.** GF(2⁸) looked abstract in the
  textbook; as a pair of 256-entry log/antilog tables plus XOR, it's just lookups. Building the
  tables made the "why" of the generator polynomial click.
- **The decoder is where the difficulty is.** Encoding RS is a polynomial division; decoding needs
  four algorithms chained together, and a bug in any one shows up as "wrong bytes" at the end. We
  leaned heavily on flipping known bytes with `hexedit` and checking that exactly those got fixed.
- **Interfaces matter more than cleverness.** The bitstream-vs-bytes mismatch between LZW and RS was
  the real engineering problem. The temp-file + length-header design is less elegant than streaming,
  but it made each stage testable in isolation.
- **Compression isn't free.** PDFs and PNGs got *bigger*: they're already compressed, so LZW finds
  little to exploit and RS adds ~14 % parity on top. A smarter tool would detect this and skip LZW.
- **Limits I'd fix next:** RS corrects at most 16 errors *per block*, so a burst concentrated in
  one block can defeat it even if the file is mostly fine — interleaving blocks would spread bursts
  out. The temp file is also an unnecessary disk round-trip.

## Repo structure

```
src/
  main.cpp            CLI + encode/decode pipeline
  bitio.h             binary file I/O + variable-width bit-packing
  LZW.hpp             LZW compress() / decompress()
  Reed_Solomon.hpp    GF(2^8) arithmetic + RS(255,223) encode/decode
report/
  DSA_PROJECT_Report.pdf
data/
  input.cpp           sample input
  output.cpp          recovered output (identical to input.cpp)
  encoded.bin         input.cpp after compress + RS
  encoded_corrupted.bin   encoded.bin with bytes flipped in a hex editor
Makefile              build / demo / clean
```

## How to run

Needs a C++17 compiler (`g++` or `clang++`); `make` is optional.

```bash
make                       # or: g++ -std=c++17 -O2 src/main.cpp -o coding_tool

./coding_tool encode <input_file> <output_file>      # compress + protect
./coding_tool decode <protected_file> <recovered>    # repair + decompress
```

End-to-end demo on the pre-corrupted fixture:

```bash
make demo
# or manually:
./coding_tool decode data/encoded_corrupted.bin recovered.cpp
diff data/input.cpp recovered.cpp      # no output = identical
```

To try your own corruption, encode a file, flip bytes with a hex editor (keep it to ≤16 per
255-byte block), and decode.

## License

[MIT](LICENSE) © 2025 Adit Prabhu, Kartikya Saini, Darshan Jain.
