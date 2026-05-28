#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// --- NodeMCU LoRa Pins ---
#define SS 15    // D8
#define RST 16   // D0
#define DIO0 5   // D1

// --- Wi-Fi & MQTT Cloud Settings ---
const char* ssid = "Nash";         
const char* password = "12345678"; 
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "naresh/mine-safety/telemetry"; 
const char* mqtt_alert_topic = "naresh/mine-safety/alerts"; // Dedicated alert topic

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected!");
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT Cloud connection...");
        String clientId = "ESP8266Receiver-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            Serial.println("CONNECTED TO CLOUD!");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" trying again in 5 seconds...");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n------------------------------------");
    Serial.println("   COAL MINE SAFETY STATION ACTIVE  ");
    Serial.println("------------------------------------");

    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);

    LoRa.setPins(SS, RST, DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa Initialization: [FAILED]");
        while (1); 
    }

    LoRa.setSyncWord(0xF3);
    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    
    Serial.println("LoRa Receiver: [READY]");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        String receivedData = "";
        while (LoRa.available()) {
            receivedData += (char)LoRa.read();
        }

        if (receivedData.indexOf("BAT") >= 0) {
            Serial.println("\n[--- WORKER UPDATE RECEIVED ---]");
            
            // 1. Format Telemetry for Serial
            String displayData = receivedData;
            displayData.replace(",", " | ");
            Serial.print("TELEMETRY: ");
            Serial.println(displayData);

            // 2. Extract specific Alert Message
            int msgIndex = receivedData.indexOf("MSG:");
            String emergency = "NONE";
            if (msgIndex >= 0) {
                emergency = receivedData.substring(msgIndex + 4);
            }

            // 3. Visual Warning on Serial Monitor
            if (emergency != "NONE") {
                Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                Serial.print("CRITICAL ALERT: "); Serial.println(emergency);
                Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            }

            // 4. Push Data and Alerts to Cloud
            String cloudPayload = "DATA: " + displayData + " | RSSI:" + String(LoRa.packetRssi());
            client.publish(mqtt_topic, cloudPayload.c_str());
            
            if (emergency != "NONE") {
                client.publish(mqtt_alert_topic, emergency.c_str());
            }

            Serial.print("RSSI: "); Serial.print(LoRa.packetRssi()); Serial.println(" dBm");
            Serial.println("-------------------------------");
        }
    }
}