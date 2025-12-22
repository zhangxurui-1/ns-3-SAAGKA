#include "../crypto/agka.h"
#include "../sgc.h"
#include "big.h"
#include "header.h"
#include "pairing_1.h"

#include <cstddef>
#include <cstdint>

class ByteWriter
{
  public:
    explicit ByteWriter(std::vector<uint8_t>& out)
        : out_(out)
    {
    }

    // basic type
    template <typename T>
    std::enable_if_t<std::is_integral_v<T>> write(T v)
    {
        for (int i = sizeof(T) - 1; i >= 0; --i)
        {
            out_.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
        }
    }

    // byte array
    void write(const uint8_t* data, size_t len);

    // std::vector<uint8_t>
    void write(const std::vector<uint8_t>& v);

    // NOTE: total_lenth = length + 2B
    // -------------------------
    // | length (2B) | payload |
    // -------------------------
    void write(const Big& b);

    // NOTE: total_length = length_x + length_y + length_z + 6B
    // -----------------------------------------------------------
    // | length_x (2B) | length_y (2B) | length_z (2B) | payload |
    // -----------------------------------------------------------
    void write(const G1& g1);

    // NOTE: total_length = length_x + length_y + 4B
    // -------------------------------------------
    // | length_x (2B) | length_y (2B) | payload |
    // -------------------------------------------
    void write(const GT& gt);

    void write(const std::chrono::time_point<std::chrono::steady_clock>& tp);

    // NOTE: This method does not encode z_{ii}, which the node should keep secret.
    // format:
    // -------------------------------------------------------------------------------
    // | pk_id (4B) | size_param (4B) | pos (4B) | sid (16B) | u | z[0] | ... | z[n] |
    // -------------------------------------------------------------------------------
    void write(const SAAGKA::KAMaterial& kam);

    void write(const SAAGKA::EncryptionKey& ek);

    // format:
    // -----------------------------------------------------------------------------
    // | len_ (4B) | key_verifier (44B) | c1 | c2[0] | c3[0] | ... | c2[n] | c3[n] |
    // -----------------------------------------------------------------------------
    void write(const SAAGKA::Ciphertext& ct);

    void write(const Header& header);

    // format:
    // ----------------------------------------------
    // | version (4B) | timestamp (8B) | hash (32B) |
    // ----------------------------------------------
    void write(const SGC::KeyVerifier& kv);

    // format:
    // --------------------------------------
    // | total length (4B) | n_member_ (4B) |
    // --------------------------------------
    // |               sid                  |
    // |              (16B)                 |
    // --------------------------------------
    // |        expiry time (8B)            |
    // --------------------------------------
    // |             mem_bitmap             |
    // --------------------------------------
    // |                ek                  |
    // --------------------------------------
    void write(const SGC::GroupSessionInfo& gsi);

    void write(const std::string& str);

    template <typename T>
    auto write(const T& obj) -> decltype(obj.Serialize(), void())
    {
        auto tmp = obj.serialize();
        write(tmp);
    }

    size_t position() const;
    void patch_u32(size_t offset, uint32_t v);

  private:
    std::vector<uint8_t>& out_;
};
