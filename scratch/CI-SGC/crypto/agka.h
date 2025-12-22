#pragma once

#include "MIRACL-wrapper.h"
#include "utils.h"

#include <big.h>
#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>

class SAAGKA
{
  public:
    struct PrivateKey
    {
        Big x1, x2;

        friend std::ostream& operator<<(std::ostream& os, const PrivateKey& sk)
        {
            os << "(x1=" << sk.x1 << ", x2=" << sk.x2 << ")";
            return os;
        }
    };

    struct PublicKey
    {
        G1 y1, y2;

        friend std::ostream& operator<<(std::ostream& os, const PublicKey& pk)
        {
            ZZn coordinate_a1, coordinate_b1;
            ZZn coordinate_a2, coordinate_b2;
            extract(const_cast<ECn&>(pk.y1.g), coordinate_a1, coordinate_b1);
            extract(const_cast<ECn&>(pk.y2.g), coordinate_a2, coordinate_b2);

            os << "(y1=(" << coordinate_a1 << ", " << coordinate_b1 << "), y2=(" << coordinate_a2
               << ", " << coordinate_b2 << "))";
            return os;
        }
    };

    template <typename T>
    using Matrix = std::vector<std::vector<T>>;

    struct PublicParameter
    {
        std::shared_ptr<PFC> pfc;
        G1 generator_1; // generator of G1
        G1 g0;
        std::vector<G1> h;
        std::vector<Matrix<G1>> matrices;

        PublicParameter(int security_level)
            : pfc(std::make_shared<PFC>(security_level))
        {
        }
    };

    // key agreement material, aka OTBMS signature
    struct KAMaterial
    {
        uint32_t pk_id;
        uint32_t size_param;
        uint32_t pos;
        std::vector<uint8_t> sid;
        G1 u;
        std::vector<G1> z;

        friend ostream& operator<<(ostream& os, const KAMaterial& kam)
        {
            os << "{pk_id=" << kam.pk_id << ", size_param=" << kam.size_param << ", pos=" << kam.pos
               << ", m=" << ToString(kam.sid) << ", u=" << ToString(kam.u) << ", z=(\n";
            for (int i = 0; i < kam.z.size(); i++)
            {
                if (i == kam.pos)
                {
                    continue;
                }
                os << "\t" << i << ":" << ToString(kam.z[i]) << ",\n";
            }
            os << ")}";
            return os;
        }
    };

    struct EncryptionKey
    {
        G1 lambda;
        GT mu;

        EncryptionKey(EncryptionKey& ek)
            : lambda(ek.lambda),
              mu(ek.mu)
        {
        }

        EncryptionKey() = default;

        EncryptionKey(const EncryptionKey& ek)
            : lambda(ek.lambda),
              mu(ek.mu)
        {
        }

        friend ostream& operator<<(ostream& os, const EncryptionKey& ek)
        {
            os << "{lambda=" << ToString(ek.lambda) << ", mu=" << ToString(ek.mu) << "}";
            return os;
        }
    };

    typedef G1 DecryptionKey;

    struct Ciphertext
    {
        uint32_t len_;
        G1 c1_;
        std::vector<G1> c2_;
        std::vector<std::vector<uint8_t>> c3_;

        friend ostream& operator<<(ostream& os, const Ciphertext& ct)
        {
            os << "{len_=" << ct.len_ << ", c1_=" << ToString(ct.c1_) << ", c2_=(\n";
            for (int i = 0; i < ct.c2_.size(); i++)
            {
                os << "\t" << i << ":" << ToString(ct.c2_[i]) << ",\n";
            }
            os << "), c3_=(\n";
            for (int i = 0; i < ct.c3_.size(); i++)
            {
                os << "\t" << i << ":" << ToString(ct.c3_[i]) << ",\n";
            }
            os << ")}";
            return os;
        }
    };

    SAAGKA()
        : is_key_used_(false) {};
    virtual ~SAAGKA() {};
    KAMaterial MessageGen(const std::vector<uint8_t>& sid,
                          const EncryptionKey& cur_ek,
                          int size_param,
                          int pos);
    bool AsymKeyDerive(const std::vector<uint8_t>& sid,
                       uint32_t pos,
                       const G1& d,
                       const EncryptionKey& expected_ek);
    void KeyGen();
    std::vector<uint8_t> Decrypt(const Ciphertext& ct, uint32_t index);
    PublicKey GetPublicKey();
    PrivateKey GetPrivateKey();
    EncryptionKey GetEncryptionKey();
    void UpdateKey(const KAMaterial& kam);

    static Big HashAnyToBig(const std::vector<uint8_t>& m, const PublicKey& pk, const G1& elem);
    static std::vector<uint8_t> HashGTToBytes(const GT& gt, uint32_t length);

    static void Setup(int security_level, int max_group_size);
    static std::shared_ptr<PublicParameter> GetPublicParameter();
    static bool IsSetup();
    static bool CheckValid(const KAMaterial& kam);
    static Ciphertext Encrypt(const std::vector<uint8_t>& msg, std::vector<EncryptionKey>& eks);
    // TODO: Encrypt and Decrypt functions

  private:
    static bool is_setup_;
    static std::shared_ptr<PublicParameter> pp_;
    static uint32_t pk_counter_;

    static Matrix<G1> GenOneMatrix(int size_param);

    bool is_key_used_;
    PrivateKey sk_;
    PublicKey pk_;
    uint32_t pk_id_;
    EncryptionKey ek_;
    DecryptionKey dk_;
    std::vector<uint8_t> sid_;
    uint32_t pos_;

    // pending values for latest session
    EncryptionKey pending_ek_;
    uint32_t pending_pos_;
    G1 reserved_z_;
};

inline bool
operator==(const SAAGKA::EncryptionKey& a, const SAAGKA::EncryptionKey& b)
{
    return a.lambda == b.lambda && a.mu == b.mu;
}

inline bool
operator!=(const SAAGKA::EncryptionKey& a, const SAAGKA::EncryptionKey& b)
{
    return !(a == b);
}
