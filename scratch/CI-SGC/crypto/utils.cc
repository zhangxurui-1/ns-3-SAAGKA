#include "utils.h"

#include <pairing_1.h>
#include <sstream>

void
Sha256(const char* msg, size_t msg_len, char out32[32])
{
    sha256 sh;
    shs256_init(&sh);
    for (size_t i = 0; i < msg_len; i++)
    {
        shs256_process(&sh, msg[i]);
    }
    shs256_hash(&sh, out32);
}

std::string
ToString(const G1& elem)
{
    ZZn coordinate_a, coordinate_b, coordinate_c;
    extract(const_cast<ECn&>(elem.g), coordinate_a, coordinate_b, coordinate_c);

    std::stringstream ss;
    ss << "(" << coordinate_a << ", " << coordinate_b << ", " << coordinate_c << ")";
    return ss.str();
}

std::string
ToString(const GT& elem)
{
    ZZn x, y;
    elem.g.get(x, y);

    std::stringstream ss;
    ss << "(" << x << "," << y << ")";
    return ss.str();
}

std::string
ToString(const std::vector<uint8_t>& m)
{
    stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto b : m)
    {
        ss << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}
