#pragma once

#include "MIRACL-wrapper.h"

#include <cstddef>
#include <pairing_1.h>
#include <string>

void Sha256(const char* msg, size_t msg_len, char out32[32]);

std::string ToString(const G1& elem);
std::string ToString(const GT& elem);
std::string ToString(const std::vector<uint8_t>& m);
