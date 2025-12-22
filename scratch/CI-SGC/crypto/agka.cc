#include "agka.h"

#include "../message/bytewriter.h"
#include "../utils.h"
#include "pki.h"
#include "utils.h"

#include "ns3/singleton.h"

#include <algorithm>
#include <big.h>
#include <cstddef>
#include <cstdint>
#include <ecn.h>
#include <miracl.h>
#include <pairing_1.h>
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
uint32_t SAAGKA::pk_counter_ = 0;

void
SAAGKA::KeyGen()
{
    if (!is_setup_)
    {
        FATAL_ERROR("KeyGen failed: not setup yet");
        return;
    }
    auto pfc = pp_->pfc;
    pfc->random(sk_.x1);
    pfc->random(sk_.x2);
    pk_.y1 = pfc->mult(pp_->generator_1, sk_.x1);
    pk_.y2 = pfc->mult(pp_->generator_1, sk_.x2);
    is_key_used_ = false;
    pk_id_ = pk_counter_;
    ns3::Singleton<PKI>::Get()->Upload(pk_id_, pk_);
    pk_counter_++;
}

void
SAAGKA::UpdateKey(const KAMaterial& kam)
{
    auto& matrix = pp_->matrices[kam.size_param];
    auto pfc = pp_->pfc;
    auto scale = matrix.size();

    auto pk = ns3::Singleton<PKI>::Get()->Get(kam.pk_id);
    auto pseudo_pk = SAAGKA::PublicKey{matrix[kam.pos][scale + 1], matrix[kam.pos][scale + 2]};
    Big v = SAAGKA::HashAnyToBig(kam.sid, pk, kam.u);
    Big pseudo_v = SAAGKA::HashAnyToBig(std::vector<uint8_t>(), pseudo_pk, matrix[kam.pos][scale]);
    G1 tmp = pk.y1 + pfc->mult(pk.y2, v) + (-pseudo_pk.y1) + (-pfc->mult(pseudo_pk.y2, pseudo_v));

    GT delta_mu = pfc->pairing(tmp, pp_->g0);
    G1 delta_lamda = kam.u + (-matrix[kam.pos][scale]);

    ek_.lambda = ek_.lambda + delta_lamda;
    ek_.mu = ek_.mu * delta_mu;

    dk_ = dk_ + (-matrix[kam.pos][pos_]) + kam.z[pos_];
}

SAAGKA::KAMaterial
SAAGKA::MessageGen(const std::vector<uint8_t>& sid,
                   const EncryptionKey& cur_ek,
                   int size_param,
                   int pos)
{
    if (is_key_used_)
    {
        KeyGen();
    }

    auto pfc = pp_->pfc;
    Big w;
    pfc->random(w);

    KAMaterial kam;
    kam.pk_id = pk_id_;
    kam.size_param = size_param;
    kam.pos = pos;
    kam.u = pfc->mult(pp_->generator_1, w);
    kam.sid = std::vector<uint8_t>(sid);

    Big v = HashAnyToBig(kam.sid, pk_, kam.u);
    for (int j = 0; j < pp_->matrices[size_param].size(); ++j)
    {
        G1 elem = pfc->mult(pp_->g0, sk_.x1 + (v * sk_.x2)) + pfc->mult(pp_->h[j], w);
        kam.z.push_back(elem);
        if (j == pos)
        {
            reserved_z_ = elem;
        }
    }

    is_key_used_ = true;
    pending_pos_ = pos;

    // set pending ek
    auto& matrix = pp_->matrices[size_param];
    auto scale = matrix.size();

    auto pseudo_pk = SAAGKA::PublicKey{matrix[pos][scale + 1], matrix[pos][scale + 2]};
    Big pseudo_v = SAAGKA::HashAnyToBig(std::vector<uint8_t>(), pseudo_pk, matrix[pos][scale]);
    G1 tmp = pk_.y1 + pfc->mult(pk_.y2, v) + (-pseudo_pk.y1) + (-pfc->mult(pseudo_pk.y2, pseudo_v));

    GT delta_mu = pfc->pairing(tmp, pp_->g0);
    G1 delta_lamda = kam.u + (-matrix[pos][scale]);

    pending_ek_.lambda = cur_ek.lambda + delta_lamda;
    pending_ek_.mu = cur_ek.mu * delta_mu;
    sid_ = sid;

    return kam;
}

