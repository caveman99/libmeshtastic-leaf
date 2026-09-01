// Encrypted direct messages using Curve25519 keys.
// SX1262 on ESP32; adjust the pins below for your board.

#include <SPI.h>
#include <libmeshtastic_leaf.h>

#include "MeshCryptoPKI.h"
#include "MeshNodeId.h"

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

// A real application stores these in NVS or EEPROM, not in RAM.
uint8_t myPublicKey[32];
uint8_t myPrivateKey[32];

struct RemoteNode {
  uint32_t nodeNum;
  uint8_t publicKey[32];
};

// Fill both in from the other node. The number is not free: it is the CRC-32
// of the key, which lookupPublicKey() below checks.
RemoteNode knownNodes[] = {
    {0x11223344,
     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};
const int numKnownNodes = sizeof(knownNodes) / sizeof(knownNodes[0]);

// Supplies a sender's public key when decrypting. A node number is the CRC-32
// of the key, so a table entry that disagrees is wrong and must not be used.
bool lookupPublicKey(uint32_t nodeNum, uint8_t pubKey[32]) {
  for (int i = 0; i < numKnownNodes; i++) {
    if (knownNodes[i].nodeNum != nodeNum) {
      continue;
    }
    if (!libmeshtastic_leaf::MeshCryptoPKI::isUsablePublicKey(
            knownNodes[i].publicKey) ||
        libmeshtastic_leaf::MeshNodeId::nodeNumFromPublicKey(
            knownNodes[i].publicKey, 32) != nodeNum) {
      Serial.println("Table entry rejected: key does not match the node "
                     "number");
      return false;
    }
    memcpy(pubKey, knownNodes[i].publicKey, 32);
    return true;
  }
  Serial.print("Unknown PKI sender: 0x");
  Serial.println(nodeNum, HEX);
  return false;
}

void printHex(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10)
      Serial.print('0');
    Serial.print(data[i], HEX);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
    ;

  Serial.println("libmeshtastic_leaf PKI Encryption Example");
  Serial.println("=====================================");

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
    while (1)
      delay(1000);
  }
#ifdef LORA_DIO2_AS_RF_SWITCH
  radio.setDio2AsRfSwitch(true);
#endif
  radio.setCRC(2); // Meshtastic requires LoRa CRC
  radio.setCurrentLimit(140.0f);

  libmeshtastic_leaf::MeshConfig meshConfig;
  meshConfig.radio = radioConfig;
  meshConfig.hopLimit = 3;

  if (!mesh.begin(meshConfig, &radio)) {
    Serial.println("ERROR: Mesh init failed!");
    while (1)
      delay(1000);
  }

  // The node number is the CRC-32 of the public key, so this decides who
  // this node is on the mesh, not just how it encrypts.
  Serial.println("Generating Curve25519 keypair...");
  if (!libmeshtastic_leaf::libmeshtastic_leaf::generateKeyPair(myPublicKey,
                                                               myPrivateKey)) {
    Serial.println("ERROR: could not generate a usable keypair!");
    while (1)
      delay(1000);
  }

  mesh.setMyPublicKey(myPublicKey);
  mesh.setMyPrivateKey(myPrivateKey);
  mesh.setOwner("PKI Leaf", "PKI");

  mesh.onReceivePKI(lookupPublicKey);

  mesh.setDefaultChannel();

  Serial.println("Ready!");
  Serial.println();

  Serial.print("Node ID: 0x");
  Serial.println(mesh.getNodeNum(), HEX);

  Serial.println();
  Serial.println("Your PUBLIC key (share this with other nodes):");
  printHex(myPublicKey, 32);
  Serial.println();

  Serial.println();
  Serial.println("Commands:");
  Serial.println("  message         - Broadcast (channel encrypted)");
  Serial.println("  @NODEID message - Direct message (channel encrypted)");
  Serial.println("  #NODEID message - PKI encrypted message");
  Serial.println();
}

char inputBuffer[200];
int inputPos = 0;

