#include "Sha1.h"
#include <stdint.h>

static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

std::string Sha1::raw(const std::string& data) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    std::string msg = data;
    uint64_t bitLen = (uint64_t)data.size() * 8;
    msg += (char)0x80;
    while (msg.size() % 64 != 56) msg += (char)0x00;
    for (int i = 7; i >= 0; --i) msg += (char)((bitLen >> (i * 8)) & 0xFF);

    for (size_t c = 0; c < msg.size(); c += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)(unsigned char)msg[c + i * 4]     << 24) |
                   ((uint32_t)(unsigned char)msg[c + i * 4 + 1] << 16) |
                   ((uint32_t)(unsigned char)msg[c + i * 4 + 2] << 8)  |
                   ((uint32_t)(unsigned char)msg[c + i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i)
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h[0], b = h[1], cc = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & cc) | ((~b) & d);          k = 0x5A827999u; }
            else if (i < 40) { f = b ^ cc ^ d;                     k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & cc) | (b & d) | (cc & d);  k = 0x8F1BBCDCu; }
            else             { f = b ^ cc ^ d;                     k = 0xCA62C1D6u; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = cc; cc = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += cc; h[3] += d; h[4] += e;
    }

    std::string out;
    for (int i = 0; i < 5; ++i)
        for (int j = 3; j >= 0; --j)
            out += (char)((h[i] >> (j * 8)) & 0xFF);
    return out;
}

std::string Sha1::toHex(const std::string& r) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < r.size(); ++i) {
        unsigned char ch = (unsigned char)r[i];
        out += digits[ch >> 4];
        out += digits[ch & 0x0F];
    }
    return out;
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

std::string Sha1::toRaw(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        out += (char)((hexVal(hex[i]) << 4) | hexVal(hex[i + 1]));
    return out;
}

std::string Sha1::hex(const std::string& data) { return toHex(raw(data)); }
