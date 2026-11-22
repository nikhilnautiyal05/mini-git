#include "Zlib.h"
#include "Common.h"
#include <stdint.h>

namespace Zlib {

std::string deflateStored(const std::string& data) {
    std::string out;
    out += (char)0x78;    // CMF: deflate, 32K window
    out += (char)0x01;    // FLG: makes the header a multiple of 31

    size_t pos = 0;
    do {
        size_t len = data.size() - pos;
        if (len > 65535) len = 65535;
        bool last = (pos + len >= data.size());
        out += (char)(last ? 1 : 0);                  // BFINAL + BTYPE=00 (stored)
        out += (char)(len & 0xFF);
        out += (char)((len >> 8) & 0xFF);
        out += (char)(~len & 0xFF);
        out += (char)((~len >> 8) & 0xFF);
        out.append(data, pos, len);
        pos += len;
    } while (pos < data.size());

    uint32_t a = 1, b = 0;                            // Adler-32 checksum
    for (size_t i = 0; i < data.size(); ++i) {
        a = (a + (unsigned char)data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    uint32_t adler = (b << 16) | a;
    out += (char)((adler >> 24) & 0xFF);
    out += (char)((adler >> 16) & 0xFF);
    out += (char)((adler >> 8) & 0xFF);
    out += (char)(adler & 0xFF);
    return out;
}

// ---------------------------------------------------------------------------
// Inflate (structure follows the public-domain "puff" reference decoder)
// ---------------------------------------------------------------------------
namespace {

struct BitReader {
    const std::string& in;
    size_t pos;
    unsigned bitbuf;
    int bitcnt;
    BitReader(const std::string& s, size_t start) : in(s), pos(start), bitbuf(0), bitcnt(0) {}

    int bits(int need) {
        long val = (long)bitbuf;
        while (bitcnt < need) {
            if (pos >= in.size()) throw MiniGitError("corrupt object: zlib data ended early");
            val |= (long)(unsigned char)in[pos++] << bitcnt;
            bitcnt += 8;
        }
        bitbuf = (unsigned)(val >> need);
        bitcnt -= need;
        return (int)(val & ((1L << need) - 1));
    }
};

struct Huffman {
    short count[16];
    short symbol[288];
};

int decode(BitReader& s, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; ++len) {
        code |= s.bits(1);
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    throw MiniGitError("corrupt object: invalid Huffman code");
}

int construct(Huffman& h, const short* length, int n) {
    for (int len = 0; len <= 15; ++len) h.count[len] = 0;
    for (int sym = 0; sym < n; ++sym) h.count[length[sym]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }
    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; ++len) offs[len + 1] = (short)(offs[len] + h.count[len]);
    for (int sym = 0; sym < n; ++sym)
        if (length[sym] != 0) h.symbol[offs[length[sym]]++] = (short)sym;
    return left;
}

const short LENS[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const short LEXT[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const short DISTS[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const short DEXT[30]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

void codes(BitReader& s, std::string& out, const Huffman& lencode, const Huffman& distcode) {
    int symbol;
    do {
        symbol = decode(s, lencode);
        if (symbol < 256) {
            out += (char)symbol;
        } else if (symbol > 256) {
            symbol -= 257;
            if (symbol >= 29) throw MiniGitError("corrupt object: bad length symbol");
            int len = LENS[symbol] + s.bits(LEXT[symbol]);
            symbol = decode(s, distcode);
            if (symbol >= 30) throw MiniGitError("corrupt object: bad distance symbol");
            size_t dist = (size_t)(DISTS[symbol] + s.bits(DEXT[symbol]));
            if (dist > out.size()) throw MiniGitError("corrupt object: distance too far back");
            size_t from = out.size() - dist;
            while (len--) {
                char c = out[from++];
                out += c;
            }
        }
    } while (symbol != 256);
}

void storedBlock(BitReader& s, std::string& out) {
    s.bitbuf = 0;
    s.bitcnt = 0;
    if (s.pos + 4 > s.in.size()) throw MiniGitError("corrupt object: truncated stored block");
    unsigned len  = (unsigned char)s.in[s.pos]     | ((unsigned)(unsigned char)s.in[s.pos + 1] << 8);
    unsigned nlen = (unsigned char)s.in[s.pos + 2] | ((unsigned)(unsigned char)s.in[s.pos + 3] << 8);
    s.pos += 4;
    if (len != (~nlen & 0xFFFFu)) throw MiniGitError("corrupt object: stored block length mismatch");
    if (s.pos + len > s.in.size()) throw MiniGitError("corrupt object: stored block too long");
    out.append(s.in, s.pos, len);
    s.pos += len;
}

void fixedBlock(BitReader& s, std::string& out) {
    static bool built = false;
    static Huffman lencode, distcode;
    if (!built) {
        short lengths[288];
        int sym;
        for (sym = 0; sym < 144; ++sym) lengths[sym] = 8;
        for (; sym < 256; ++sym) lengths[sym] = 9;
        for (; sym < 280; ++sym) lengths[sym] = 7;
        for (; sym < 288; ++sym) lengths[sym] = 8;
        construct(lencode, lengths, 288);
        for (sym = 0; sym < 30; ++sym) lengths[sym] = 5;
        construct(distcode, lengths, 30);
        built = true;
    }
    codes(s, out, lencode, distcode);
}

void dynamicBlock(BitReader& s, std::string& out) {
    static const short ORDER[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    int nlen  = s.bits(5) + 257;
    int ndist = s.bits(5) + 1;
    int ncode = s.bits(4) + 4;
    if (nlen > 286 || ndist > 30) throw MiniGitError("corrupt object: bad dynamic block header");

    short lengths[320];
    int index;
    for (index = 0; index < ncode; ++index) lengths[ORDER[index]] = (short)s.bits(3);
    for (; index < 19; ++index) lengths[ORDER[index]] = 0;

    Huffman lencode, distcode;
    if (construct(lencode, lengths, 19) != 0) throw MiniGitError("corrupt object: bad code-length code");

    index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(s, lencode);
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
        } else {
            int len = 0, rep;
            if (symbol == 16) {
                if (index == 0) throw MiniGitError("corrupt object: repeat with no previous length");
                len = lengths[index - 1];
                rep = 3 + s.bits(2);
            } else if (symbol == 17) {
                rep = 3 + s.bits(3);
            } else {
                rep = 11 + s.bits(7);
            }
            if (index + rep > nlen + ndist) throw MiniGitError("corrupt object: too many lengths");
            while (rep--) lengths[index++] = (short)len;
        }
    }
    if (lengths[256] == 0) throw MiniGitError("corrupt object: no end-of-block code");

    int err = construct(lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
        throw MiniGitError("corrupt object: bad literal/length code");
    err = construct(distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
        throw MiniGitError("corrupt object: bad distance code");

    codes(s, out, lencode, distcode);
}

} // namespace

std::string inflate(const std::string& z) {
    if (z.size() < 6) throw MiniGitError("corrupt object: zlib stream too short");
    unsigned cmf = (unsigned char)z[0], flg = (unsigned char)z[1];
    if ((cmf & 0x0F) != 8 || ((cmf << 8) | flg) % 31 != 0)
        throw MiniGitError("corrupt object: not a zlib stream");
    if (flg & 0x20) throw MiniGitError("corrupt object: preset dictionary not supported");

    BitReader s(z, 2);
    std::string out;
    int last;
    do {
        last = s.bits(1);
        int type = s.bits(2);
        if (type == 0)      storedBlock(s, out);
        else if (type == 1) fixedBlock(s, out);
        else if (type == 2) dynamicBlock(s, out);
        else throw MiniGitError("corrupt object: invalid block type");
    } while (!last);

    // The stream must end with a 4-byte Adler-32 checksum and nothing else.
    if (s.pos + 4 != z.size()) throw MiniGitError("corrupt object: extra or missing bytes after zlib data");
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        a = (a + (unsigned char)out[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    uint32_t expect = ((uint32_t)(unsigned char)z[s.pos] << 24) | ((uint32_t)(unsigned char)z[s.pos + 1] << 16) |
                      ((uint32_t)(unsigned char)z[s.pos + 2] << 8) | (uint32_t)(unsigned char)z[s.pos + 3];
    if (((b << 16) | a) != expect) throw MiniGitError("corrupt object: zlib checksum mismatch");
    return out;
}

} // namespace Zlib
