# ESP32 Modbus Client (ESP-NOW Wireless Bridge)

**ไทย**: [ดูเวอร์ชันภาษาไทย](#อุปกรณ์-esp32-modbus-client)

## English Version

### Overview
This is an ESP32-based wireless Modbus client device that bridges RS-485 serial communication with ESP-NOW wireless protocol. It receives data from a Modbus device via RS-485 serial interface and wirelessly transmits it to a server node.

### Hardware Components

#### Microcontroller
- **Board**: ESP32 DOIT DevKit v1
- **Platform**: Arduino-compatible
- **Build System**: PlatformIO

#### Connectivity & Communication
- **Wireless Protocol**: ESP-NOW (2.4GHz, low latency)
- **Wired Interface**: RS-485 (Serial1 on pins 25/26)
- **Debug Interface**: USB Serial (Serial on pins 1/3)

#### LED Indicators
| LED | Pin | Purpose | Status |
|-----|-----|---------|--------|
| Power | 19 | Power Status | Always ON when powered |
| WiFi | 18 | ESP-NOW Transmission | ON = Success, OFF = Failed |
| RS-485 | 17 | Data Reception | Blinks when data received |

### Pin Configuration

```cpp
// Serial Interfaces
RXD1 = GPIO25  // RS-485 Receive (Readout)
TXD1 = GPIO26  // RS-485 Transmit (Readout)

// LED Indicators
LED_power = GPIO19
LED_wifi  = GPIO18
LED_RS    = GPIO17
```

### Default Settings
- **Baud Rate**: 9600 bps (both Serial and Serial1)
- **Serial Format**: 8N1 (8 bits, No parity, 1 stop bit)
- **Target Server MAC**: `0xc4:0xd8:0xd5:0x95:0xa7:0xd8`
- **ESP-NOW Channel**: 0
- **Encryption**: Disabled

### Installation & Setup

#### Prerequisites
- PlatformIO CLI or VS Code with PlatformIO extension
- ESP32 DOIT DevKit v1 board
- USB cable for programming

#### Steps

1. **Install Dependencies**
   ```bash
   platformio run --target=install_framework_packages
   ```

2. **Build the Firmware**
   ```bash
   platformio run -e esp32doit-devkit-v1
   ```

3. **Upload to Device**
   ```bash
   platformio run -e esp32doit-devkit-v1 --target=upload
   ```

4. **Monitor Serial Output**
   ```bash
   platformio device monitor --baud 9600
   ```

### Operation

#### Data Flow
```
Modbus Device (RS-485) 
        ↓ (Serial1, Pin 25/26)
   ESP32 Client
        ↓ (ESP-NOW wireless)
   Server Node
        ↓
   Computer/Network
```

#### LED Indicators
- **Power LED (pin 19)**: Continuously ON (indicates device is powered)
- **RS-485 LED (pin 17)**: Blinks when data is received from Modbus device
- **WiFi LED (pin 18)**: Turns ON when data is successfully sent via ESP-NOW

#### Message Format
Data is prefixed with `MB2L:` before transmission:
```
Original: <modbus_data>
Sent as:  MB2L:<modbus_data>\r\n
```

### Debugging

#### Via USB Serial
You can send test messages through the USB serial connection for debugging:
1. Open serial monitor at 9600 baud
2. Type a message and press Enter
3. The device will transmit it to the server node with `MB2L:` prefix

#### Serial Monitor Output Examples
```
OK - OnDataSent()        // Successful transmission to server
FAIL - OnDataSent()      // Failed transmission
Task1 running on core 0  // Core 0 starts handling data relay
Task2 running on core 1  // Core 1 available for future tasks
```

### Configuration

To change the target server node, modify `main.cpp`:
```cpp
uint8_t broadcastAddress_server[] = {0xc4,0xd8,0xd5,0x95,0xa7,0xd8};
```

Replace with your server's MAC address.

### Dual-Core Processing
- **Core 0 (Task1)**: Handles data relay from RS-485 to ESP-NOW
- **Core 1 (Task2)**: Reserved for future tasks

### Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| WiFi LED stays OFF | No connection to server | Verify server MAC address and ESP-NOW configuration |
| RS LED doesn't blink | No data from device | Check RS-485 wiring and Modbus device |
| No serial output | Device not powered or wrong port | Check USB connection and COM port selection |
| Garbled data | Baud rate mismatch | Verify 9600 baud setting on Modbus device |

### Power Requirements
- **Input Voltage**: 5V USB or external power supply
- **Current**: ~80-150mA (depends on WiFi activity)

### Serial Configuration File
See `platformio.ini` for build configuration details.

---

## เวอร์ชันภาษาไทย

### ภาพรวม
นี่คือตัวรับสัญญาณ Modbus บนพื้นฐาน ESP32 ที่ใช้งานไร้สายผ่าน ESP-NOW เพื่อเชื่อมต่อการสื่อสารแบบ RS-485 กับโปรโตคอลไร้สายแบบ ESP-NOW โดยรับข้อมูลจากอุปกรณ์ Modbus ผ่านอินเทอร์เฟซ RS-485 และส่งข้อมูลไปยังเซิร์ฟเวอร์โหนดแบบไร้สาย

### อุปกรณ์ฮาร์ดแวร์

#### ไมโครคอนโทรลเลอร์
- **บอร์ด**: ESP32 DOIT DevKit v1
- **แพลตฟอร์ม**: Arduino-compatible
- **ระบบการสร้าง**: PlatformIO

#### การเชื่อมต่อและการสื่อสาร
- **โปรโตคอลไร้สาย**: ESP-NOW (2.4GHz)
- **อินเทอร์เฟซเชื่อมสาย**: RS-485 (Serial1 บน pins 25/26)
- **อินเทอร์เฟซดีบัก**: USB Serial (Serial บน pins 1/3)

#### ตัวบ่งชี้ LED
| LED | Pin | วัตถุประสงค์ | สถานะ |
|-----|-----|-----------|--------|
| Power | 19 | สถานะไฟ | ติดอยู่เมื่อมีการจ่ายไฟ |
| WiFi | 18 | การส่ง ESP-NOW | ติด = สำเร็จ, ดับ = ล้มเหลว |
| RS-485 | 17 | การรับข้อมูล | กระพริบเมื่อรับข้อมูล |

### การกำหนดค่า Pin

```cpp
// อินเทอร์เฟซ Serial
RXD1 = GPIO25  // RS-485 Receive (อ่านข้อมูล)
TXD1 = GPIO26  // RS-485 Transmit (ส่งข้อมูล)

// ตัวบ่งชี้ LED
LED_power = GPIO19
LED_wifi  = GPIO18
LED_RS    = GPIO17
```

### การตั้งค่าเริ่มต้น
- **อัตราบอด**: 9600 bps (ทั้ง Serial และ Serial1)
- **รูปแบบ Serial**: 8N1 (8 บิต, ไม่มี parity, 1 stop bit)
- **MAC เซิร์ฟเวอร์เป้าหมาย**: `0xc4:0xd8:0xd5:0x95:0xa7:0xd8`
- **ช่อง ESP-NOW**: 0
- **การเข้ารหัส**: ปิดใช้งาน

### การติดตั้งและตั้งค่า

#### ข้อกำหนดเบื้องต้น
- PlatformIO CLI หรือ VS Code พร้อมส่วนขยาย PlatformIO
- บอร์ด ESP32 DOIT DevKit v1
- สายเคเบิล USB สำหรับการเขียนโปรแกรม

#### ขั้นตอน

1. **ติดตั้งไลบรารี่ที่จำเป็น**
   ```bash
   platformio run --target=install_framework_packages
   ```

2. **สร้าง Firmware**
   ```bash
   platformio run -e esp32doit-devkit-v1
   ```

3. **อัปโหลดไปยังอุปกรณ์**
   ```bash
   platformio run -e esp32doit-devkit-v1 --target=upload
   ```

4. **ตรวจสอบผลลัพธ์ Serial**
   ```bash
   platformio device monitor --baud 9600
   ```

### การทำงาน

#### การไหลของข้อมูล
```
อุปกรณ์ Modbus (RS-485)
        ↓ (Serial1, Pin 25/26)
   ESP32 Client
        ↓ (ESP-NOW wireless)
   Server Node
        ↓
   Computer/Network
```

#### ตัวบ่งชี้ LED
- **LED Power (pin 19)**: ติดอยู่เรื่อย ๆ (บ่งชี้ว่าอุปกรณ์มีไฟ)
- **LED RS-485 (pin 17)**: กระพริบเมื่อรับข้อมูลจากอุปกรณ์ Modbus
- **LED WiFi (pin 18)**: เปิดเมื่อส่งข้อมูลไปยังเซิร์ฟเวอร์โหนดผ่าน ESP-NOW สำเร็จ

#### รูปแบบข้อความ
ข้อมูลจะมีคำนำหน้า `MB2L:` ก่อนการส่ง:
```
ต้นฉบับ: <modbus_data>
ส่งเป็น: MB2L:<modbus_data>\r\n
```

### การดีบัก

#### ผ่าน USB Serial
คุณสามารถส่งข้อความทดสอบผ่านการเชื่อมต่อ USB serial สำหรับการดีบัก:
1. เปิด serial monitor ที่ 9600 baud
2. พิมพ์ข้อความและกด Enter
3. อุปกรณ์จะส่งข้อมูลไปยังเซิร์ฟเวอร์โหนดพร้อมคำนำหน้า `MB2L:`

#### ตัวอย่างผลลัพธ์ Serial Monitor
```
OK - OnDataSent()        // การส่งสำเร็จไปยังเซิร์ฟเวอร์
FAIL - OnDataSent()      // การส่งล้มเหลว
Task1 running on core 0  // Core 0 เริ่มจัดการการส่งต่อข้อมูล
Task2 running on core 1  // Core 1 พร้อมสำหรับงานในอนาคต
```

### การกำหนดค่า

เพื่อเปลี่ยนเซิร์ฟเวอร์โหนดเป้าหมาย ให้แก้ไข `main.cpp`:
```cpp
uint8_t broadcastAddress_server[] = {0xc4,0xd8,0xd5,0x95,0xa7,0xd8};
```

แทนที่ด้วยที่อยู่ MAC ของเซิร์ฟเวอร์ของคุณ

### การประมวลผลแบบ Dual-Core
- **Core 0 (Task1)**: จัดการการส่งต่อข้อมูลจาก RS-485 ไปยัง ESP-NOW
- **Core 1 (Task2)**: สงวนไว้สำหรับงานในอนาคต

### แก้ไขปัญหา

| ปัญหา | สาเหตุ | วิธีแก้ไข |
|-------|--------|---------|
| LED WiFi ดับอยู่เรื่อย | ไม่มีการเชื่อมต่อกับเซิร์ฟเวอร์ | ตรวจสอบที่อยู่ MAC ของเซิร์ฟเวอร์และการกำหนดค่า ESP-NOW |
| LED RS ไม่กระพริบ | ไม่มีข้อมูลจากอุปกรณ์ | ตรวจสอบการต่อสายและอุปกรณ์ Modbus |
| ไม่มีผลลัพธ์ serial | อุปกรณ์ไม่มีไฟหรือพอร์ตผิด | ตรวจสอบการเชื่อมต่อ USB และเลือกพอร์ต COM ที่ถูกต้อง |
| ข้อมูลไม่ชัดเจน | ความไม่ตรงกันของอัตราบอด | ตรวจสอบการตั้งค่า 9600 baud บนอุปกรณ์ Modbus |

### ข้อกำหนดพลังงาน
- **แรงดันไฟฟ้าเข้า**: 5V USB หรือแหล่งจ่ายไฟภายนอก
- **กระแส**: ~80-150mA (ขึ้นอยู่กับกิจกรรม WiFi)

### ไฟล์การกำหนดค่า Serial
ดู `platformio.ini` เพื่อดูรายละเอียดการกำหนดค่าการสร้าง

---

**Last Updated**: May 2026  
**Project**: ESP32 Modbus Client (ESP-NOW Wireless Bridge)
