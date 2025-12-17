#include "agka.h"

#include <big.h>
#include <cstddef>
#include <cstdint>
#include <ecn.h>
#include <iostream>
#include <miracl.h>
#include <ostream>
#include <pairing_1.h>
#include <sstream>
#include <type_traits>
#include <vector>
#include <zzn.h>

SAAGKA::PublicKey
SAAGKA::GetPublicKey()
{
    return pk_;
}

SAAGKA::PrivateKey
SAAGKA::GetPrivateKey()
{
    return sk_;
}

bool SAAGKA::is_setup_ = false;
std::shared_ptr<SAAGKA::PublicParameter> SAAGKA::pp_ = nullptr;

void
SAAGKA::KeyGen()
{
    if (!is_setup_)
    {
        // std::cerr << "Scheme not setup, KeyGen failed" << std::endl;
        return;
    }
    auto pfc = pp_->pfc;
    pfc->random(sk_.x1);
    pfc->random(sk_.x2);
    pk_.y1 = pfc->mult(pp_->generator_1, sk_.x1);
    pk_.y2 = pfc->mult(pp_->generator_1, sk_.x2);
    is_key_used_ = false;
}

SAAGKA::KAMaterial
SAAGKA::MessageGen(std::vector<uint8_t> sid, int i)
{
    if (is_key_used_)
    {
        KeyGen();
    }
    int l = ParseSid(sid);
    auto pfc = pp_->pfc;
    Big w;
    pfc->random(w);
    KAMaterial kam;
    kam.size_param = l;
    kam.pos = i;
    kam.u = pfc->mult(pp_->generator_1, w);

    HashInputItem hit{sid, pk_, kam.u};
    Big v = HashAnyToBig(hit);
    for (int j = 0; j < pp_->matrices[l].size(); ++j)
    {
        G1 elem = pfc->mult(pp_->generator_1, sk_.x1 + (v * sk_.x2)) + pfc->mult(pp_->h[j], w);
        kam.z.push_back(elem);
        if (j == i)
        {
            reserved_ = elem;
        }
    }

    return kam;
}

// TODO: implement this function
int
SAAGKA::ParseSid(std::vector<uint8_t> sid)
{
    return 0;
}

SAAGKA::Matrix<G1>
SAAGKA::GenOneMatrix(int size_param)
{
    SAAGKA::Matrix<G1> m(size_param, std::vector<G1>(size_param + 3));
    auto pfc = pp_->pfc;
    for (int i = 0; i < size_param; ++i)
    {
        Big x1, x2, w;
        pfc->random(x1);
        pfc->random(x2);
        pfc->random(w);
        int j = 0;
        G1 y1 = pfc->mult(pp_->generator_1, x1);
        G1 y2 = pfc->mult(pp_->generator_1, x2);
        G1 u = pfc->mult(pp_->generator_1, w);

        static std::string s = "SAAGKA::Setup";
        static const std::vector<uint8_t> INIT_MSG(s.begin(), s.end());

        for (; j < size_param; ++j)
        {
            if (i == j)
            {
                continue;
            }
            HashInputItem input{INIT_MSG, PublicKey{y1, y2}, u};
            Big v = HashAnyToBig(input);
            m[i][j] = pfc->mult(pp_->h[j], w) + pfc->mult(pp_->generator_1, x1 + (x2 * v));
        }
        m[i][j] = u;
        ++j;
        m[i][j] = y1;
        ++j;
        m[i][j] = y2;
    }
    return m;
}

void
SAAGKA::Setup(int security_level, int max_group_size)
{
    if (is_setup_)
    {
        return;
    }

    pp_ = std::make_shared<SAAGKA::PublicParameter>(security_level);
    auto pfc = pp_->pfc;

    pfc->random(pp_->generator_1);

    Big exp;
    pfc->random(exp);
    pp_->g0 = pfc->mult(pp_->generator_1, exp);
    pp_->h = std::vector<G1>(max_group_size);
    for (int i = 0; i < max_group_size; i++)
    {
        pfc->random(exp);
        pp_->h[i] = pfc->mult(pp_->generator_1, exp);
    }

    pp_->matrices = std::vector<Matrix<G1>>();
    int size_param = 10;
    do
    {
        pp_->matrices.push_back(GenOneMatrix(size_param));
        size_param += 10;
    } while (size_param < max_group_size);

    is_setup_ = true;
}

