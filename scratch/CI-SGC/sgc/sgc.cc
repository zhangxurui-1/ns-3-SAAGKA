#include "sgc.h"

#include "../application.h"
#include "../crypto/agka.h"
#include "../crypto/pki.h"
#include "../crypto/utils.h"
#include "../message/bytereader.h"
#include "../message/header.h"
#include "../message/message.h"
#include "../utils.h"

#include "ns3/singleton.h"

#include <big.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <pairing_1.h>
#include <string>
#include <sys/resource.h>
#include <vector>

const int SGC::SidLength = 16;
const int SGC::KeyVerifierLength = sizeof(uint32_t) + sizeof(uint64_t) + 32;

std::vector<std::shared_ptr<SGCMessage>>
SGC::HandleMsg(const uint8_t* bytes, size_t len)
{
    INFO("Received " << len << " bytes, do nothing");
    return std::vector<std::shared_ptr<SGCMessage>>();
}

bool
SGC::IsPositionOccupied(std::vector<uint8_t> sid, uint32_t pos) const
{
    auto gsi_p = gsis_.find(ParseGroupSeqFromSid(sid))->second;
    size_t index = pos / 8;
    uint8_t mask = 0x80 >> (pos % 8);
    return (gsi_p->mem_bitmap_[index] & mask) != 0;
}

void
SGC::OccupyOnePosition(std::vector<uint8_t> sid, uint32_t pos)
{
    auto gsi_p = gsis_.find(ParseGroupSeqFromSid(sid))->second;
    size_t index = pos / 8;
    uint8_t mask = 0x80 >> (pos % 8);
    gsi_p->mem_bitmap_[index] |= mask;
}

// GroupSessionInfo

SGC::GroupSessionInfo::GroupSessionInfo(uint16_t seq, uint16_t size_param)
    : size_param_(size_param),
      n_member_(0)
{
    auto pp = SAAGKA::GetPublicParameter();
    int scale = pp->matrices[size_param_].size();

    size_t bm_size = ((scale + 63) / 64) * 8;
    mem_bitmap_ = std::vector<uint8_t>(bm_size, 0);

    expiry_time_ = ns3::Simulator::Now() + ns3::Seconds(60);

    // construct sid
    sid_ = std::vector<uint8_t>();
    ByteWriter bw(sid_);

    bw.write("SID-");
    bw.write(seq);
    bw.write(size_param);
    bw.write(expiry_time_);

    auto& matrix = pp->matrices[size_param_];
    d_.resize(scale);
    for (int i = 0; i < scale; i++)
    {
        d_[i].g.clear();
    }
    ek_.lambda.g.clear();

    G1 tmp;
    tmp.g.clear();
    for (int i = 0; i < scale; i++)
    {
        auto v = SAAGKA::HashAnyToBig(std::vector<uint8_t>(),
                                      SAAGKA::PublicKey{matrix[i][scale + 1], matrix[i][scale + 2]},
                                      matrix[i][scale]);
        int j = 0;
        for (; j < scale; ++j)
        {
            if (j == i)
            {
                continue;
            }
            d_[j] = d_[j] + matrix[i][j];
        }
        ek_.lambda = ek_.lambda + matrix[i][j];
        j++;

        tmp = tmp + matrix[i][j] + (pp->pfc->mult(matrix[i][j + 1], v));
    }
    ek_.mu = pp->pfc->pairing(tmp, pp->g0);
}

SGC::GroupSessionInfo::GroupSessionInfo(const GroupSessionInfo& gsi)
    : size_param_(gsi.size_param_),
      n_member_(gsi.n_member_),
      ek_(gsi.ek_),
      expiry_time_(gsi.expiry_time_)
{
    sid_.insert(sid_.end(), gsi.sid_.begin(), gsi.sid_.end());
    mem_bitmap_.insert(mem_bitmap_.end(), gsi.mem_bitmap_.begin(), gsi.mem_bitmap_.end());
}
