#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include "MAX30105.h"           
#include "heartRate.h"          
#include <DHT.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// --- Pin Definitions ---
#define BATTERY_PIN 35  
#define DHTPIN 27
#define MQ2_PIN 34
#define BELT_PIN 25
#define BUZZER_PIN 33
#define SCK 18
#define MISO 19
#define MOSI 23
#define SS 5
#define RST 14
#define DIO0 26

// --- Global Objects ---
DHT dht(DHTPIN, DHT11);
MAX30105 particleSensor;
BLEScan* pBLEScan;

// --- Heart Rate Variables ---
const byte RATE_SIZE = 4; 
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0; 
int beatAvg = 0;

// --- BLE & Safety Settings ---
bool helmetDetected = false;
String targetBeaconAddress = "b0:d2:78:48:39:65"; 
int rssiThreshold = -84;

void setup() {
    Serial.begin(115200);
    pinMode(BELT_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    analogSetAttenuation(ADC_11db); 
    
    // 1. Initialize LoRa
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa Failed!");
    }
    
    // --- CRITICAL: MATCH THESE TO RECEIVER ---
    LoRa.setSyncWord(0xF3);           
    LoRa.setSpreadingFactor(12);     
    LoRa.setSignalBandwidth(125E3);

    // 2. Initialize Sensors
    dht.begin();
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 Not Found!");
    } else {
        particleSensor.setup(); 
        particleSensor.setPulseAmplitudeRed(0x0A); 
    }

    // 3. Initialize BLE
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    
    Serial.println("TRANSMITTER READY: Emergency Logic Active");
}

void loop() {
    // --- Heart Rate Sensing ---
    long irValue = particleSensor.getIR();
    if (checkForBeat(irValue)) {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        float bpm = 60 / (delta / 1000.0);
        if (bpm < 255 && bpm > 20) {
            rates[rateSpot++] = (byte)bpm;
            rateSpot %= RATE_SIZE;
            beatAvg = 0;
            for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
            beatAvg /= RATE_SIZE;
        }
    }

    static unsigned long lastReport = 0;
    if (millis() - lastReport > 3000) {
        // 1. Read Battery
        int rawADC = analogRead(BATTERY_PIN);
        float volt = (rawADC / 4095.0) * 3.3 * 2.0;
        int bat_per = map(volt * 100, 330, 420, 0, 100);
        bat_per = constrain(bat_per, 0, 100);

        // 2. Read Sensors
        float temp = dht.readTemperature();
        int gasValue = analogRead(MQ2_PIN);
        bool beltLocked = (digitalRead(BELT_PIN) == LOW);

        // 3. BLE Scan for Helmet
        BLEScanResults* foundDevices = pBLEScan->start(1, false); 
        helmetDetected = false;
        for (int i = 0; i < foundDevices->getCount(); i++) {
            if (foundDevices->getDevice(i).getAddress().toString() == targetBeaconAddress) {
                if (foundDevices->getDevice(i).getRSSI() > rssiThreshold) helmetDetected = true;
                break;
            }
        }
        pBLEScan->clearResults();

        // --- 4. EMERGENCY WARNING LOGIC ----
        String alertMsg = "NONE";
        bool isUnsafe = false;

        // Priority 1: Gas Leakage
        if (gasValue > 800) {-
            alertMsg = "GAS LEAKAGE DETECTED";
            isUnsafe = true;
        } 
        // Priority 2: High Temp
        else if (temp > 45) {
            alertMsg = "HIGH TEMP ALERT";
            isUnsafe = true;
        }
        // Priority 3: PPE Violations
        else if (!helmetDetected) {
            alertMsg = "HELMET REMOVED";
            isUnsafe = true;
        }
        else if (!beltLocked) {
            alertMsg = "BELT UNLOCKED";
            isUnsafe = true;
        }

        // Trigger Local Buzzer if unsafe
        digitalWrite(BUZZER_PIN, isUnsafe ? HIGH : LOW);

        // --- 5. LoRa Transmission ---
        String data = "BAT:" + String(bat_per) + 
                      ",HR:" + String(beatAvg) + 
                      ",T:" + String(temp) + 
                      ",G:" + String(gasValue) + 
                      ",B:" + (beltLocked ? "L" : "U") + 
                      ",H:" + (helmetDetected ? "Y" : "N") +
                      ",MSG:" + alertMsg;

        LoRa.beginPacket();
        LoRa.print(data);
        LoRa.endPacket();

        // --- 6. Serial Debug ---
        Serial.println("\n--- WORKER STATUS ---");
        Serial.print("Data: "); Serial.println(data);
        if(isUnsafe) {
            Serial.print("!!! WARNING: "); Serial.println(alertMsg);
        }

        lastReport = millis();
    }
}