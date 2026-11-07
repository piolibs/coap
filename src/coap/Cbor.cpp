#include <cstdint>
#include <optional>
#include <vector>

#include "Cbor.h"

#define SIZE_GE(_size)                                  \
    do {                                                \
        if (pos_ + _size > data_.size())                \
            return std::nullopt;                        \
    } while (false)

namespace coap {

Cbor::Cbor(const std::vector<uint8_t>& data, size_t position)
: data_(data), pos_(position) {}

std::optional<uint8_t> Cbor::parseParameterID()
{
    SIZE_GE(1);
    return data_[pos_++];
}

std::optional<uint32_t> Cbor::parseNumber()
{
    SIZE_GE(1);

    uint8_t type = data_[pos_++];

    if (type <= 0x17)  // small unsigned integer, the value is in the type byte
    {
        return type;
    }

    switch (type)
    {
        case 0x18:  // 1-byte unsigned
        case 0x38:  // 1-byte negative
        {
            SIZE_GE(1);
            uint32_t value = data_[pos_];
            pos_ += 1;
            return (type == 0x18) ? value : ((-1) - value);
        }

        case 0x19:  // 2-byte integer
        case 0x39:  // 2-byte signed negative
        {
            SIZE_GE(2);
            uint32_t value = (data_[pos_] << 8) | data_[pos_ + 1];
            pos_ += 2;

            return (type == 0x19) ? value : ((-1) - value);
        }

        case 0x1A:  // 4-byte unsigned
        case 0x3A:  // 4-byte signed negative
        {
            SIZE_GE(4);
            uint32_t value = (static_cast<uint32_t>(data_[pos_]) << 24) |
                             (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                             (static_cast<uint32_t>(data_[pos_ + 2]) << 8) |
                             data_[pos_ + 3];
            pos_ += 4;

            return (type == 0x1A) ? value : ((-1) - value);
        }

        default:
            break;
    }

    return std::nullopt;
}

std::optional<size_t> Cbor::parseMap()
{
    SIZE_GE(1);

    uint8_t major = data_[pos_] >> 5;
    uint8_t minor = data_[pos_] & 0x1F;

    if ((major == 5) && (minor < 24))  // the map
    {
        pos_++;
        return minor;  // size
    }

    return std::nullopt;
}

bool Cbor::isEmpty() const
{
    return pos_ >= data_.size();
}

bool Cbor::isType(Cbor::Type type) const
{
    if (isEmpty())
    {
        return false;
    }

    uint8_t value = data_[pos_];

    if (type == Type::MAP)
    {
        return (value & 0xE0) == Type::MAP;
    }

    return (type != Type::UNKNOWN) && (value == type);
}

} // namespace coap