bool
SAAGKA::IsSetup()
{
    return is_setup_;
}

std::shared_ptr<SAAGKA::PublicParameter>
SAAGKA::GetPublicParameter()
{
    if (!is_setup_)
    {
        return nullptr;
    }
    return pp_;
}

Big
SAAGKA::HashAnyToBig(const HashInputItem& item)
{
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), item.m.begin(), item.m.end());

    auto append_g1 = [&](const G1& elem) {
        auto bytes = SerializeG1(elem);
        buf.insert(buf.end(), bytes.begin(), bytes.end());
    };

    append_g1(item.pk.y1);
    append_g1(item.pk.y2);
    append_g1(item.u);

    char hash_res[32];
    Sha256(reinterpret_cast<const char*>(buf.data()), buf.size(), hash_res);

    Big x = from_binary(32, hash_res);
    Big q = pp_->pfc->order();
    x %= q;
    if (x == 0)
    {
        x = 1;
    }

    return x;
}

// NOTE: Serialize does not encode z_{ii}, which the node should keep secret.
std::vector<uint8_t>
SAAGKA::Serialize(const KAMaterial& kam)
{
    Header header(kam);
    auto bytes = header.Serialize();

    auto ubytes = SerializeG1(kam.u);
    bytes.insert(bytes.end(), ubytes.begin(), ubytes.end());
    for (int i = 0; i < kam.z.size(); i++)
    {
        if (i == kam.pos)
        {
            continue;
        }
        auto tmp = SerializeG1(kam.z[i]);
        bytes.insert(bytes.end(), tmp.begin(), tmp.end());
    }
    return bytes;
}

void
SAAGKA::Deserialize(const uint8_t* bytes, int len, KAMaterial& kam)
{
    if (len < 4)
    {
        return;
    }

    Header header;
    header.Deserialize(bytes, 4);
    if (header.type != MsgType::kJoin)
    {
        return;
    }

    kam.size_param = header.info[0];
    kam.pos = header.info[1];
    auto p = bytes + 4;
    auto end = bytes + len;
    if (p + 4 > end)
    {
        return;
    }
    int lu = ((p[0] << 8) | p[1]) + ((p[2] << 8) | p[3]) + 4;
    if ((p + lu) <= end)
    {
        kam.u = DeserializeG1(p, lu);
    }
    p += lu;

    kam.z.resize(pp_->matrices[kam.size_param].size());
    for (int i = 0; i < kam.z.size(); i++)
    {
        if (i == kam.pos)
        {
            continue;
        }

        if (p + 4 > end)
        {
            break;
        }
        int lz = ((p[0] << 8) | p[1]) + ((p[2] << 8) | p[3]) + 4;
        if ((p + lz) > end)
        {
            break;
        }
        kam.z[i] = DeserializeG1(p, lz);
        p += lz;
    }
}

std::string
SAAGKA::G1ToString(const G1& elem)
{
    ZZn coordinate_a, coordinate_b;
    extract(const_cast<ECn&>(elem.g), coordinate_a, coordinate_b);

    std::stringstream ss;
    ss << "(" << coordinate_a << ", " << coordinate_b << ")";
    return ss.str();
}

// Header
SAAGKA::Header::Header(const KAMaterial& kam)
    : type(MsgType::kJoin)
{
    info[0] = static_cast<uint8_t>(kam.size_param);
    info[1] = static_cast<uint8_t>(kam.pos);
    info[2] = 0;
}

std::vector<uint8_t>
SAAGKA::Header::Serialize() const
{
    std::vector<uint8_t> bytes(4);
    bytes[0] = static_cast<uint8_t>(type);
    for (int i = 0; i < 3; i++)
    {
        bytes[i + 1] = info[i];
    }
    return bytes;
}

void
SAAGKA::Header::Deserialize(const uint8_t* bytes, int len)
{
    if (len < 4)
    {
        return;
    }
    type = static_cast<MsgType>(bytes[0]);
    info[0] = bytes[1];
    info[1] = bytes[2];
    info[2] = 0;
}
