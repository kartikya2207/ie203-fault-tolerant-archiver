#ifndef RS_HPP
#define RS_HPP

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

using std::vector;
using namespace std;

// --- Galois Field Arithmetic ---
struct GF256 {
    int exp[512];
    int logt[256];

    GF256() {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            exp[i] = x; 
            logt[x] = i;
            x <<= 1;
            if (x & 0x100) x ^= 0x11d; // Primitive Polynomial
        }
        for (int i = 255; i < 512; ++i) exp[i] = exp[i - 255];
        logt[0] = -1;
    }

    int mul(int a, int b) const { 
        if (a && b){
            return exp[logt[a] + logt[b]];
        }
        else{
            return 0;
        }
    }

    int div(int a, int b) const { 
        if (b == 0){
            throw std::runtime_error("GF div by zero");
        }
        
        if (a){
            return exp[(logt[a] - logt[b] + 255) % 255];
        }
        else{
            return 0;
        } 
    }
};

// --- Reed-Solomon Class ---
class RS {
    GF256 gf;
    int n, k, nsym;
    vector<int> gen;

    using Poly = vector<int>;

    void trim(Poly &p) { 
        while (!p.empty() && p.back() == 0) p.pop_back(); 
    }

    int eval(const Poly &p, int x) {
        int res = 0;
        for (int i = p.size() - 1; i >= 0; --i) res = gf.mul(res, x) ^ p[i];
        return res;
    }

    Poly mul_poly(const Poly &a, const Poly &b) {
        if (a.empty() || b.empty()) return {};
        Poly r(a.size() + b.size() - 1, 0);
        for (size_t i = 0; i < a.size(); ++i)
            for (size_t j = 0; j < b.size(); ++j)
                r[i + j] ^= gf.mul(a[i], b[j]);
        trim(r); return r;
    }

public:
    RS(int n_, int k_) : n(n_), k(k_), nsym(n_ - k_) {
        gen = {1};
        for (int i = 0; i < nsym; ++i) {
            gen = mul_poly(gen, {gf.exp[i], 1});
        }
    }

    Poly encode(const Poly &msg) {
        if ((int)msg.size() != k) throw std::runtime_error("Message size mismatch");
        Poly padded = msg; 
        padded.insert(padded.begin(), nsym, 0);
        
        Poly rem = padded;
        for (int i = rem.size() - 1; i >= nsym; --i) {
            if (rem[i] == 0) continue;
            int coef = gf.div(rem[i], gen.back());
            for (size_t j = 0; j < gen.size(); ++j) {
                rem[i - gen.size() + 1 + j] ^= gf.mul(coef, gen[j]);
            }
        }
        Poly code = padded;
        for (int i = 0; i < nsym; ++i) code[i] = rem[i];
        return code;
    }

    Poly berlekamp_massey(const Poly &s) {
        Poly C = {1}, B = {1};
        int L = 0, m = 1, b = 1;

        for (int i = 0; i < nsym; ++i) {
            int d = s[i];
            for (int j = 1; j <= L; ++j) 
                d ^= gf.mul(C[j], s[i - j]);

            if (d == 0) {
                m++;
            } else {
                Poly T = C;
                int coef = gf.div(d, b);
                if (C.size() < B.size() + m) C.resize(B.size() + m, 0);
                for (size_t j = 0; j < B.size(); ++j)
                    C[j + m] ^= gf.mul(coef, B[j]);

                if (2 * L <= i) {
                    L = i + 1 - L;
                    B = T; b = d; m = 1;
                } else {
                    m++;
                }
            }
        }
        trim(C); return C;
    }

    vector<int> chien(const Poly &lambda) {
        vector<int> pos;
        for (int i = 0; i < n; ++i) {
            if (eval(lambda, gf.exp[(255 - i) % 255]) == 0) 
                pos.push_back(i);
        }
        return pos;
    }

    Poly compute_omega(const Poly &s, const Poly &lambda) {
        Poly prod = mul_poly(s, lambda);
        if ((int)prod.size() > nsym) prod.resize(nsym);
        trim(prod); return prod;
    }

    std::pair<Poly, bool> decode(Poly recv) {
        Poly s(nsym);
        bool clean = true;
        for (int i = 0; i < nsym; ++i) {
            s[i] = eval(recv, gf.exp[i]);
            if (s[i]) clean = false;
        }
        
        Poly msg(recv.begin() + nsym, recv.end());
        if (clean) return {msg, true};

        Poly lambda = berlekamp_massey(s);
        vector<int> err_pos = chien(lambda);
        
        if (err_pos.size() != (size_t)(lambda.size() - 1)) return {{}, false};

        Poly omega = compute_omega(s, lambda);
        
        for (int pos : err_pos) {
            int Xinv = gf.exp[(255 - pos) % 255];
            int den = 0;
            for (size_t i = 1; i < lambda.size(); i += 2)
                den ^= gf.mul(lambda[i], gf.exp[(gf.logt[Xinv] * (i - 1)) % 255]);
            
            int num = eval(omega, Xinv);
            if (den == 0) continue;
            int mag = gf.div(num, den); 
            mag = gf.div(mag, Xinv); 
            recv[pos] ^= mag;
        }
        
        // Verify correction
        for (int i = 0; i < nsym; ++i) {
            if (eval(recv, gf.exp[i]) != 0) return {{}, false};
        }

        return {Poly(recv.begin() + nsym, recv.end()), true};
    }
};

#endif
