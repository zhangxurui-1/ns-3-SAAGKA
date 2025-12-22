#pragma once

#include "bytereader.h"

bool
ByteReader::eof() const
{
    return pos_ == len_;
}

size_t
ByteReader::remaining() const
{
    return len_ - pos_;
}

size_t
ByteReader::position() const
{
    return pos_;
}

uint8_t
ByteReader::readByte()
{
    if (remaining() < 1)
    {
        throw std::runtime_error("ByteReader: underflow");
    }
    return data_[pos_++];
}

const uint8_t*
ByteReader::readBytes(size_t len)
{
    if (remaining() < len)
    {
        throw std::runtime_error("ByteReader: underflow");
    }
    const uint8_t* ret = data_ + pos_;
    pos_ += len;
    return ret;
}
