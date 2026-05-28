# Smart Coal Mine Worker Safety System - Real-time Monitoring Dashboard

A mission-critical React dashboard for monitoring coal mine worker safety using LoRa wireless communication. The system tracks vital signs, environmental conditions, and safety equipment compliance in real-time.

## System Architecture

### Overview

```
Coal Mine Workers (Transmitters)
    ↓ LoRa RF433MHz
Mining Gateway (Receiver/Receiver PC)
    ↓ HTTP/WebSocket
React Dashboard
```

## Hardware Components

### Worker Node (Transmitter - ESP32)

Each miner carries an ESP32 microcontroller with integrated sensors:

- **Heart Rate Monitor**: MAX30105 pulse oximeter via I2C
- **Environmental Sensors**: DHT11 (temperature, humidity)
- **Gas Detection**: MQ-9 analog sensor (coal mine hazardous gases)
- **Safety Verification**: BLE receiver for helmet detection
- **Equipment Monitoring**: Digital I/O for safety belt locks
- **Battery Monitoring**: ADC voltage divider circuit
- **LoRa Radio**: SX1278 module at 433 MHz (license-free ISM band)

### Mining Operations Center (Receiver - NodeMCU)

A stationary gateway that receives telemetry from worker nodes:

- **LoRa Receiver**: SX1278 module at 433 MHz matching transmitter frequency
- **Serial Connection**: USB to PC for data aggregation
- **Data Validation**: Filters packets by sync word (0xF3)

---

## Transmitter Code Explanation

**File**: `lib/transmitter-code.ino`

### Pin Configuration

```cpp
#define SS 5         // SPI Chip Select (LoRa)
#define RST 14       // LoRa Reset
#define DIO0 26      // LoRa Interrupt
#define DHTPIN 27    // Temperature/Humidity Sensor
#define MQ2_PIN 34   // Gas Sensor Analog
#define BELT_PIN 25  // Safety Belt Lock
#define BUZZER_PIN 33// Alert Buzzer
#define BATTERY_PIN 35// Battery Voltage
```

### Core Functionality

#### 1. **Sensor Initialization**
- **LoRa**: 433 MHz frequency, spreading factor 12 (maximum range), sync word 0xF3
- **Heart Rate**: MAX30105 configured for continuous IR monitoring
- **Environmental**: DHT11 for temperature readings
- **Bluetooth**: BLE scanner for helmet beacon detection (MAC: b0:d2:78:48:39:65)

#### 2. **Real-Time Data Collection**

**Heart Rate Measurement** (continuously):
```cpp
long irValue = particleSensor.getIR();
if (checkForBeat(irValue)) {
    // Calculate BPM and update rolling average
    beatAvg = average of last 4 readings
}
```

**Telemetry Transmission** (every 3 seconds):
```
BAT:<0-100>,HR:<bpm>,T:<celsius>,G:<ppm>,B:<L|U>,H:<Y|N>
```

Where:
- `BAT`: Battery percentage (0-100%)
- `HR`: Heart rate in BPM (calculated from IR reflectance)
- `T`: Temperature in Celsius
- `G`: Gas level (raw MQ-9 ADC value, ~700+ is hazardous)
- `B`: Belt status - L=Locked, U=Unlocked
- `H`: Helmet status - Y=Detected (BLE), N=Not Detected

#### 3. **Safety Logic**

Unsafe condition triggered if ANY of these occur:
- Helmet NOT detected (H:N)
- Safety belt unlocked (B:U)
- Gas level > 700 ppm
- Temperature > 45°C

When unsafe condition detected, onboard buzzer activates immediately.

#### 4. **LoRa Configuration**

**Critical Settings (MUST match receiver)**:
```cpp
LoRa.setSyncWord(0xF3);              // Unique network ID
LoRa.setSpreadingFactor(12);         // SF7-12, 12 = max range
LoRa.setSignalBandwidth(125E3);      // 125 kHz bandwidth
```

**Spreading Factor Details**:
- SF12: ~80 dB link budget (very long range, slower)
- SF7: ~140 dB link budget (shorter range, faster)
- For coal mines: SF12 chosen for reliability and wall penetration

---

## Receiver Code Explanation

**File**: `lib/receiver-code.ino`

### Pin Configuration

```cpp
#define SS 15    // D8 - NodeMCU SPI
#define RST 16   // D0 - Reset
#define DIO0 5   // D1 - Interrupt
```

### Core Functionality

#### 1. **Initialization**

Receiver waits for packets on 433 MHz with matching sync word:
```cpp
LoRa.begin(433E6);
LoRa.setSyncWord(0xF3);        // MUST match transmitter
LoRa.setSpreadingFactor(12);
LoRa.setSignalBandwidth(125E3);
```

#### 2. **Packet Reception**

```cpp
int packetSize = LoRa.parsePacket();
if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {
        receivedData += (char)LoRa.read();
    }
}
```

#### 3. **Data Validation & Parsing**

Receiver validates data contains expected telemetry fields:
- Checks for "BAT" or "HR" tags
- Parses comma-separated values
- Extracts RSSI (Received Signal Strength Indicator)

#### 4. **Alert Detection**

Real-time alert generation:
- **Belt Violation**: If packet contains "B:U"
- **Helmet Violation**: If packet contains "H:N"
- **High Gas**: Parses G value, alerts if > 700
- **High Temperature**: Parses T value, alerts if > 45

**Sample Parsed Output**:
```
TELEMETRY: BAT:85 | HR:72 | T:38.5 | G:250 | B:L | H:Y
Signal Strength (RSSI): -65 dBm
```

