#include "utils.h"

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

std::vector<uint8_t>
SerializeG1(const G1& elem)
{
    ZZn x, y;
    extract(const_cast<ECn&>(elem.g), x, y);

    Big bx(x);
    Big by(y);

    std::vector<uint8_t> bytes;
    bytes.resize(4); // 2 bytes lx + 2 bytes ly

    char tmp[1024];

    int lx = to_binary(bx, 1024, tmp);
    bytes[0] = (lx >> 8) & 0xff;
    bytes[1] = lx & 0xff;
    bytes.insert(bytes.end(), tmp, tmp + lx);

    int ly = to_binary(by, 1024, tmp);
    bytes[2] = (ly >> 8) & 0xff;
    bytes[3] = ly & 0xff;
    bytes.insert(bytes.end(), tmp, tmp + ly);

    return bytes;
}

G1
DeserializeG1(const uint8_t* bytes, size_t len)
{
    if (len < 4)
    {
        throw std::runtime_error("bytes too small");
    }

    int lx = (bytes[0] << 8) | bytes[1];
    int ly = (bytes[2] << 8) | bytes[3];

    if (len < 4 + lx + ly)
    {
        throw std::runtime_error("invalid G1 bytes");
    }

    Big bx = from_binary(lx, (char*)bytes + 4);
    Big by = from_binary(ly, (char*)bytes + 4 + lx);

    ZZn x(bx);
    ZZn y(by);

    ECn P;
    force(x, y, P);

    G1 elem;
    elem.g = P;
    return elem;
}
