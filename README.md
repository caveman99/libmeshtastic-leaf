# libmeshtastic-leaf

A minimal, cross-platform Arduino-compatible C++ library for operating Meshtastic devices as leaf nodes. Enables sending, receiving, and processing Meshtastic packets without routing or rebroadcasting.

## Features

- Send and receive text/data messages over LoRa
- Channel-based PSK encryption (AES-CTR)
- PKI encryption (Curve25519 + AES-CCM)
- Support for multiple radio chips (SX1262, SX1268, SX1276, SX1278, SX1280, LR11x0)
- Cross-platform (ESP32, ESP8266, nRF52, Raspberry Pi, STM32)

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    libmeshtastic-leaf
```

### Arduino IDE

Download and install via the Library Manager or manually place in your libraries folder.

## Dependencies

- jgromes/RadioLib@^7.5.0
- nanopb/Nanopb@^0.4.91
- rweather/Crypto@^0.4.0

## Quick Start

```cpp
#include <MeshtasticLeaf.h>
#include <MeshRadioSX126x.h>

MeshtasticLeaf mesh;
MeshRadioSX126x radio(NSS_PIN, DIO1_PIN, RST_PIN, BUSY_PIN);

void setup() {
    MeshConfig config;
    config.nodeNum = getHardwareNodeId();
    config.region = meshtastic_Config_LoRaConfig_RegionCode_US;
    config.modemPreset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;

    mesh.begin(config, &radio);
    mesh.setDefaultChannel();
}

void loop() {
    mesh.update();

    if (mesh.available()) {
        MeshPacket packet;
        if (mesh.receive(packet) == ReceiveResult::Success) {
            // Handle received packet
        }
    }

    // Send a message
    mesh.sendText("Hello mesh!", BROADCAST_ADDR);
}
```

## API Overview

### Initialization

- `begin(config, radio)` - Initialize with configuration and radio driver
- `end()` - Shutdown the library

### Channels

- `setChannel(psk, len, name)` - Configure channel with custom PSK
- `setDefaultChannel()` - Use the Meshtastic default channel

### Sending Messages

- `sendText(text, dest)` - Send text message
- `sendData(port, data, len, dest, wantAck)` - Send raw data
- `sendTextPKI(text, node, pubKey)` - Send PKI-encrypted text
- `sendDataPKI(port, data, len, node, pubKey, wantAck)` - Send PKI-encrypted data

### Receiving Messages

- `update()` - Process radio events (call in loop)
- `available()` - Check if packet is available
- `receive(packet)` - Receive decoded packet
- `onReceive(callback)` - Set receive callback

### PKI Encryption

- `generateKeyPair(pubKey, privKey)` - Generate Curve25519 keypair
- `setMyPrivateKey(privKey)` - Set node's private key
- `onReceivePKI(callback)` - Set PKI key lookup callback

## Examples

See the `examples/` directory for complete examples:

- **BasicSendReceive** - Simple transmit and receive
- **TextMessaging** - Custom channel messaging
- **PKIEncryption** - Public key encrypted messaging

## License

GPL-3.0