---

## Dashboard Features

### Light Theme Color Palette

- **Primary Purple**: #7553e1 (buttons, highlights)
- **Secondary Purple**: #8d65f7 (accents)
- **Background**: #f3f3f3 (very light gray)
- **Text**: #272d3f (dark navy)

### Real-Time Components

#### 1. **Worker Cards** (Center Grid)
- **Live Telemetry Display**: Battery %, Heart Rate, Temperature, Gas Level
- **Status Indicators**: Belt (Locked/Unlocked), Helmet (Detected/Not Detected)
- **Signal Strength**: RSSI in dBm with color coding
- **Critical Alerts**: Full-card red pulse if ANY violation detected

#### 2. **Environmental Summary** (Left Sidebar)
- **Average Mine Temperature** across all workers
- **Maximum Gas Level** detected
- **Active Worker Count** and connectivity status
- **System Uptime** in hours/minutes

#### 3. **System Event Log** (Right Sidebar)
- **Real-Time Events**: Color-coded by severity (Critical/Warning/Info)
- **Worker Context**: Links events to specific workers
- **Auto-Scroll**: Always shows newest events
- **Event Types**:
  - Gas/Temperature Alerts
  - Safety Violations (Belt/Helmet)
  - Battery Warnings
  - System Status

#### 4. **Mission Control Header**
- **Gateway Status**: Online/Offline indicator
- **Connected Nodes**: Number of active workers
- **System Clock**: Real-time display

---

## Data Flow

### Transmission Cycle (Every 3 Seconds)

1. **Worker Node** reads all sensors
2. Calculates safety conditions
3. Formats LoRa packet: `BAT:85,HR:72,T:38.5,G:250,B:L,H:Y`
4. Sends via LoRa RF at 433 MHz
5. Activates buzzer if unsafe

### Reception Cycle

1. **Mining Gateway** receives LoRa packet
2. Validates sync word (0xF3)
3. Parses telemetry fields
4. Transmits to dashboard via serial/HTTP
5. **Dashboard** updates in real-time
6. Generates alerts for violations
7. Logs events for historical tracking

---

## Safety Thresholds

| Parameter | Safe Range | Warning | Critical |
|-----------|-----------|---------|----------|
| Battery | 50-100% | 20-49% | <20% |
| Heart Rate | 60-110 BPM | 50-59, 111-120 | <50, >120 |
| Temperature | <40°C | 40-45°C | >45°C |
| Gas Level | <500 ppm | 500-700 ppm | >700 ppm |
| Belt Status | Locked | - | Unlocked |
| Helmet Status | Detected | - | Not Detected |

---

## LoRa Specifications

### Why LoRa for Coal Mines?

1. **Long Range**: SF12 penetrates deep underground (vs WiFi)
2. **Low Power**: Battery lasts days (vs cellular)
3. **License-Free**: 433 MHz ISM band (no approval needed)
4. **Interference Resistant**: Unique sync words filter noise
5. **Reliable**: Built-in error correction (Hamming codes)

### Signal Quality Interpretation

| RSSI (dBm) | Signal Quality |
|-----------|-----------------|
| > -50 | Excellent |
| -50 to -70 | Good |
| -70 to -85 | Fair |
| < -85 | Weak |

---

## Integration with Dashboard

### Mock Data Simulation

For development without hardware:
- `generateMockWorkers()`: Creates 16 simulated workers
- `generateSystemEvents()`: Simulates violations/alerts
- `updateWorkerTelemetry()`: Continuously updates values (3-sec interval)

### Real Hardware Integration

Replace mock data with actual LoRa receiver serial data:
```typescript
// Connect to serial port from mining gateway
const serialPort = new SerialPort('/dev/ttyUSB0', { baudRate: 115200 });
serialPort.on('data', (data) => {
  const telemetry = parseLoRaPacket(data);
  updateDashboard(telemetry);
});
```

---

## File Structure

```
/vercel/share/v0-project/
├── lib/
│   ├── transmitter-code.ino      # ESP32 worker node firmware
│   ├── receiver-code.ino          # NodeMCU gateway firmware
│   ├── types.ts                   # TypeScript interfaces
│   └── mock-data.ts               # Development data generator
├── components/dashboard/
│   ├── header.tsx                 # Mission control header
│   ├── worker-card.tsx            # Individual worker telemetry card
│   ├── worker-grid.tsx            # Grid layout of worker cards
│   ├── environmental-summary.tsx   # Left sidebar metrics
│   └── system-event-log.tsx        # Right sidebar event log
├── app/
│   ├── page.tsx                   # Main dashboard page
│   ├── layout.tsx                 # App layout & metadata
│   └── globals.css                # Light theme color palette
└── README.md                       # This file
```

---

## Getting Started

### Hardware Setup

1. **Flash transmitter code** to each ESP32 worker node
2. **Flash receiver code** to NodeMCU mining gateway
3. **Configure serial connection** from gateway PC
4. **Verify LoRa communication**: Both devices should show "READY" on startup

### Dashboard Deployment

```bash
# Install dependencies
npm install

# Run development server
npm run dev

# Build for production
npm run build
npm start
```

Visit `http://localhost:3000` to access the dashboard.

---

## Safety Notices

⚠️ **Critical Systems Notice**:
- This dashboard is designed for underground mining environments
- All safety thresholds are configurable based on coal type and ventilation
- Hardware should include redundant communication paths
- Always test system before deploying in active mines
- Maintain regular sensor calibration schedules

---

**Last Updated**: March 2026  
**Version**: 1.0  
**Status**: Production-Ready
