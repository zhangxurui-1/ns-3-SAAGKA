#include "MIRACL-wrapper.h"
#include "utils.h"

#include <big.h>
#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>

enum class MsgType : uint8_t
{
    kHeartbeat,
    kJoin,
    kKeyDistribution,
    kKeyUpdate,
    kUnknown
};

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

    struct HashInputItem
    {
        std::vector<uint8_t> m;
        PublicKey pk;
        G1 u;

        std::vector<uint8_t> Serialize() const
        {
            std::vector<uint8_t> buf = m;

            return buf;
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
        int size_param;
        int pos;
        G1 u;
        std::vector<G1> z;

        friend ostream& operator<<(ostream& os, const KAMaterial& kam)
        {
            os << "{ size_param=" << kam.size_param << ", pos=" << kam.pos
               << ", u=" << G1ToString(kam.u) << ", z=(\n";
            for (int i = 0; i < kam.z.size(); i++)
            {
                if (i == kam.pos)
                {
                    continue;
                }
                os << "\t" << i << ":" << G1ToString(kam.z[i]) << ",\n";
            }
            os << ")}";
            return os;
        }
    };

    struct EncryptionKey
    {
        G1 lambda;
        GT mu;
    };

    typedef G1 DecryptionKey;

    class Header
    {
      public:
        MsgType type;
        uint8_t info[3]; // this field is reused for various uses

        Header(const KAMaterial& kam);
        Header() = default;
        std::vector<uint8_t> Serialize() const;
        void Deserialize(const uint8_t* bytes, int len);
    };

    SAAGKA()
        : is_key_used_(false) {};
    virtual ~SAAGKA() {};
    KAMaterial MessageGen(std::vector<uint8_t> sid, int i);
    void KeyGen();
    PublicKey GetPublicKey();
    PrivateKey GetPrivateKey();

    static Big HashAnyToBig(const HashInputItem& item);
    static std::vector<uint8_t> HashGTToBytes(const GT& gt);
    static void Setup(int security_level, int max_group_size);
    static std::shared_ptr<PublicParameter> GetPublicParameter();
    static bool IsSetup();
    // ParseSid returns the underlying size parameter l
    static int ParseSid(std::vector<uint8_t> sid);
    static std::vector<uint8_t> Serialize(const KAMaterial& kam);
    static void Deserialize(const uint8_t* bytes, int len, KAMaterial& kam);
    static std::string G1ToString(const G1& elem);
    // TODO: Encrypt and Decrypt functions

  private:
    static bool is_setup_;
    static std::shared_ptr<PublicParameter> pp_;
    static Matrix<G1> GenOneMatrix(int size_param);

    bool is_key_used_;
    PrivateKey sk_;
    PublicKey pk_;
    EncryptionKey ek_;
    DecryptionKey dk_;
    G1 reserved_;
};
