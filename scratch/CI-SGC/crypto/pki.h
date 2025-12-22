#pragma once

#include "agka.h"

#include <unordered_map>

class PKI
{
  public:
    bool Upload(uint32_t pk_id, SAAGKA::PublicKey pk);
    SAAGKA::PublicKey Get(uint32_t pk_id);

  private:
    std::unordered_map<uint32_t, SAAGKA::PublicKey> bulletin_board_;
};
