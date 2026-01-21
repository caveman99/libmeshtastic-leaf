/**
 * @file TextMessaging.ino
 * @brief Text messaging example with custom channel
 *
 * This example demonstrates:
 * - Setting up a custom channel with a specific PSK
 * - Sending direct messages to specific nodes
 * - Handling received text messages
 * - Serial interface for user input
 *
 * Hardware: ESP32 with SX1262
 */

#include <SPI.h>
#include <libmeshtastic_leaf.h>

// Pin definitions - adjust for your hardware
#define LORA_CS   18
#define LORA_IRQ  26
#define LORA_RST  14
#define LORA_BUSY 33

// Create instances
SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY);
libmeshtastic_leaf::MeshRadioSX1262 meshRadio(&radio);
libmeshtastic_leaf::libmeshtastic_leaf mesh;

// Serial input buffer
char inputBuffer[200];
int inputPos = 0;

// Custom channel PSK (256-bit / 32 bytes for AES256)
// In a real application, use a secure random key!
const uint8_t myChannelPSK[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000);

    Serial.println("libmeshtastic_leaf Text Messaging Example");
    Serial.println("=====================================");

    SPI.begin();

    // Configure radio using region helpers
    libmeshtastic_leaf::RadioConfig radioConfig;
    radioConfig.type = libmeshtastic_leaf::RadioType::SX1262;
    radioConfig.region = libmeshtastic_leaf::REGION_US;  // Set your region
    radioConfig.preset = libmeshtastic_leaf::PRESET_LONG_FAST;
    radioConfig.frequency = libmeshtastic_leaf::MeshRegion::getDefaultFrequency(radioConfig.region);
    radioConfig.txPower = libmeshtastic_leaf::MeshRegion::getPowerLimit(radioConfig.region);

    if (!meshRadio.begin(radioConfig)) {
        Serial.println("ERROR: Radio init failed!");
        while (1) delay(1000);
    }

    // Configure mesh
    libmeshtastic_leaf::MeshConfig meshConfig;
    meshConfig.radio = radioConfig;
    // Get node number from hardware MAC address
    meshConfig.nodeNum = libmeshtastic_leaf::MeshNodeId::getNodeNum();
    meshConfig.hopLimit = 3;

    if (!mesh.begin(meshConfig, &meshRadio)) {
        Serial.println("ERROR: Mesh init failed!");
        while (1) delay(1000);
    }

    // Set up custom channel with AES256 encryption
    mesh.setChannel(myChannelPSK, sizeof(myChannelPSK), "MyChannel");

    Serial.println("Ready!");
    Serial.print("Node: 0x");
    Serial.println(mesh.getNodeNum(), HEX);
    Serial.print("Channel: MyChannel (hash: 0x");
    Serial.print(mesh.getChannelHash(), HEX);
    Serial.println(")");
    Serial.println();
    Serial.println("Commands:");
    Serial.println("  Type a message and press Enter to broadcast");
    Serial.println("  @NODEID message  - Send to specific node (e.g., @12345678 hello)");
    Serial.println();
}

void processCommand(const char* input) {
    if (strlen(input) == 0) return;

    // Check for direct message format: @NODEID message
    if (input[0] == '@') {
        // Parse node ID
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
        Serial.println("Invalid format. Use: @NODEID message");
        return;
    }

    // Broadcast message
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
        if (packet.portNum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
            packet.payload[packet.payloadLen] = '\0';

            Serial.println();
            Serial.print("[");
            Serial.print(packet.rxRssi);
            Serial.print("dBm] From 0x");
            Serial.print(packet.header.from, HEX);
            Serial.print(": ");
            Serial.println((char*)packet.payload);
        }
    }
}

void loop() {
    // Process radio events
    mesh.update();

    // Handle received packets
    while (mesh.available()) {
        handleReceivedPacket();
    }

    // Handle serial input
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
