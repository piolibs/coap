/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

#include <coap.h>

using namespace coap;

// Transport that returns a canned reply and records what was sent
class FakeTransport : public Transport {
public:
    std::string resolvedHost;
    bool resolveOk = true;
    bool sendOk = true;

    std::vector<uint8_t> sent;
    std::string sentAddress;
    uint16_t sentPort = 0;

    std::vector<uint8_t> reply;
    std::string replyFrom = "10.0.0.5";
    uint16_t replyPort = 5683;

    bool resolve(const std::string& host, std::string& address) override
    {
        resolvedHost = host;
        address = "10.0.0.5";
        return resolveOk;
    }

    bool send(const std::string& address, uint16_t port,
              const uint8_t* data, size_t size) override
    {
        sentAddress = address;
        sentPort = port;
        sent.assign(data, data + size);
        return sendOk;
    }

    size_t receive(uint8_t* buffer, size_t capacity,
                   std::string& fromAddress, uint16_t& fromPort, uint32_t) override
    {
        fromAddress = replyFrom;
        fromPort = replyPort;
        if (reply.empty()) return 0;
        size_t n = reply.size() < capacity ? reply.size() : capacity;
        memcpy(buffer, reply.data(), n);
        return n;
    }
};

// Response with the message ID and an empty token, followed by the payload
static std::vector<uint8_t> response(uint16_t messageId, const std::vector<uint8_t>& payload,
                                     const std::vector<uint8_t>& options = {})
{
    std::vector<uint8_t> packet = {0x60, 0x45,
                                   static_cast<uint8_t>(messageId >> 8),
                                   static_cast<uint8_t>(messageId & 0xFF)};
    packet.insert(packet.end(), options.begin(), options.end());
    packet.push_back(0xFF);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

// CBOR map with the timestamp (id 1) and report interval (id 2)
static const std::vector<uint8_t> kPayload = {
    0xA2,                                // map of 2
    0x01, 0x1A, 0x6A, 0x2F, 0x5C, 0x00,  // timestamp, uint32
    0x02, 0x19, 0x01, 0x2C,              // report interval 300
};

void setUp() {}
void tearDown() {}

void test_sync_parses_payload()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_TRUE(c.sync("aabbccddeeff", 0x1234));

    TEST_ASSERT_EQUAL_STRING("nas.home", t.resolvedHost.c_str());
    TEST_ASSERT_EQUAL_STRING("10.0.0.5", t.sentAddress.c_str());
    TEST_ASSERT_EQUAL_UINT16(5683, t.sentPort);
    TEST_ASSERT_TRUE(c.data.timestamp.has_value());
    TEST_ASSERT_EQUAL_UINT32(0x6A2F5C00, *c.data.timestamp);
    TEST_ASSERT_EQUAL_INT16(300, *c.data.reportInterval);
    TEST_ASSERT_FALSE(c.data.alfaSensor.has_value());
}

void test_request_packet_layout()
{
    FakeTransport t;
    t.reply = response(0x0102, kPayload);

    Client c(t, "nas.home", 5683);
    c.sync("aabb", 0x0102);

    const std::vector<uint8_t> expected = {
        0x40, 0x01, 0x01, 0x02,
        0xB6, 'd', 'e', 'v', 'i', 'c', 'e',
        0x04, 'a', 'a', 'b', 'b'};
    TEST_ASSERT_EQUAL_UINT32(expected.size(), t.sent.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), t.sent.data(), expected.size());
}

void test_rejects_wrong_message_id()
{
    FakeTransport t;
    t.reply = response(0x9999, kPayload);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
    TEST_ASSERT_FALSE(c.data.timestamp.has_value());
}

void test_rejects_token()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload);
    t.reply[0] = 0x61;  // token length 1

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_rejects_reply_from_other_host()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload);
    t.replyFrom = "10.0.0.99";

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_rejects_reply_from_other_port()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload);
    t.replyPort = 5684;

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_rejects_error_code()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload);
    t.reply[1] = 0x84;  // 4.04 Not Found

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_no_reply()
{
    FakeTransport t;

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_send_failure()
{
    FakeTransport t;
    t.sendOk = false;
    t.reply = response(0x1234, kPayload);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_resolve_failure()
{
    FakeTransport t;
    t.resolveOk = false;
    t.reply = response(0x1234, kPayload);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
    TEST_ASSERT_EQUAL_UINT32(0, t.sent.size());
}

void test_resource_too_long()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("0123456789abcdef", 0x1234));
    TEST_ASSERT_EQUAL_UINT32(0, t.sent.size());
}

void test_truncated_payload()
{
    FakeTransport t;
    // The map claims 2 entries, the second one is cut off
    std::vector<uint8_t> payload = {0xA2, 0x01, 0x1A, 0x6A, 0x2F, 0x5C, 0x00, 0x02, 0x19};
    t.reply = response(0x1234, payload);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
    TEST_ASSERT_FALSE(c.data.timestamp.has_value());
}

void test_not_a_map()
{
    FakeTransport t;
    t.reply = response(0x1234, {0x01});

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

void test_old_timestamp_is_ignored()
{
    FakeTransport t;
    // Timestamp 1000 (before 2024), report interval 300
    t.reply = response(0x1234, {0xA2, 0x01, 0x19, 0x03, 0xE8, 0x02, 0x19, 0x01, 0x2C});

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_TRUE(c.sync("aabb", 0x1234));
    TEST_ASSERT_FALSE(c.data.timestamp.has_value());
    TEST_ASSERT_EQUAL_INT16(300, *c.data.reportInterval);
}

void test_extended_option_length_is_skipped()
{
    FakeTransport t;
    // Option byte 0xDD: delta 13 (one extension byte 0x0B), length 13 (one
    // extension byte 0x00 means 13 bytes), then 13 bytes of option value
    std::vector<uint8_t> options = {0xDD, 0x0B, 0x00};
    options.insert(options.end(), 13, 0x00);
    t.reply = response(0x1234, kPayload, options);

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_TRUE(c.sync("aabb", 0x1234));
    TEST_ASSERT_EQUAL_UINT32(0x6A2F5C00, *c.data.timestamp);
}

void test_reserved_option_nibble_rejected()
{
    FakeTransport t;
    t.reply = response(0x1234, kPayload, {0xF0});

    Client c(t, "nas.home", 5683);
    TEST_ASSERT_FALSE(c.sync("aabb", 0x1234));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_sync_parses_payload);
    RUN_TEST(test_request_packet_layout);
    RUN_TEST(test_rejects_wrong_message_id);
    RUN_TEST(test_rejects_token);
    RUN_TEST(test_rejects_reply_from_other_host);
    RUN_TEST(test_rejects_reply_from_other_port);
    RUN_TEST(test_rejects_error_code);
    RUN_TEST(test_no_reply);
    RUN_TEST(test_send_failure);
    RUN_TEST(test_resolve_failure);
    RUN_TEST(test_resource_too_long);
    RUN_TEST(test_truncated_payload);
    RUN_TEST(test_not_a_map);
    RUN_TEST(test_old_timestamp_is_ignored);
    RUN_TEST(test_extended_option_length_is_skipped);
    RUN_TEST(test_reserved_option_nibble_rejected);
    return UNITY_END();
}
