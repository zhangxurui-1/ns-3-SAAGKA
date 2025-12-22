#include "pki.h"

#include "agka.h"

bool
PKI::Upload(uint32_t pk_id, SAAGKA::PublicKey pk)
{
    if (bulletin_board_.find(pk_id) != bulletin_board_.end())
    {
        return false;
    }
    bulletin_board_[pk_id] = pk;
    return true;
}

SAAGKA::PublicKey
PKI::Get(uint32_t pk_id)
{
    if (bulletin_board_.find(pk_id) == bulletin_board_.end())
    {
        return SAAGKA::PublicKey();
    }
    return bulletin_board_[pk_id];
}
