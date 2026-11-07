/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <cstring>
#include <optional>
#include <utility>
#include <vector>

#include "Client.h"
#include "Cbor.h"

namespace coap {

namespace {

constexpr size_t MAX_RESOURCE = 12;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 2000;
// 2024-01-01 00:00:00 UTC, an older timestamp means a corrupted or fake payload
constexpr uint32_t MIN_EPOCH = 1704067200;

// Skips the options and returns the offset of the payload, or nothing if the
// header is malformed
std::optional<size_t> skipOptions(const std::vector<uint8_t>& packet, size_t pos)
{
    while (pos < packet.size() && packet[pos] != 0xFF)
    {
        uint8_t optionByte = packet[pos++];
        uint8_t delta = optionByte >> 4;
        uint16_t length = optionByte & 0x0F;

        if (delta == 15 || length == 15)
        {
            return std::nullopt;
        }

        if (delta == 13) pos += 1;
        else if (delta == 14) pos += 2;

        if (length == 13)
        {
            if (pos >= packet.size()) return std::nullopt;
            length = packet[pos++] + 13;
        }
        else if (length == 14)
        {
            if (pos + 1 >= packet.size()) return std::nullopt;
            length = ((packet[pos] << 8) | packet[pos + 1]) + 269;
            pos += 2;
        }

        pos += length;
    }

    if (pos >= packet.size() || packet[pos] != 0xFF)
    {
        return std::nullopt;
    }

    return pos + 1;
}

// Parses the CBOR map with the settings, false if a value is missing or broken
bool parsePayload(const std::vector<uint8_t>& packet, size_t offset, Client::Result& result)
{
    Cbor payload(packet, offset);

    if (!payload.isType(Cbor::Type::MAP))
    {
        return false;
    }

    bool valid = true;
    auto remaining = payload.parseMap().value_or(0);

    while (valid && remaining-- && !payload.isEmpty())
    {
        valid = false;

        switch (payload.parseParameterID().value_or(0x00))
        {
            case 0x01:  // Timestamp (UTC)
                if (auto v = payload.parseNumber())
                {
                    valid = true;
                    result.timestamp = static_cast<uint32_t>(*v);
                }
                break;

            case 0x02:  // Report interval (secs)
                if (auto v = payload.parseNumber())
                {
                    valid = true;
                    result.reportInterval = static_cast<uint32_t>(*v);
                }
                break;

            case 0x03:  // Skip empty reports
                if (auto v = payload.parseNumber())
                {
                    valid = true;
                    result.skipEmptyReports = static_cast<uint8_t>(*v) ? true : false;
                }
                break;

            case 0x11:  // sensor A
            case 0x12:
                if (auto v = payload.parseNumber())
                {
                    valid = true;
                    result.alfaSensor = static_cast<int16_t>(*v);
                }
                break;

            case 0x13:  // sensor B
                if (auto v = payload.parseNumber())
                {
                    valid = true;
                    result.betaSensor = static_cast<int16_t>(*v);
                }
                break;

            default:
                break;
        }
    }

    return valid;
}

}  // namespace

Client::Client(Transport& transport, std::string server, uint16_t port)
    : transport_(transport), server_(std::move(server)), port_(port)
{}

bool Client::sync(const std::string& resource, uint16_t messageId)
{
    data = Result();

    if (resource.empty() || resource.size() > MAX_RESOURCE)
    {
        return false;
    }

    // Header: version 1, confirmable, no token, GET
    uint8_t packet[128];
    size_t size = 0;

    packet[size++] = 0x40;
    packet[size++] = 0x01;
    packet[size++] = static_cast<uint8_t>(messageId >> 8);
    packet[size++] = static_cast<uint8_t>(messageId & 0xFF);

    // Uri-Path "device" and the resource
    packet[size++] = 0xB6;  // delta 11, length 6
    memcpy(&packet[size], "device", 6);
    size += 6;

    packet[size++] = static_cast<uint8_t>(resource.size());  // delta 0
    memcpy(&packet[size], resource.data(), resource.size());
    size += resource.size();

    std::string address;
    if (!transport_.resolve(server_, address))
    {
        return false;
    }

    if (!transport_.send(address, port_, packet, size))
    {
        return false;
    }

    // Read the response, skipping datagrams from other senders
    std::vector<uint8_t> reply(256);
    std::string from;
    uint16_t fromPort = 0;

    size_t received = transport_.receive(reply.data(), reply.size(), from, fromPort,
                                         RESPONSE_TIMEOUT_MS);
    if (received == 0 || from != address || fromPort != port_)
    {
        return false;
    }
    reply.resize(received);

    // Header: a 2.xx response with the same message ID and an empty token
    if (reply.size() < 4)
    {
        return false;
    }

    uint8_t tokenLength = reply[0] & 0x0F;
    uint8_t code = reply[1];
    uint16_t id = static_cast<uint16_t>((reply[2] << 8) | reply[3]);

    if ((tokenLength != 0) || (id != messageId) || ((code & 0xE0) != 0x40))
    {
        return false;
    }

    auto offset = skipOptions(reply, 4);
    if (!offset)
    {
        return false;
    }

    if (!parsePayload(reply, *offset, data))
    {
        data = Result();
        return false;
    }

    if (data.timestamp.has_value() && (*data.timestamp < MIN_EPOCH))
    {
        data.timestamp.reset();
    }

    return true;
}

}  // namespace coap