bool
SAAGKA::AsymKeyDerive(const std::vector<uint8_t>& sid,
                      uint32_t pos,
                      const G1& d,
                      const EncryptionKey& expected_ek)
{
    if (!IsEqual(sid, sid_) || pending_pos_ != pos)
    {
        INFO("AsymKeyDerive failed: sid or position not match");
        return false;
    }

    // INFO("pending_ek_=" << pending_ek_);
    // INFO("expected_ek_=" << expected_ek);

    if (pending_ek_ != expected_ek)
    {
        WARN("AsymKeyDerive failed: encryption key not expected");
        return false;
    }

    // finish aysmmetric key agreement
    dk_ = d + reserved_z_;
    ek_ = pending_ek_;
    pos_ = pending_pos_;

    return true;
    // INFO("Asymmetric key derived, Group-" << ParseGroupSeqFromSid(sid) << ", member-" << pos_
    //                                       << ", ek=" << ek_ << ", dk=" << ToString(dk_));
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

        for (; j < size_param; ++j)
        {
            if (i == j)
            {
                continue;
            }
            Big v = HashAnyToBig(std::vector<uint8_t>(), PublicKey{y1, y2}, u);
            m[i][j] = pfc->mult(pp_->h[j], w) + pfc->mult(pp_->g0, x1 + (x2 * v));
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
SAAGKA::HashAnyToBig(const std::vector<uint8_t>& m, const PublicKey& pk, const G1& elem)
{
    std::vector<uint8_t> buf;
    ByteWriter bw(buf);

    bw.write(m);
    bw.write(pk.y1);
    bw.write(pk.y2);
    bw.write(elem);

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

bool
SAAGKA::CheckValid(const KAMaterial& kam)
{
    auto pk = ns3::Singleton<PKI>::Get()->Get(kam.pk_id);
    std::vector<Big> r(pp_->matrices[kam.size_param].size());

    Big r_sum = 0;
    G1 left_G1;
    G1 h_prod_G1;
    left_G1.g.clear();
    h_prod_G1.g.clear();
    auto pfc = pp_->pfc;

    for (int i = 0; i < r.size(); i++)
    {
        pfc->random(r[i]);

        if (i != kam.pos)
        {
            r_sum += r[i];
            left_G1 = left_G1 + pfc->mult(kam.z[i], r[i]);
            h_prod_G1 = h_prod_G1 + pfc->mult(pp_->h[i], r[i]);
        }
    }
    GT left_GT = pfc->pairing(left_G1, pp_->generator_1);

    // INFO("SAAGKA::CheckValid, left_GT=" << ToString(left_GT));

    Big v = HashAnyToBig(kam.sid, pk, kam.u);
    // INFO("SAAGKA::CheckValid, v=" << v);

    GT right1_GT = pfc->pairing(pk.y1 + pfc->mult(pk.y2, v), pfc->mult(pp_->g0, r_sum));
    GT right2_GT = pfc->pairing(h_prod_G1, kam.u);
    GT right_GT = right1_GT * right2_GT;
    // INFO("SAAGKA::CheckValid, right_GT=" << ToString(right_GT));

    return left_GT == right_GT;
}

SAAGKA::Ciphertext
SAAGKA::Encrypt(const std::vector<uint8_t>& msg, std::vector<EncryptionKey>& eks)
{
    Ciphertext ct;
    Big omega;
    auto pfc = pp_->pfc;
    pfc->random(omega);

    ct.len_ = msg.size();
    ct.c1_ = pfc->mult(pp_->generator_1, omega);
    ct.c2_.resize(eks.size());
    ct.c3_.resize(eks.size());

    for (size_t i = 0; i < eks.size(); ++i)
    {
        ct.c2_[i] = pfc->mult(eks[i].lambda, omega);

        GT tmp = pfc->power(eks[i].mu, omega);
        ct.c3_[i] = BytesXOR(msg, HashGTToBytes(tmp, msg.size()));
    }

    return ct;
}

std::vector<uint8_t>
SAAGKA::Decrypt(const Ciphertext& ct, uint32_t index)
{
    auto pfc = pp_->pfc;

    GT gt1 = pfc->pairing(dk_, ct.c1_);
    GT gt2 = pfc->pairing(pp_->h[pos_], ct.c2_[index]);
    return BytesXOR(ct.c3_[index], HashGTToBytes(gt1 / gt2, ct.c3_[index].size()));
}

std::vector<uint8_t>
SAAGKA::HashGTToBytes(const GT& gt, uint32_t length)
{
    std::vector<uint8_t> input;
    uint32_t nonce = 0;
    ByteWriter bw(input);
    bw.write(gt);
    bw.write(nonce);

    std::vector<uint8_t> res(0);
    res.reserve(length);

    char hash_res[32];

    while (res.size() < length)
    {
        bw.patch_u32(bw.position() - sizeof(uint32_t), nonce);
        nonce++;

        Sha256(reinterpret_cast<const char*>(input.data()), input.size(), hash_res);

        int delta_length = std::min((int)(length - res.size()), 32);
        res.insert(res.end(), hash_res, hash_res + delta_length);
    }

    return res;
}

SAAGKA::EncryptionKey
SAAGKA::GetEncryptionKey()
{
    return ek_;
}
