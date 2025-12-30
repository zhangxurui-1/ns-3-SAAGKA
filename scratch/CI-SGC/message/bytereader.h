#pragma once

#include "../crypto/agka.h"
#include "../sgc/sgc.h"
#include "../utils.h"
#include "header.h"
#include "pairing_1.h"

#include <big.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <zzn2.h>

template <typename T>
struct ByteReadTrait;

class ByteReader
{
  public:
    ByteReader(const uint8_t* data, size_t len)
        : data_(data),
          len_(len),
          pos_(0)
    {
    }

    bool eof() const;
    size_t remaining() const;
    size_t position() const;
    uint8_t readByte();
    const uint8_t* readBytes(size_t len);

    template <typename T>
    T read()
    {
        return ByteReadTrait<T>::read(*this);
    }

  private:
    const uint8_t* data_;
    size_t len_;
    size_t pos_;
};

// template sepcialization
template <typename T>
struct ByteReadTrait
{
    static_assert(std::is_integral_v<T>, "ByteReadTrait not specialized for this type");

    static T read(ByteReader& r)
    {
        if (r.remaining() < sizeof(T))
        {
            throw std::runtime_error("ByteReader: underflow");
        }

        using U = std::make_unsigned_t<T>;
        U v = 0;

        for (size_t i = 0; i < sizeof(T); ++i)
        {
            v = (v << 8) | r.readByte();
        }

        return static_cast<T>(v);
    }
};

template <>
struct ByteReadTrait<G1>
{
    static G1 read(ByteReader& r)
    {
        auto lx = r.read<uint16_t>();
        auto ly = r.read<uint16_t>();
        auto lz = r.read<uint16_t>();

        const uint8_t* px = r.readBytes(lx);
        const uint8_t* py = r.readBytes(ly);
        const uint8_t* pz = r.readBytes(lz);

        Big bx = from_binary(lx, const_cast<char*>(reinterpret_cast<const char*>(px)));
        Big by = from_binary(ly, const_cast<char*>(reinterpret_cast<const char*>(py)));
        Big bz = from_binary(lz, const_cast<char*>(reinterpret_cast<const char*>(pz)));

        ZZn x(bx);
        ZZn y(by);
        ZZn z(bz);

        ECn P;
        force(x, y, z, P);
        G1 g1;
        g1.g = P;

        return g1;
    }
};

template <>
struct ByteReadTrait<GT>
{
    static GT read(ByteReader& r)
    {
        auto lx = r.read<uint16_t>();
        auto ly = r.read<uint16_t>();

        const uint8_t* px = r.readBytes(lx);
        const uint8_t* py = r.readBytes(ly);

        Big bx = from_binary(lx, const_cast<char*>(reinterpret_cast<const char*>(px)));
        Big by = from_binary(ly, const_cast<char*>(reinterpret_cast<const char*>(py)));
        ZZn x(bx);
        ZZn y(by);
        ZZn2 z(x, y);

        GT gt;
        gt.g = z;
        return gt;
    }
};

template <>
struct ByteReadTrait<Big>
{
    static Big read(ByteReader& r)
    {
        auto len = r.read<uint16_t>();
        const uint8_t* p = r.readBytes(len);
        Big b = from_binary(len, const_cast<char*>(reinterpret_cast<const char*>(p)));
        return b;
    }
};

template <>
struct ByteReadTrait<SAAGKA::KAMaterial>
{
    static SAAGKA::KAMaterial read(ByteReader& r)
    {
        SAAGKA::KAMaterial kam;
        kam.pk_id = r.read<uint32_t>();
        kam.size_param = r.read<uint32_t>();
        kam.pos = r.read<uint32_t>();

        const uint8_t* sid_p = r.readBytes(SGC::SidLength);
        kam.sid.insert(kam.sid.end(), sid_p, sid_p + SGC::SidLength);

        kam.u = r.read<G1>();

        size_t scale = SAAGKA::GetPublicParameter()->matrices[kam.size_param].size();
        kam.z.resize(scale);
        for (size_t i = 0; i < kam.z.size(); i++)
        {
            if (i == kam.pos)
            {
                continue;
            }
            kam.z[i] = r.read<G1>();
        }

        return kam;
    }
};

template <>
struct ByteReadTrait<SAAGKA::EncryptionKey>
{
    static SAAGKA::EncryptionKey read(ByteReader& r)
    {
        SAAGKA::EncryptionKey ek;
        ek.lambda = r.read<G1>();
        ek.mu = r.read<GT>();
        return ek;
    }
};

template <>
struct ByteReadTrait<SAAGKA::Ciphertext>
{
    static SAAGKA::Ciphertext read(ByteReader& r)
    {
        SAAGKA::Ciphertext ct;
        ct.len_ = r.read<uint32_t>();
        ct.c1_ = r.read<G1>();

        while (!r.eof())
        {
            ct.c2_.emplace_back(r.read<G1>());

            const uint8_t* c3_p = r.readBytes(ct.len_);
            auto& c3 = ct.c3_.emplace_back();
            c3.reserve(ct.len_);
            c3.insert(c3.end(), c3_p, c3_p + ct.len_);
        }

        return ct;
    }
};

template <>
struct ByteReadTrait<SGC::KeyVerifier>
{
    static SGC::KeyVerifier read(ByteReader& r)
    {
        SGC::KeyVerifier kv;
        kv.version_ = r.read<uint32_t>();
        kv.timestamp_ = r.read<ns3::Time>();

        if (kv.version_ > 0)
        {
            const uint8_t* p = r.readBytes(32);
            kv.hash_.insert(kv.hash_.end(), p, p + 32);
        }

        return kv;
    }
};

template <>
struct ByteReadTrait<SGC::GroupSessionInfo>
{
    static SGC::GroupSessionInfo read(ByteReader& r)
    {
        SGC::GroupSessionInfo gsi;
        r.read<uint32_t>(); // total_len

        gsi.n_member_ = r.read<uint32_t>();

        const uint8_t* sid_p = r.readBytes(SGC::SidLength);
        gsi.sid_.insert(gsi.sid_.end(), sid_p, sid_p + SGC::SidLength);
        gsi.expiry_time_ = r.read<ns3::Time>();

        gsi.size_param_ = ParseSizeParamFromSid(gsi.sid_);
        auto scale = SAAGKA::GetPublicParameter()->matrices[gsi.size_param_].size();
        size_t bm_size = ((scale + 63) / 64) * 8;
        const uint8_t* bm_p = r.readBytes(bm_size);
        gsi.mem_bitmap_.insert(gsi.mem_bitmap_.end(), bm_p, bm_p + bm_size);

        gsi.ek_ = r.read<SAAGKA::EncryptionKey>();

        return gsi;
    }
};

template <>
struct ByteReadTrait<ns3::Time>
{
    static ns3::Time read(ByteReader& r)
    {
        auto t = r.read<int64_t>();
        return ns3::NanoSeconds(t);
    }
};

// NOTE: This function MUST NOT be used in other ByteReadTrait.
template <>
struct ByteReadTrait<Header>
{
    static Header read(ByteReader& r)
    {
        Header h;
        h.type_ = static_cast<MsgType>(r.read<uint32_t>());
        h.payload_len_ = r.read<uint32_t>();
        return h;
    }
};