void processCommand(const char *input) {
  if (strlen(input) == 0)
    return;

  if (input[0] == '#') {
    char *space = strchr(input, ' ');
    if (space && (space - input) > 1) {
      char nodeIdStr[16];
      int len = space - input - 1;
      if (len > 15)
        len = 15;
      strncpy(nodeIdStr, input + 1, len);
      nodeIdStr[len] = '\0';

      uint32_t destNode = strtoul(nodeIdStr, NULL, 16);
      const char *message = space + 1;

      if (destNode != 0 && strlen(message) > 0) {
        const uint8_t *remotePubKey = nullptr;
        for (int i = 0; i < numKnownNodes; i++) {
          if (knownNodes[i].nodeNum == destNode) {
            remotePubKey = knownNodes[i].publicKey;
            break;
          }
        }

        if (remotePubKey == nullptr) {
          Serial.print("No public key for node 0x");
          Serial.println(destNode, HEX);
          return;
        }

        Serial.print("PKI encrypting to 0x");
        Serial.print(destNode, HEX);
        Serial.print(": ");
        Serial.println(message);

        uint32_t id = mesh.sendTextPKI(message, destNode, remotePubKey);
        if (id != 0) {
          Serial.print("Sent! ID: 0x");
          Serial.println(id, HEX);
        } else {
          Serial.println("Send failed!");
        }
        return;
      }
    }
    Serial.println("Usage: #NODEID message");
    return;
  }

  if (input[0] == '@') {
    char *space = strchr(input, ' ');
    if (space && (space - input) > 1) {
      char nodeIdStr[16];
      int len = space - input - 1;
      if (len > 15)
        len = 15;
      strncpy(nodeIdStr, input + 1, len);
      nodeIdStr[len] = '\0';

      uint32_t destNode = strtoul(nodeIdStr, NULL, 16);
      const char *message = space + 1;

      if (destNode != 0 && strlen(message) > 0) {
        Serial.print("Sending to 0x");
        Serial.print(destNode, HEX);
        Serial.print(": ");
        Serial.println(message);

        uint32_t id = mesh.sendText(message, destNode);
        if (id != 0) {
          Serial.print("Sent! ID: 0x");
          Serial.println(id, HEX);
        } else {
          Serial.println("Send failed!");
        }
        return;
      }
    }
    Serial.println("Usage: @NODEID message");
    return;
  }

  Serial.print("Broadcasting: ");
  Serial.println(input);
  uint32_t id = mesh.sendText(input);
  if (id != 0) {
    Serial.print("Sent! ID: 0x");
    Serial.println(id, HEX);
  } else {
    Serial.println("Send failed!");
  }
}

void handleReceivedPacket() {
  libmeshtastic_leaf::MeshPacket packet;
  libmeshtastic_leaf::ReceiveResult result = mesh.receive(packet);

  if (result == libmeshtastic_leaf::ReceiveResult::OK) {
    Serial.println();

    if (packet.isPKI) {
      Serial.print("[PKI] ");
    }

    Serial.print("[");
    Serial.print(packet.rxRssi);
    Serial.print("dBm] From 0x");
    Serial.print(packet.header.from, HEX);

    if (packet.header.to != libmeshtastic_leaf::BROADCAST_ADDR) {
      Serial.print(" -> 0x");
      Serial.print(packet.header.to, HEX);
    }

    if (packet.portNum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
      packet.payload[packet.payloadLen] = '\0';
      Serial.print(": ");
      Serial.println((char *)packet.payload);
    } else {
      Serial.print(" [Port ");
      Serial.print(packet.portNum);
      Serial.print(", ");
      Serial.print(packet.payloadLen);
      Serial.println(" bytes]");
    }
  } else if (result == libmeshtastic_leaf::ReceiveResult::PKI_KEY_UNKNOWN) {
    Serial.println("Received PKI packet from unknown sender (no public key)");
  } else if (result != libmeshtastic_leaf::ReceiveResult::NO_PACKET) {
    Serial.print("Receive error: ");
    Serial.println((int)result);
  }
}

void loop() {
  mesh.update();

  while (mesh.available()) {
    handleReceivedPacket();
  }

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputPos > 0) {
        inputBuffer[inputPos] = '\0';
        processCommand(inputBuffer);
        inputPos = 0;
      }
    } else if (inputPos < sizeof(inputBuffer) - 1) {
      inputBuffer[inputPos++] = c;
    }
  }
}
