// Transmit and receive on the default Meshtastic channel.
// SX1262 on ESP32; adjust the pins below for your board.

#include <SPI.h>
#include <libmeshtastic_leaf.h>

// Override these for your board, either here or with -D build flags.
#ifndef LORA_CS
#define LORA_CS 18
#endif
#ifndef LORA_IRQ
#define LORA_IRQ 26
#endif
#ifndef LORA_RST
#define LORA_RST 14
#endif
#ifndef LORA_BUSY
#define LORA_BUSY 33
#endif
#ifndef MESH_REGION
#define MESH_REGION REGION_US
#endif
// DIO3 drives the TCXO on many SX1262 boards. It has to be right in begin(),
// since the chip is calibrated against that clock; 0 means no TCXO.
#ifndef LORA_TCXO_VOLTAGE
#define LORA_TCXO_VOLTAGE 0.0f
#endif

SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY);
libmeshtastic_leaf::libmeshtastic_leaf mesh;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 30000; // Send every 30 seconds

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
    ; // Wait for serial

  Serial.println("libmeshtastic_leaf Basic Send/Receive Example");
  Serial.println("=========================================");

#if defined(LORA_SCK) && defined(LORA_MISO) && defined(LORA_MOSI)
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
#else
  SPI.begin();
#endif

  libmeshtastic_leaf::RadioConfig radioConfig;
  radioConfig.region = libmeshtastic_leaf::MESH_REGION;
  radioConfig.preset = libmeshtastic_leaf::PRESET_LONG_FAST;

  // Modem values and sync word below are placeholders mesh.begin() rewrites.
  // The mesh sync word is 0x2B, not RadioLib's private default.
  int st = radio.begin(434.0f, 125.0f, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                       10, 8, LORA_TCXO_VOLTAGE);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("ERROR: radio.begin() failed: ");
    Serial.println(st);
    while (1) {
      delay(1000);
    }
  }
#ifdef LORA_DIO2_AS_RF_SWITCH
  radio.setDio2AsRfSwitch(true);
#endif
  radio.setCRC(2); // Meshtastic requires LoRa CRC
  radio.setCurrentLimit(140.0f);

  libmeshtastic_leaf::MeshConfig meshConfig;
  meshConfig.radio = radioConfig;
  meshConfig.hopLimit = 3;

  // A throwaway key each boot, so this is a different node each time. Real
  // applications generate one once and keep it in NVS or EEPROM.
  uint8_t publicKey[32], privateKey[32];
  if (!libmeshtastic_leaf::libmeshtastic_leaf::generateKeyPair(publicKey,
                                                               privateKey)) {
    // Only happens if the entropy source is broken. Carrying on would
    // give this node an identity the mesh rejects.
    Serial.println("ERROR: could not generate a usable keypair!");
    while (1) {
      delay(1000);
    }
  }
  mesh.setMyPublicKey(publicKey);
  mesh.setMyPrivateKey(privateKey);
  mesh.setOwner("Leaf Node", "Leaf");

  if (!mesh.begin(meshConfig, &radio)) {
    Serial.println("ERROR: Failed to initialize libmeshtastic_leaf!");
    while (1) {
      delay(1000);
    }
  }

  mesh.setDefaultChannel();

  Serial.println("Initialization complete!");
  Serial.print("Node number: 0x");
  Serial.println(mesh.getNodeNum(), HEX);

  char shortName[5];
  libmeshtastic_leaf::MeshNodeId::getShortName(mesh.getNodeNum(), shortName);
  Serial.print("Short name: !");
  Serial.println(shortName);

  Serial.print("Channel hash: 0x");
  Serial.println(mesh.getChannelHash(), HEX);
  Serial.print("Region: ");
  Serial.print(
      libmeshtastic_leaf::MeshRegion::getRegionName(radioConfig.region));
  Serial.print(" (");
  Serial.print(libmeshtastic_leaf::MeshRegion::getFrequency(
                   radioConfig.region, radioConfig.preset, ""),
               3);
  Serial.println(" MHz)");
  Serial.print("Preset: ");
  Serial.println(
      libmeshtastic_leaf::MeshRegion::getPresetName(radioConfig.preset));
  Serial.println();
}

void loop() {
  mesh.update();

  if (mesh.available()) {
    libmeshtastic_leaf::MeshPacket packet;
    libmeshtastic_leaf::ReceiveResult result = mesh.receive(packet);

    if (result == libmeshtastic_leaf::ReceiveResult::OK) {
      Serial.println("--- Received Packet ---");
      Serial.print("From: 0x");
      Serial.println(packet.header.from, HEX);
      Serial.print("To: 0x");
      Serial.println(packet.header.to, HEX);
      Serial.print("Port: ");
      Serial.println(packet.portNum);
      Serial.print("RSSI: ");
      Serial.print(packet.rxRssi);
      Serial.println(" dBm");
      Serial.print("SNR: ");
      Serial.print(packet.rxSnr);
      Serial.println(" dB");

      if (packet.portNum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        Serial.print("Message: ");
        packet.payload[packet.payloadLen] = '\0';
        Serial.println((char *)packet.payload);
      } else {
        Serial.print("Data (");
        Serial.print(packet.payloadLen);
        Serial.println(" bytes)");
      }
      Serial.println("-----------------------");
      Serial.println();
    } else {
      Serial.print("Receive error: ");
      Serial.println((int)result);
    }
  }

  if (millis() - lastSendTime > SEND_INTERVAL) {
    lastSendTime = millis();

    Serial.println("Sending broadcast message...");

    uint32_t packetId = mesh.sendText("Hello from libmeshtastic_leaf!");

    if (packetId != 0) {
      Serial.print("Sent! Packet ID: 0x");
      Serial.println(packetId, HEX);
    } else {
      Serial.println("Failed to send!");
    }
    Serial.println();
  }
}
