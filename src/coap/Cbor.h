#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace coap {

class Cbor
{
public:
    explicit Cbor(const std::vector<uint8_t>& data, size_t position = 0);

    std::optional<uint8_t> parseParameterID();

    std::optional<size_t> parseMap();
    std::optional<uint32_t> parseNumber();

    enum Type { UNKNOWN = 0, UINT8 = 0x18, UINT16 = 0x19, UINT32 = 0x1A, MAP = 0xA0 };
    bool isType(Type type) const;

    bool isEmpty() const;

private:
    const std::vector<uint8_t>& data_;
    size_t pos_;
};

} // namespace coap
