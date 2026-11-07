/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Transport.h"

namespace coap {

class Client {
public:
    // Fields are set only if the server sent them
    struct Result {
        std::optional<uint32_t> timestamp;
        std::optional<int16_t> reportInterval;
        std::optional<uint8_t> skipEmptyReports;
        std::optional<int16_t> alfaSensor;
        std::optional<int16_t> betaSensor;
    };

    Client(Transport& transport, std::string server, uint16_t port);

    // Sends a GET for the resource (a Uri-Path of up to 12 bytes) and waits for
    // a response with the same message ID from the server. Returns true if
    // a valid payload arrived, the values are in data.
    bool sync(const std::string& resource, uint16_t messageId);

    Result data;

private:
    Transport& transport_;
    std::string server_;
    uint16_t port_;
};

}  // namespace coap
