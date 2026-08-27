# Architecture

This library is the Meshtastic stack above the radio. RadioLib is the radio.
The rule that keeps the two apart: **this library holds one `PhysicalLayer*`
and calls the generic API on it. It never wraps a chip, and never re-abstracts
something RadioLib already abstracts.**

## RadioLib owns this

Do not wrap it, do not re-declare it, do not add a driver for it.

- SPI, GPIO and timing, through `RadioLibHal`
- chip registers and IRQ pin wiring
- `setFrequency`
- spreading factor, bandwidth and coding rate, set atomically through
  `setDataRate(DataRate_t, ModemType_t)`
- `setSyncWord`, `setPreambleLength`, `setOutputPower`, `checkOutputPower`
- `startTransmit`, `readData`, `startReceive`, `standby`, `sleep`
- `scanChannel(ChannelScanConfig_t)` for carrier sense
- `getRSSI`, `getSNR`, `getPacketLength`
- `getTimeOnAir`, `calculateTimeOnAir`
- `randomByte`, `random`
- `setPacketReceivedAction`, `setPacketSentAction`
- chip errata: the SX1262 RX sensitivity register patch, RX boosted gain, the
  RFM95 frequency limits

A per-chip class in this library is always wrong. `PhysicalLayer` covers
SX126x, SX127x, SX128x, LR11x0 and LR2021 through one pointer.

## The sketch owns this

`PhysicalLayer` does not expose chip specific setup, and this library will not
guess at it. The sketch constructs the radio, calls `begin()`, and configures:

- pin assignment
- TCXO voltage
- current limit
- RF switch or DIO2 control
- **LoRa CRC**, which Meshtastic requires for interoperability

Only then does it hand the radio to `begin(config, &radio)`.

## This library owns this

| Layer          | Contents                                                                                           |
| -------------- | -------------------------------------------------------------------------------------------------- |
| RF policy      | region table, region profiles, slot to frequency calculation, power clamping, preset display names |
| Modem presets  | preset to spreading factor, bandwidth and coding rate                                              |
| Framing        | the 16 byte header, packed and unpacked explicitly as little endian                                |
| Channel crypto | PSK expansion, channel hash, AES-CTR with the Meshtastic nonce layout                              |
| PKI            | X25519 to SHA256 to AES-256-CCM, and the 12 byte wire overhead                                     |
| Payload        | `Data` encoding and decoding, portnum dispatch                                                     |
| MAC            | carrier sense, contention window, airtime accounting, duty cycle gate                              |

### Not a leaf, never port these

Flooding and next-hop routing, rebroadcast, hop limit decrement, the node
database, the mesh service, the phone API, MQTT, modules, the screen, the power
state machine, configuration persistence, admin messages.

### Still missing

Duplicate suppression, acknowledgement and retransmission, and dwell time
limiting. See the status section in [README.md](README.md).

## Transmission

`send*()` stages one frame and returns; `update()` drives it. The states are
backoff, then carrier sense, then transmit, and a frame is refused outright if
it would take the hour's transmit airtime past the duty cycle limit.

Two airtime windows, because they answer different questions. Our own
transmissions over a rolling hour give the duty cycle. Everything heard plus
everything sent over the last minute gives channel utilisation, which sizes the
contention window between `cwMin` and `cwMax`.

Slot time is `max(2.25, NUM_SYM_CAD + 0.5) * 2^SF / BW` plus 7.6 ms for
propagation, turnaround and processing. On LongFast that is about 28 ms, so an
idle channel backs off up to about 200 ms and a saturated one up to about 7
seconds. That is why staging and returning is the only workable shape for
`send*()`.

Policy that stays with the application: region choice, licensed or exempt
status, and what to do when a send is refused.

## Wire formats

### Header

16 bytes, little endian, always in clear:

| Offset | Size | Field                                    |
| ------ | ---- | ---------------------------------------- |
| 0      | 4    | destination node                         |
| 4      | 4    | sender node                              |
| 8      | 4    | packet id                                |
| 12     | 1    | flags                                    |
| 13     | 1    | channel hash                             |
| 14     | 1    | next hop, last byte of the node number   |
| 15     | 1    | relay node, last byte of the node number |

Flags: bits 0 to 2 hop limit, bit 3 want ack, bit 4 via MQTT, bits 5 to 7 hop
start. The payload follows immediately, at most 239 bytes.

The header is packed and unpacked byte by byte, so the layout does not depend
on host endianness or struct padding.

### Channel encryption

AES-CTR, with a 16 byte IV and a 4 byte counter:

| Bytes    | Contents                          |
| -------- | --------------------------------- |
| 0 to 3   | packet id, little endian          |
| 4 to 7   | zero, or the extra nonce for PKI  |
| 8 to 11  | sender node number, little endian |
| 12 to 15 | block counter, starts at zero     |

CTR is symmetric, so encrypt and decrypt are the same operation.

### PSK lengths

| Length | Meaning                                                                                                           |
| ------ | ----------------------------------------------------------------------------------------------------------------- |
| 0      | no encryption                                                                                                     |
| 1      | index into the default key; 0 disables, 1 is the default key unchanged, and higher values increment its last byte |
| 16     | AES128                                                                                                            |
| 32     | AES256                                                                                                            |

Anything else is zero padded up to 16 or 32.

The channel hash on the wire is the XOR of the channel name bytes with the
XOR of the expanded key bytes.

### PKI

X25519 shared secret, hashed with SHA256, used as an AES-256-CCM key with an 8
byte tag and a 13 byte nonce. The packet carries ciphertext, then the 8 byte
tag, then a 4 byte extra nonce, for 12 bytes of overhead.

## Persistence

The library owns RAM, the application owns flash. Node number, channel keys and
the PKI keypair are held in memory and handed in by the caller. Nothing here
touches NVS, EEPROM or a filesystem.

## Interrupts

Reception and transmission share one interrupt. On SX126x both
`setPacketReceivedAction()` and `setPacketSentAction()` install the same DIO1
action, so there is a single flag, and the transmit state decides what it
means. `update()` services transmission first and only looks for a received
packet when no transmission is in flight.

RadioLib callbacks take a plain `void(*)(void)` with no context pointer, so the
flag cannot live in the instance. One active instance per process.

There is no RTOS dependency and no thread. A single flag can drop a second
event that arrives before the first is consumed, so a transmission also carries
a deadline of twice its time on air plus 500 ms as a backstop against a missed
interrupt.

## Protobufs

The only protobuf on the air is the `Data` submessage. The 16 byte header is
raw bytes, not protobuf. `MeshPacket` as a protobuf appears only on the serial,
BLE and MQTT sides, none of which exist here.

`Data` is trimmed out of upstream `mesh.proto` at the descriptor level by
`bin/regen-protos.py` rather than copied, so the `protobufs` submodule stays
the single source of truth. Only one translation unit, `MeshPacket.cpp`,
includes the generated header.
