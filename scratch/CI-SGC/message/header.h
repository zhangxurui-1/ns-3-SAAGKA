#pragma once

#include <ostream>
enum class MsgType : uint32_t
{
    kUnknown,
    kHeartbeat,
    kHeartbeatAck,
    kNotifyPosition,
    kJoin,
    kJoinAck,
    kKeyEncapNotify,
    kKeyEncap,
    kKeyUpdate,
    kKeyUpdateAck,

    kMsgTypeNum,
};

inline std::ostream&
operator<<(std::ostream& os, MsgType t)
{
    switch (t)
    {
    case MsgType::kUnknown:
        os << "kUnknown";
        break;
    case MsgType::kHeartbeat:
        os << "kHeartbeat";
        break;
    case MsgType::kHeartbeatAck:
        os << "kHeartbeatAck";
        break;
    case MsgType::kNotifyPosition:
        os << "kNotifyPosition";
        break;
    case MsgType::kJoin:
        os << "kJoin";
        break;
    case MsgType::kJoinAck:
        os << "kJoinAck";
        break;
    case MsgType::kKeyEncapNotify:
        os << "kKeyEncapNotify";
        break;
    case MsgType::kKeyEncap:
        os << "kKeyEncap";
        break;
    case MsgType::kKeyUpdate:
        os << "kKeyUpdate";
        break;
    case MsgType::kKeyUpdateAck:
        os << "kKeyUpdateAck";
        break;
    default:
        os << "Unknown MsgType";
        break;
    }

    return os;
}

class Header
{
  public:
    const static int HeaderSize = sizeof(MsgType) + sizeof(uint32_t);

    MsgType type_;
    volatile uint32_t payload_len_;

    Header(MsgType t)
        : type_(t)
    {
    }

    Header() {};
};
