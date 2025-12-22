#pragma once

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
};

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
