/**
 * @file BasicSendReceive.ino
 * @brief Basic example of sending and receiving Meshtastic packets
 *
 * This example demonstrates:
 * - Initializing the libmeshtastic_leaf library with an SX1262 radio
 * - Configuring the default channel
 * - Sending text messages
 * - Receiving and displaying messages
 *
 * Hardware requirements:
 * - ESP32 or similar board
 * - SX1262 LoRa module connected via SPI
 *
 * Pin configuration (adjust for your board):
 * - CS:   18
 * - IRQ:  26
 * - RST:  14
 * - BUSY: 33
 */

#include <SPI.h>
#include <libmeshtastic_leaf.h>

// Pin definitions - adjust for your hardware
#define LORA_CS   18
#define LORA_IRQ  26
#define LORA_RST  14
#define LORA_BUSY 33

// Create RadioLib module and libmeshtastic_leaf instances
SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY);
libmeshtastic_leaf::MeshRadioSX1262 meshRadio(&radio);
libmeshtastic_leaf::libmeshtastic_leaf mesh;

// Timing for periodic transmission
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 30000; // Send every 30 seconds

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000); // Wait for serial

    Serial.println("libmeshtastic_leaf Basic Send/Receive Example");
    Serial.println("=========================================");

    // Initialize SPI
    SPI.begin();

    // Configure the radio using region helpers
    libmeshtastic_leaf::RadioConfig radioConfig;
    radioConfig.type = libmeshtastic_leaf::RadioType::SX1262;
    radioConfig.region = libmeshtastic_leaf::REGION_US;  // Set your region
    radioConfig.preset = libmeshtastic_leaf::PRESET_LONG_FAST;

    // Get frequency and power from region (or override manually)
    radioConfig.frequency = libmeshtastic_leaf::MeshRegion::getDefaultFrequency(radioConfig.region);
    radioConfig.txPower = libmeshtastic_leaf::MeshRegion::getPowerLimit(radioConfig.region);

    // Pin configuration
    radioConfig.csPin = LORA_CS;
    radioConfig.irqPin = LORA_IRQ;
    radioConfig.rstPin = LORA_RST;
    radioConfig.busyPin = LORA_BUSY;
    radioConfig.tcxoVoltage = 1.8f;    // Adjust for your module (0 if no TCXO)

    // Initialize the radio driver
    if (!meshRadio.begin(radioConfig)) {
        Serial.println("ERROR: Failed to initialize radio!");
        while (1) { delay(1000); }
    }

    // Configure the library
    libmeshtastic_leaf::MeshConfig meshConfig;
    meshConfig.radio = radioConfig;
    // Get node number from hardware MAC address (same method as main firmware)
    meshConfig.nodeNum = libmeshtastic_leaf::MeshNodeId::getNodeNum();
    meshConfig.hopLimit = 3;

    // Initialize libmeshtastic_leaf
    if (!mesh.begin(meshConfig, &meshRadio)) {
        Serial.println("ERROR: Failed to initialize libmeshtastic_leaf!");
        while (1) { delay(1000); }
    }

    // Use the default public channel
    mesh.setDefaultChannel();

    Serial.println("Initialization complete!");
    Serial.print("Node number: 0x");
    Serial.println(mesh.getNodeNum(), HEX);

    // Display short name (like "!1a2b")
    char shortName[5];
    libmeshtastic_leaf::MeshNodeId::getShortName(mesh.getNodeNum(), shortName);
    Serial.print("Short name: !");
    Serial.println(shortName);

    Serial.print("Channel hash: 0x");
    Serial.println(mesh.getChannelHash(), HEX);
    Serial.print("Region: ");
    Serial.print(libmeshtastic_leaf::MeshRegion::getRegionName(radioConfig.region));
    Serial.print(" (");
    Serial.print(radioConfig.frequency);
    Serial.println(" MHz)");
    Serial.print("Preset: ");
    Serial.println(libmeshtastic_leaf::MeshRegion::getPresetName(radioConfig.preset));
    Serial.println();
}

void loop() {
    // Process any incoming packets
    mesh.update();

    // Check for received packets
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

            // If it's a text message, display it
            if (packet.portNum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
                Serial.print("Message: ");
                // Ensure null-termination
                packet.payload[packet.payloadLen] = '\0';
                Serial.println((char*)packet.payload);
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

    // Periodically send a message
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
