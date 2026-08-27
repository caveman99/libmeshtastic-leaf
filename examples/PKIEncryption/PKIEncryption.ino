/**
 * @file PKIEncryption.ino
 * @brief PKI (Public Key Infrastructure) encryption example
 *
 * This example demonstrates:
 * - Generating Curve25519 keypairs
 * - Storing and managing public keys for other nodes
 * - Sending PKI-encrypted messages to specific nodes
 * - Receiving and decrypting PKI messages
 *
 * PKI encryption provides end-to-end encryption between two specific
 * nodes, even if the channel PSK is compromised.
 *
 * Hardware: ESP32 with SX1262
 */

#include <SPI.h>
#include <libmeshtastic_leaf.h>

// Pin definitions
#define LORA_CS   18
#define LORA_IRQ  26
#define LORA_RST  14
#define LORA_BUSY 33

// Create instances
SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY);
libmeshtastic_leaf::libmeshtastic_leaf mesh;

// Our keypair (in a real app, store these securely in NVS/EEPROM)
uint8_t myPublicKey[32];
uint8_t myPrivateKey[32];

// Known remote nodes and their public keys
// In a real application, this would be stored in non-volatile memory
// and managed through a key exchange protocol
struct RemoteNode {
    uint32_t nodeNum;
    uint8_t publicKey[32];
};

// Example: Pre-configured remote node (replace with actual values)
RemoteNode knownNodes[] = {
    {
        0x11223344,  // Node ID
        {   // Public key (32 bytes) - replace with actual key!
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        }
    }
};
const int numKnownNodes = sizeof(knownNodes) / sizeof(knownNodes[0]);

/**
 * PKI key lookup callback
 * Called by the library when decrypting PKI packets to find sender's public key
 */
bool lookupPublicKey(uint32_t nodeNum, uint8_t pubKey[32]) {
    for (int i = 0; i < numKnownNodes; i++) {
        if (knownNodes[i].nodeNum == nodeNum) {
            memcpy(pubKey, knownNodes[i].publicKey, 32);
            return true;
        }
    }
    Serial.print("Unknown PKI sender: 0x");
    Serial.println(nodeNum, HEX);
    return false;
}

void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000);

    Serial.println("libmeshtastic_leaf PKI Encryption Example");
    Serial.println("=====================================");

    SPI.begin();

    // Configure radio using region helpers
    libmeshtastic_leaf::RadioConfig radioConfig;
    radioConfig.region = libmeshtastic_leaf::REGION_US;  // Set your region
    radioConfig.preset = libmeshtastic_leaf::PRESET_LONG_FAST;
    radioConfig.frequency = libmeshtastic_leaf::MeshRegion::getDefaultFrequency(radioConfig.region);
    radioConfig.txPower = libmeshtastic_leaf::MeshRegion::getPowerLimit(radioConfig.region);

    // Chip-specific bring-up is the sketch's job. The library only speaks
    // the generic RadioLib PhysicalLayer API.
    int st = radio.begin();
    if (st != RADIOLIB_ERR_NONE) {
        Serial.print("ERROR: radio.begin() failed: ");
        Serial.println(st);
        while (1) delay(1000);
    }
    radio.setTCXO(1.8f);              // adjust for your module, omit if none
    radio.setCRC(2);                  // Meshtastic requires LoRa CRC
    radio.setCurrentLimit(140.0f);

    // Configure mesh
    libmeshtastic_leaf::MeshConfig meshConfig;
    meshConfig.radio = radioConfig;
    // Get node number from hardware MAC address
    meshConfig.nodeNum = libmeshtastic_leaf::MeshNodeId::getNodeNum();
    meshConfig.hopLimit = 3;

    if (!mesh.begin(meshConfig, &radio)) {
        Serial.println("ERROR: Mesh init failed!");
        while (1) delay(1000);
    }

    // Generate or load keypair
    // In a real application, check if keys exist in NVS first
    Serial.println("Generating Curve25519 keypair...");
    libmeshtastic_leaf::libmeshtastic_leaf::generateKeyPair(myPublicKey, myPrivateKey);

    // Set our private key for PKI operations
    mesh.setMyPrivateKey(myPrivateKey);

    // Register the key lookup callback
    mesh.onReceivePKI(lookupPublicKey);

    // Also set up a channel for non-PKI messages
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

// Serial input
char inputBuffer[200];
int inputPos = 0;

void processCommand(const char* input) {
    if (strlen(input) == 0) return;

    // PKI message: #NODEID message
    if (input[0] == '#') {
        char* space = strchr(input, ' ');
        if (space && (space - input) > 1) {
            char nodeIdStr[16];
            int len = space - input - 1;
            if (len > 15) len = 15;
            strncpy(nodeIdStr, input + 1, len);
            nodeIdStr[len] = '\0';

            uint32_t destNode = strtoul(nodeIdStr, NULL, 16);
            const char* message = space + 1;

            if (destNode != 0 && strlen(message) > 0) {
                // Find the public key for this node
                const uint8_t* remotePubKey = nullptr;
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

    // Direct message: @NODEID message
    if (input[0] == '@') {
        char* space = strchr(input, ' ');
        if (space && (space - input) > 1) {
            char nodeIdStr[16];
            int len = space - input - 1;
            if (len > 15) len = 15;
            strncpy(nodeIdStr, input + 1, len);
            nodeIdStr[len] = '\0';

            uint32_t destNode = strtoul(nodeIdStr, NULL, 16);
            const char* message = space + 1;

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

    // Broadcast
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
            Serial.println((char*)packet.payload);
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
