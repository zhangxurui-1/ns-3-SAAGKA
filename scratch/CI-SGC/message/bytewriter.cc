#include "bytewriter.h"

#include "../utils.h"

#include <cstddef>
#include <cstdint>

void
ByteWriter::write(const uint8_t* data, size_t len)
{
    out_.insert(out_.end(), data, data + len);
}

void
ByteWriter::write(const std::vector<uint8_t>& v)
{
    out_.insert(out_.end(), v.begin(), v.end());
}

void
ByteWriter::write(const Big& b)
{
    char tmp[1024];

    uint16_t l = to_binary(b, 1024, tmp);
    write(l);
    write(reinterpret_cast<uint8_t*>(tmp), l);
}

void
ByteWriter::write(const G1& g1)
{
    ZZn x, y, z;
    extract(const_cast<ECn&>(g1.g), x, y, z);

    Big bx(x);
    Big by(y);
    Big bz(z);

    char tmp_x[1024];
    char tmp_y[1024];
    char tmp_z[1024];

    uint16_t lx = to_binary(bx, 1024, tmp_x);
    write(lx);
    uint16_t ly = to_binary(by, 1024, tmp_y);
    write(ly);
    uint16_t lz = to_binary(bz, 1024, tmp_z);
    write(lz);

    write(reinterpret_cast<uint8_t*>(tmp_x), lx);
    write(reinterpret_cast<uint8_t*>(tmp_y), ly);
    write(reinterpret_cast<uint8_t*>(tmp_z), lz);
}

void
ByteWriter::write(const GT& gt)
{
    ZZn x, y;
    gt.g.get(x, y);

    Big bx(x);
    Big by(y);

    char tmp_x[1024];
    char tmp_y[1024];

    uint16_t lx = to_binary(bx, 1024, tmp_x);
    write(lx);
    uint16_t ly = to_binary(by, 1024, tmp_y);
    write(ly);

    write(reinterpret_cast<uint8_t*>(tmp_x), lx);
    write(reinterpret_cast<uint8_t*>(tmp_y), ly);
}

void
ByteWriter::write(const std::chrono::time_point<std::chrono::steady_clock>& tp)
{
    int64_t us =
        std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();

    write(us);
}

void
ByteWriter::write(const SAAGKA::KAMaterial& kam)
{
    write(kam.pk_id);
    write(kam.size_param);
    write(kam.pos);
    write(kam.sid);
    write(kam.u);

    for (size_t i = 0; i < kam.z.size(); i++)
    {
        if (i == kam.pos)
        {
            continue;
        }
        write(kam.z[i]);
    }
}

void
ByteWriter::write(const SAAGKA::EncryptionKey& ek)
{
    write(ek.lambda);
    write(ek.mu);
}

void
ByteWriter::write(const SAAGKA::Ciphertext& ct)
{
    write(ct.len_);
    write(ct.c1_);

    for (size_t i = 0; i < ct.c2_.size(); i++)
    {
        write(ct.c2_[i]);
        write(ct.c3_[i]);
    }
}

void
ByteWriter::write(const Header& header)
{
    write(static_cast<uint32_t>(header.type_));
    write(header.payload_len_);
}

size_t
ByteWriter::position() const
{
    return out_.size();
}

void
ByteWriter::patch_u32(size_t offset, uint32_t v)
{
    for (int i = 3; i >= 0; --i)
    {
        out_[offset + (3 - i)] = (v >> (8 * i)) & 0xff;
    }
}

void
ByteWriter::write(const SGC::KeyVerifier& kv)
{
    write(kv.version_);
    write(kv.timestamp_);
    write(kv.hash_);
}

void
ByteWriter::write(const SGC::GroupSessionInfo& gsi)
{
    uint32_t total_len = 0;
    size_t start = position();

    write(total_len);
    write(gsi.n_member_);
    write(gsi.sid_);
    write(gsi.expiry_time_);
    write(gsi.mem_bitmap_);
    write(gsi.ek_);

    total_len = position() - start;
    patch_u32(start, total_len);
}

void
ByteWriter::write(const std::string& str)
{
    write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}
