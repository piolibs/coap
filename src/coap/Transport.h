/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace coap {

// UDP socket used by the client, implemented by the platform (e.g. WiFiUDP)
class Transport {
public:
    virtual ~Transport() = default;

    // Resolves a host name to an address string, false if it fails
    virtual bool resolve(const std::string& host, std::string& address) = 0;

    // Sends one datagram, false if it couldn't be queued
    virtual bool send(const std::string& address, uint16_t port,
                      const uint8_t* data, size_t size) = 0;

    // Waits up to timeoutMs for a datagram and copies up to capacity bytes of it.
    // Returns the datagram size, 0 if nothing arrived.
    virtual size_t receive(uint8_t* buffer, size_t capacity,
                           std::string& fromAddress, uint16_t& fromPort,
                           uint32_t timeoutMs) = 0;
};

}  // namespace coap
