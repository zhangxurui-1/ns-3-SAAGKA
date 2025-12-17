#include "MIRACL-wrapper.h"

#include <cstddef>
#include <cstdint>
#include <vector>

void Sha256(const char* msg, size_t msg_len, char out32[32]);
std::vector<uint8_t> SerializeG1(const G1& elem);
G1 DeserializeG1(const uint8_t* bytes, size_t len);
