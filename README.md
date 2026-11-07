# coap

CoAP client for ESP8266 and Arduino. Sends confirmable GET requests over UDP
to a server, checks the response and parses the CBOR settings in its payload
into optional fields.

The library has no socket of its own: the platform provides one through the
`coap::Transport` interface, so the client is also tested on the host.

## Usage

```cpp
#include <coap.h>

class WifiTransport : public coap::Transport {
public:
    bool resolve(const std::string& host, std::string& address) override
    {
        IPAddress ip;
        if (!WiFi.hostByName(host.c_str(), ip, 2000)) return false;
        address = ip.toString().c_str();
        return true;
    }

    bool send(const std::string& address, uint16_t port,
              const uint8_t* data, size_t size) override
    {
        IPAddress ip;
        ip.fromString(address.c_str());
        if (!udp_.beginPacket(ip, port)) return false;
        udp_.write(data, size);
        return udp_.endPacket();
    }

    size_t receive(uint8_t* buffer, size_t capacity,
                   std::string& fromAddress, uint16_t& fromPort,
                   uint32_t timeoutMs) override
    {
        // poll udp_.parsePacket() until timeoutMs, fill fromAddress/fromPort
        ...
    }

private:
    WiFiUDP udp_;
};

WifiTransport transport;
coap::Client client(transport, "nas.home", 5683);

if (client.sync("aabbccddeeff", 0x1234)) {
    if (client.data.timestamp) { /* UNIX epoch, seconds */ }
    if (client.data.reportInterval) { /* seconds */ }
}
```

The message ID is chosen by the caller, the response must repeat it.

## Behaviour

- `sync(resource, messageId)` sends `GET /device/<resource>`. The resource is
  at most 12 bytes, so the Uri-Path option needs no extended length.
- The reply is accepted only with the same message ID, an empty token, a 2.xx
  code and from the server's address and port.
- Options with extended delta and length (13, 14) are skipped; reserved
  values (15) are rejected.
- The payload is a CBOR map. Keys: `0x01` timestamp, `0x02` report interval,
  `0x03` skip empty reports, `0x11`/`0x12` sensor A, `0x13` sensor B.
  A missing or broken value makes the whole reply invalid.
- Timestamps before 2024-01-01 UTC are dropped.
- `data` is reset at the start of each `sync`.

## Tests

```bash
pio test -e native
```

## Limits

- One request at a time, no retransmission.
- Only the CBOR types used by the server are parsed: small unsigned and
  negative integers, 1–4 byte integers, and maps of up to 23 entries.
- Responses are read into a 256-byte buffer, longer datagrams are truncated.

## License

GPL-3.0, see [LICENSE](LICENSE).
