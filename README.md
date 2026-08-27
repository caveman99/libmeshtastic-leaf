# libmeshtastic-leaf

A Meshtastic leaf node as an Arduino library. It sends, receives and decrypts
Meshtastic packets. It does not route, rebroadcast, or keep a node database.

The radio is driven entirely through RadioLib's generic `PhysicalLayer`
interface, so any LoRa chip RadioLib supports works without a driver in this
library. See [ARCHITECTURE.md](ARCHITECTURE.md) for where the line between the
two sits, and what this library deliberately does not do.

## Status

Working: packet framing, channel (PSK) encryption, PKI encryption, region and
preset tables, `Data` payload encoding.

Not implemented yet: the MAC layer. There is no carrier sense, no contention
window, no airtime accounting and no duty cycle limiting, so transmissions can
talk over ongoing traffic and are **not compliant with EU duty cycle limits**.
The frequency slot calculation is also incomplete for regions whose profile has
non-zero channel spacing or padding.

## Installation

PlatformIO:

```ini
lib_deps =
    https://github.com/caveman99/libmeshtastic-leaf.git
```

Arduino IDE: install through the Library Manager, or copy the repository into
your libraries folder.

### Dependencies

| Dependency                                                  | Note                                     |
| ----------------------------------------------------------- | ---------------------------------------- |
| `jgromes/RadioLib@^7.7.1`                                   | the radio driver                         |
| `nanopb/Nanopb@^0.4.91`                                     | runtime for the generated `Data` message |
| [`meshtastic/Crypto`](https://github.com/meshtastic/Crypto) | AES, Curve25519, SHA256                  |

## Quick start

The sketch owns chip specific bring-up: pins, TCXO, current limit, RF switch,
and enabling LoRa CRC, which Meshtastic requires and `PhysicalLayer` does not
expose. This library then applies frequency, spreading factor, bandwidth,
coding rate, sync word, preamble and power for the configured region and preset.

```cpp
#include <RadioLib.h>
#include <libmeshtastic_leaf.h>

SX1262 radio = new Module(18, 26, 14, 33);
libmeshtastic_leaf::libmeshtastic_leaf mesh;

void setup() {
    Serial.begin(115200);
    SPI.begin();

    radio.begin();
    radio.setTCXO(1.8f);
    radio.setCRC(2);
    radio.setCurrentLimit(140.0f);

    libmeshtastic_leaf::MeshConfig config;
    config.nodeNum = libmeshtastic_leaf::MeshNodeId::getNodeNum();
    config.hopLimit = 3;
    config.radio.region = libmeshtastic_leaf::REGION_EU_868;
    config.radio.preset = libmeshtastic_leaf::PRESET_LONG_FAST;

    mesh.begin(config, &radio);
    mesh.setDefaultChannel();
}

void loop() {
    mesh.update();

    if (mesh.available()) {
        libmeshtastic_leaf::MeshPacket packet;
        if (mesh.receive(packet) == libmeshtastic_leaf::ReceiveResult::OK) {
            Serial.write(packet.payload, packet.payloadLen);
        }
    }
}
```

Everything lives in namespace `libmeshtastic_leaf`.

## API

### Lifecycle

| Call                                            | Meaning                                                            |
| ----------------------------------------------- | ------------------------------------------------------------------ |
| `bool begin(const MeshConfig&, PhysicalLayer*)` | apply RF config, set the default channel, start receiving          |
| `void end()`                                    | stop receiving and put the radio to sleep                          |
| `void update()`                                 | call from `loop()`; drains the radio and runs the receive callback |

`MeshConfig` holds `nodeNum`, `hopLimit` and a `RadioConfig` with `region`,
`preset`, and optional `frequency` and `txPower` overrides. Leave the last two
at zero to take the region defaults.

### Channels

| Call                                                                     | Meaning                             |
| ------------------------------------------------------------------------ | ----------------------------------- |
| `bool setChannel(const uint8_t *psk, size_t len, const char *name = "")` | set the PSK and channel name        |
| `void setDefaultChannel()`                                               | use the Meshtastic public channel   |
| `ChannelHash getChannelHash() const`                                     | the hash byte that goes on the wire |

A PSK length of 0 disables encryption, 1 selects a default key by index, 16 is
AES128 and 32 is AES256. Other lengths are zero padded up to the next of those.

### Sending

| Call                                                                                                                              | Returns                    |
| --------------------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| `uint32_t sendText(const char *text, NodeNum dest = BROADCAST_ADDR)`                                                              | packet id, or 0 on failure |
| `uint32_t sendData(meshtastic_PortNum, const uint8_t *data, size_t len, NodeNum dest, bool wantAck)`                              | packet id, or 0            |
| `uint32_t sendTextPKI(const char *text, NodeNum dest, const uint8_t pubKey[32])`                                                  | packet id, or 0            |
| `uint32_t sendDataPKI(meshtastic_PortNum, const uint8_t *data, size_t len, NodeNum dest, const uint8_t pubKey[32], bool wantAck)` | packet id, or 0            |

Sending blocks until the transmission completes.

### Receiving

| Call                                 | Meaning                                   |
| ------------------------------------ | ----------------------------------------- |
| `bool available()`                   | a decoded packet is waiting               |
| `ReceiveResult receive(MeshPacket&)` | take it; `ReceiveResult::OK` on success   |
| `void onReceive(PacketCallback)`     | called from `update()` instead of polling |

`MeshPacket` carries the header, `portNum`, `payload`, `payloadLen`, `rxRssi`,
`rxSnr` and `isPKI`.

### PKI

| Call                                                             | Meaning                                      |
| ---------------------------------------------------------------- | -------------------------------------------- |
| `static void generateKeyPair(uint8_t pub[32], uint8_t priv[32])` | new Curve25519 keypair                       |
| `void setMyPrivateKey(const uint8_t priv[32])`                   | required before PKI traffic                  |
| `void onReceivePKI(PKIKeyLookup)`                                | supply a sender's public key when decrypting |

Key storage is the application's job. This library keeps keys in RAM and never
writes to flash.

### Helpers

`MeshNodeId` derives a node number from the hardware: `getNodeNum()`,
`nodeNumFromMac()`, `getShortName()`, `getLastByte()`.

`MeshRegion` looks up regulatory and modem data: `getRegion()`,
`getRegionName()`, `getDefaultFrequency()`, `getPowerLimit()`, `isWideLoRa()`,
`getModemParams()`, `getPresetName()`, `getAllRegions()`.

## Examples

Each example is a self contained PlatformIO project. Build one by changing into
its directory and running `pio run`.

| Example                     | Shows                                       |
| --------------------------- | ------------------------------------------- |
| `examples/BasicSendReceive` | transmit and receive on the default channel |
| `examples/TextMessaging`    | a custom channel and PSK                    |
| `examples/PKIEncryption`    | encrypted direct messages                   |

## Development

### Tests

```sh
cd test-native
pio test -e native
```

The tests are a host build and need no board. They live in their own PlatformIO
project: a `platformio.ini` at the repository root makes PlatformIO treat
`library.json` as the project manifest and try to build RadioLib and Crypto for
the host, which cannot work.

### Protobufs

A leaf only ever parses the `Data` submessage of `mesh.proto`.

```sh
git submodule update --init protobufs
pip install 'nanopb==0.4.9.1' grpcio-tools
python bin/regen-protos.py
```

The generator version decides the exact bytes of the output, so use the pinned
one. CI regenerates and fails if the committed files differ, which catches both
a hand edited generated file and an upstream change not yet pulled in.

### Versioning

`library.json` holds the version and the dependency pins. Everything else is
derived from it.

```sh
python bin/version.py --check     # CI runs this
python bin/version.py --sync      # rewrite derived files
python bin/version.py --set 1.2.0
```

### Releasing

Run the Release workflow from the Actions tab with a `MAJOR.MINOR.PATCH`
version. It advances the protobufs submodule, regenerates, sets the version,
runs the tests and example builds, then commits, tags and publishes. The
`dry_run` input does everything except push.

The write scoped token is never in the same job as repository code: `verify`
runs the scripts and builds with a read only token and emits a patch, and
`publish` applies that patch and runs nothing but `git` and `gh`.

## License

GPL-3.0-only. The library carries code derived from the Meshtastic firmware.
`src/aes-ccm.cpp` is BSD licensed code from hostap by Jouni Malinen.
