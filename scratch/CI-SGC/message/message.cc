#include "message.h"

#include "../utils.h"
#include "header.h"

#include <cstddef>
#include <cstdint>
#include <sstream>

// SGCMessage
std::string
SGCMessage::fmtString() const
{
    return "fmtString() UnImplemented";
}

// Heartbeat

void
Heartbeat::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kHeartbeat);
    bw.write(header);

    size_t payload_start = bw.position();
    bw.write(hb_seq_);
    bw.write(kv_);
    for (const auto& gsi : gsis_)
    {
        bw.write(gsi);
    }

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
Heartbeat::Deserialize(ByteReader& br)
{
    hb_seq_ = br.read<uint32_t>();

    kv_ = br.read<SGC::KeyVerifier>();

    while (!br.eof())
    {
        auto gsi = br.read<SGC::GroupSessionInfo>();
        gsis_.emplace_back(gsi);
    }
}

// HeartbeatAck
HeartbeatAck::HeartbeatAck(SGC::State st, uint32_t pid, uint32_t hb_seq)
    : state_(st),
      pid_(pid),
      hb_seq_(hb_seq)
{
}

void
HeartbeatAck::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kHeartbeatAck);
    bw.write(header);
    size_t payload_start = bw.position();

    bw.write(static_cast<uint32_t>(state_));
    bw.write(pid_);
    bw.write(hb_seq_);

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
HeartbeatAck::Deserialize(ByteReader& br)
{
    auto state_32 = br.read<uint32_t>();
    state_ = static_cast<SGC::State>(state_32);
    pid_ = br.read<uint32_t>();
    hb_seq_ = br.read<uint32_t>();
}

std::string
HeartbeatAck::fmtString() const
{
    std::stringstream ss;
    ss << "{state_=" << state_ << ", pid_=" << pid_ << ", hb_seq_=" << hb_seq_ << "}";
    return ss.str();
}

// NotifyPosition
void
NotifyPosition::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kNotifyPosition);
    bw.write(header);
    size_t payload_start = bw.position();

    bw.write(sid_);
    bw.write(pos_);
    bw.write(pid_);

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
NotifyPosition::Deserialize(ByteReader& br)
{
    const uint8_t* sid_p = br.readBytes(SGC::SidLength);
    sid_ = std::vector<uint8_t>(sid_p, sid_p + SGC::SidLength);
    pos_ = br.read<uint32_t>();
    pid_ = br.read<uint32_t>();
}

// Join
void
Join::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kJoin);
    bw.write(header);
    size_t payload_start = bw.position();

    bw.write(pid_);
    bw.write(kam_);

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
Join::Deserialize(ByteReader& br)
{
    pid_ = br.read<uint32_t>();
    kam_ = br.read<SAAGKA::KAMaterial>();
}

// JoinAck
void
JoinAck::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kJoinAck);
    bw.write(header);
    size_t payload_start = bw.position();

    bw.write(sid_);
    bw.write(pos_);
    bw.write(pk_id_);
    bw.write(d_);
    bw.write(ek_);

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
JoinAck::Deserialize(ByteReader& br)
{
    const uint8_t* sid_p = br.readBytes(SGC::SidLength);
    sid_.clear();
    sid_.insert(sid_.end(), sid_p, sid_p + SGC::SidLength);

    pos_ = br.read<uint32_t>();
    pk_id_ = br.read<uint32_t>();
    d_ = br.read<G1>();
    ek_ = br.read<SAAGKA::EncryptionKey>();
}

// KeyEncapNotify
void
KeyEncapNotify::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kKeyEncapNotify);
    bw.write(header);
    size_t payload_start = bw.position();

    bw.write(pid_);
    bw.write(key_length_);
    bw.write(cur_key_version_);

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
KeyEncapNotify::Deserialize(ByteReader& br)
{
    pid_ = br.read<uint32_t>();
    key_length_ = br.read<uint32_t>();
    cur_key_version_ = br.read<uint32_t>();
}

// KeyEncap
void
KeyEncap::Serialize(ByteWriter& bw) const
{
    Header header(MsgType::kKeyEncap);
    bw.write(header);
    size_t payload_start = bw.position();

    bw.write(group_num_);
    bw.write(pid_);
    bw.write(kv_);

    for (size_t i = 0; i < sids_.size(); ++i)
    {
        bw.write(sids_[i]);
    }
    bw.write(ct_);

    header.payload_len_ = bw.position() - payload_start;
    bw.patch_u32(payload_start - sizeof(uint32_t), header.payload_len_);
}

void
KeyEncap::Deserialize(ByteReader& br)
{
    group_num_ = br.read<uint32_t>();
    pid_ = br.read<uint32_t>();
    kv_ = br.read<SGC::KeyVerifier>();
    for (size_t i = 0; i < group_num_; ++i)
    {
        const uint8_t* sid_p = br.readBytes(SGC::SidLength);
        sids_.emplace_back(sid_p, sid_p + SGC::SidLength);
    }
    ct_ = br.read<SAAGKA::Ciphertext>();
}
