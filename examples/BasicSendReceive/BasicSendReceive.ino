// Transmit and receive on the default Meshtastic channel.
// SX1262 on ESP32; adjust the pins below for your board.

#include <SPI.h>
#include <libmeshtastic_leaf.h>

#define LORA_CS   18
#define LORA_IRQ  26
#define LORA_RST  14
#define LORA_BUSY 33

SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY);
libmeshtastic_leaf::libmeshtastic_leaf mesh;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 30000; // Send every 30 seconds

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000); // Wait for serial

    Serial.println("libmeshtastic_leaf Basic Send/Receive Example");
    Serial.println("=========================================");

    SPI.begin();

    libmeshtastic_leaf::RadioConfig radioConfig;
    radioConfig.region = libmeshtastic_leaf::REGION_US;  // Set your region
    radioConfig.preset = libmeshtastic_leaf::PRESET_LONG_FAST;

    radioConfig.frequency = libmeshtastic_leaf::MeshRegion::getDefaultFrequency(radioConfig.region);
    radioConfig.txPower = libmeshtastic_leaf::MeshRegion::getPowerLimit(radioConfig.region);

    // Chip specific bring-up belongs to the sketch; the library only speaks
    // the generic RadioLib PhysicalLayer API.
    int st = radio.begin();
    if (st != RADIOLIB_ERR_NONE) {
        Serial.print("ERROR: radio.begin() failed: ");
        Serial.println(st);
        while (1) { delay(1000); }
    }
    radio.setTCXO(1.8f);              // adjust for your module, omit if none
    radio.setCRC(2);                  // Meshtastic requires LoRa CRC
    radio.setCurrentLimit(140.0f);

    libmeshtastic_leaf::MeshConfig meshConfig;
    meshConfig.radio = radioConfig;
    meshConfig.nodeNum = libmeshtastic_leaf::MeshNodeId::getNodeNum();
    meshConfig.hopLimit = 3;

    if (!mesh.begin(meshConfig, &radio)) {
        Serial.println("ERROR: Failed to initialize libmeshtastic_leaf!");
        while (1) { delay(1000); }
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
    Serial.print(libmeshtastic_leaf::MeshRegion::getRegionName(radioConfig.region));
    Serial.print(" (");
    Serial.print(radioConfig.frequency);
    Serial.println(" MHz)");
    Serial.print("Preset: ");
    Serial.println(libmeshtastic_leaf::MeshRegion::getPresetName(radioConfig.preset));
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
