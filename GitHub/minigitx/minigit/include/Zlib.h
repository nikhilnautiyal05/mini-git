#ifndef ZLIB_H_MINIGIT
#define ZLIB_H_MINIGIT

#include <string>

// Minimal zlib support, written from scratch so no external library is needed.
//  - deflateStored(): writes a valid zlib stream using "stored" (uncompressed)
//    blocks. Real Git can read these objects.
//  - inflate(): full decoder (stored, fixed and dynamic Huffman blocks), so
//    objects compressed by real Git can be read too.
namespace Zlib {
std::string deflateStored(const std::string& data);
std::string inflate(const std::string& zdata);   // throws MiniGitError on bad data
}

#endif
