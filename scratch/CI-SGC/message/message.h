#pragma once

#include "../crypto/agka.h"
#include "../crypto/utils.h"
#include "../sgc/sgc-vehicle.h"
#include "../sgc/sgc.h"
#include "bytereader.h"
#include "bytewriter.h"

#include <cstdint>
#include <ostream>
#include <vector>

class SGCMessage
{
  public:
    virtual void Serialize(ByteWriter& bw) const = 0; // serialized bytes contain header
    virtual void Deserialize(ByteReader& br) = 0;     // deserialize payload only (without header)
    virtual std::string fmtString() const;
    virtual ~SGCMessage() = default;
};

class Heartbeat : public SGCMessage
{
  public:
    uint32_t hb_seq_;
    SGC::KeyVerifier kv_;
    std::vector<SGC::GroupSessionInfo> gsis_;

    Heartbeat() = default;

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};

class HeartbeatAck : public SGCMessage
{
  public:
    SGCVehicle::State state_;
    uint32_t pid_;
    uint32_t hb_seq_;

    HeartbeatAck() = default;
    HeartbeatAck(SGCVehicle::State st, uint32_t pid, uint32_t hb_seq);

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
    std::string fmtString() const override;
};

class NotifyPosition : public SGCMessage
{
  public:
    std::vector<uint8_t> sid_;
    uint32_t pos_;
    uint32_t pid_;

    NotifyPosition() = default;

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};

class Join : public SGCMessage
{
  public:
    uint32_t pid_;
    SAAGKA::KAMaterial kam_;

    Join() = default;

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};

class JoinAck : public SGCMessage
{
  public:
    std::vector<uint8_t> sid_;
    uint32_t pos_;
    uint32_t pk_id_;
    G1 d_;
    SAAGKA::EncryptionKey ek_;

    JoinAck() = default;
    virtual ~JoinAck() = default;

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;

    friend std::ostream& operator<<(std::ostream& os, const JoinAck& ack)
    {
        os << "{sid=" << ::ToString(ack.sid_) << ", pos=" << ack.pos_ << ", pk_id=" << ack.pk_id_
           << ", d=" << ::ToString(ack.d_) << ", ek=" << ack.ek_ << "}";
        return os;
    }
};

class KeyEncapNotify : public SGCMessage
{
  public:
    uint32_t pid_;
    uint32_t key_length_;
    uint32_t cur_key_version_;

    KeyEncapNotify() = default;

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};

class KeyEncap : public SGCMessage
{
  public:
    uint32_t group_num_;
    uint32_t pid_;
    std::vector<std::vector<uint8_t>> sids_; // each corresponds to a (ct2, ct3) in ct_
    SGC::KeyVerifier kv_;
    SAAGKA::Ciphertext ct_;

    friend ostream& operator<<(ostream& os, const KeyEncap& key_encap)
    {
        os << "{group_num_=" << key_encap.group_num_ << ", sids_=(\n";
        for (int i = 0; i < key_encap.sids_.size(); i++)
        {
            os << "\t" << i << ":" << ::ToString(key_encap.sids_[i]) << ",\n";
        }
        os << "), ct_=" << key_encap.ct_ << "}";
        return os;
    }

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};

class KeyUpd : public SGCMessage
{
  public:
    uint32_t group_num_;
    uint32_t pid_;
    std::vector<std::vector<uint8_t>> sids_; // each corresponds to a (ct2, ct3) in ct_
    SGC::KeyVerifier kv_;
    SAAGKA::Ciphertext ct_;

    friend ostream& operator<<(ostream& os, const KeyUpd& key_encap)
    {
        os << "{group_num_=" << key_encap.group_num_ << ", sids_=(\n";
        for (int i = 0; i < key_encap.sids_.size(); i++)
        {
            os << "\t" << i << ":" << ::ToString(key_encap.sids_[i]) << ",\n";
        }
        os << "), ct_=" << key_encap.ct_ << "}";
        return os;
    }

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};

class KeyUpdAck : public SGCMessage
{
  public:
    uint32_t pid_;
    SGC::KeyVerifier kv_;

    KeyUpdAck() = default;
    virtual ~KeyUpdAck() = default;

    void Serialize(ByteWriter& bw) const override;
    void Deserialize(ByteReader& br) override;
};
