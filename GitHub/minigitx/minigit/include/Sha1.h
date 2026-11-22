#ifndef SHA1_H
#define SHA1_H

#include <string>

// Self-contained SHA-1 (the hash real Git uses for object ids).
class Sha1 {
public:
    static std::string raw(const std::string& data);   // 20 raw bytes
    static std::string hex(const std::string& data);   // 40 hex characters
    static std::string toHex(const std::string& raw20);
    static std::string toRaw(const std::string& hex40);
};

#endif
