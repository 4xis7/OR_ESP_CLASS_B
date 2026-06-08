#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include <SPIFFS.h>

// =======================================================
// CLIENT B NODE (ปรับปรุงตามเงื่อนไขข้อกำหนดระบบ)
// =======================================================

#define RXD1 25
#define TXD1 26
#define BUTTON_PIN 35  // ปุ่มกดสำหรับเข้าโหมด Web Server

// ================== LED ==================
const int LED_wifi  = 18; // ไฟสีเหลือง (ESP-NOW)
const int LED_RS    = 17; // ไฟสีเขียว (RS232)
const int LED_power = 19; // ไฟสีแดง (Power)

// =====================================================
// AP CONFIG
// =====================================================
const char* AP_SSID     = "ESP32_CONFIG_B";
const char* AP_PASSWORD = "12345678";

// =====================================================
// GLOBAL
// =====================================================
Preferences prefs;
WebServer   server(80);

uint8_t peerMac[6];
bool    peerReady = false;
bool    configMode = false; // ตัวแปรเช็คว่าอยู่ในโหมดตั้งค่าหน้าเว็บหรือไม่

esp_now_peer_info_t peerInfo;

TaskHandle_t Task1;
TaskHandle_t Task2;

void Task1code(void * pvParameters);
void Task2code(void * pvParameters);

// =====================================================
// ตรวจสอบ MAC Format
// =====================================================
bool isValidMac(String mac) {
    if (mac.length() != 17) return false;
    for (int i = 0; i < 17; i++) {
        if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
            if (mac[i] != ':') return false;
        } else {
            if (!isxdigit(mac[i])) return false;
        }
    }
    return true;
}

// =====================================================
// MAC STRING -> BYTE
// =====================================================
bool macStringToBytes(String macStr, uint8_t *mac) {
    if (macStr.length() != 17) return false;
    int values[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)values[i];
    return true;
}

// =====================================================
// WEB HANDLERS
// =====================================================
void handleRoot() {
    if (!SPIFFS.exists("/index.html")) {
        server.send(404, "text/plain", "index.html not found");
        return;
    }
    File f = SPIFFS.open("/index.html", "r");
    String html = "";
    while (f.available()) html += (char)f.read();
    f.close();

    prefs.begin("config", true);
    String savedPeer = prefs.getString("peer", "NOT SET");
    prefs.end();

    html.replace("%MY_MAC%",   WiFi.macAddress());
    html.replace("%PEER_MAC%", savedPeer);

    server.send(200, "text/html", html);
}

void handleSave() {
    if (!server.hasArg("mac")) {
        server.send(400, "text/plain", "No MAC");
        return;
    }
    String mac = server.arg("mac");
    mac.trim();
    mac.toUpperCase();

    if (!isValidMac(mac)) {
        server.send(200, "text/plain", "ERROR: Invalid MAC Format");
        return;
    }

    prefs.begin("config", false);
    prefs.putString("peer", mac);
    prefs.end();

    Serial.println("MAC SAVED: " + mac);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

void handleRestart() {
    server.send(200, "text/plain", "RESTARTING");
    delay(1000);
    ESP.restart();
}

// =====================================================
// ESP-NOW CALLBACKS
// =====================================================
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("OK - OnDataSent()");
        digitalWrite(LED_wifi, HIGH); // ส่งสำเร็จ ไฟสีเหลืองติดค้าง
    } else {
        Serial.println("FAIL - OnDataSent()");
        digitalWrite(LED_wifi, LOW);  // ส่งไม่สำเร็จ ไฟสีเหลืองดับ
    }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    String dataIn = "";
    for (int i = 0; i < len; i++) dataIn += (char)incomingData[i];

    Serial.print(dataIn);
    Serial1.print(dataIn);
    Serial.flush();
}

// =====================================================
// ฟังก์ชันเริ่มทำงานโหมด WEB SERVER CONFIG
// =====================================================
void startConfigMode() {
    configMode = true;
    Serial.println(">>> START CONFIG MODE (WEB SERVER Active) <<<");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/save",    HTTP_POST, handleSave);
    server.on("/restart", HTTP_POST, handleRestart);
    server.begin();

    // วนลูปค้างไว้ที่นี่เพื่อรองรับหน้าเว็บอย่างเดียว จะไม่ไปรันการทำงานส่วนอื่น
    while(configMode) {
        server.handleClient();
        delay(1);
    }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
    pinMode(LED_power, OUTPUT);
    pinMode(LED_wifi,  OUTPUT);
    pinMode(LED_RS,    OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // ตั้งค่าปุ่มกด GPIO 35 (Active LOW)

    digitalWrite(LED_power, HIGH); // เงื่อนไขที่ 1: เปิดเครื่องไฟสีแดงติดค้าง
    digitalWrite(LED_wifi,  LOW);
    digitalWrite(LED_RS,    LOW);

    Serial.begin(9600);
    Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);

    if (!SPIFFS.begin(true)) Serial.println("SPIFFS ERROR");

    // ตรวจสอบความต้องการข้อที่ 2: เช็คว่ามีข้อมูล MAC ของบอร์ด Master หรือยัง?
    prefs.begin("config", true);
    String macStr = prefs.getString("peer", "");
    prefs.end();

    // เงื่อนไขเพิ่มเติม: หรือถ้าเปิดเครื่องขึ้นมาแล้วผู้ใช้งานกดปุ่ม GPIO 35 ทันที
    if (macStr == "" || digitalRead(BUTTON_PIN) == LOW) {
        if (macStr == "") Serial.println("No Peer MAC found in memory!");
        else Serial.println("Button GPIO 35 Pressed during boot!");
        
        startConfigMode(); // บังคับเข้าหน้าเว็บและหยุดการทำงานส่วนอื่นทันที
    }

    // หากมีข้อมูล MAC ครบถ้วน บอร์ดจะเริ่มเข้าสู่โหมดการทำงานปกติ (ESP-NOW + FreeRTOS)
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP NOW ERROR");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // โหลดบอร์ดผู้รับและผูกคู่การสื่อสาร
    if (macStringToBytes(macStr, peerMac)) {
        memcpy(peerInfo.peer_addr, peerMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) == ESP_OK) {
            peerReady = true;
            Serial.println("ESP-NOW Peer Ready: " + macStr);
        }
    }

    // สร้าง Multi-task สำหรับรันการสื่อสารตามปกติ
    xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 0, &Task1, 0);
    delay(500);
    xTaskCreatePinnedToCore(Task2code, "Task2", 10000, NULL, 0, &Task2, 1);
    delay(500);
}

// =====================================================
// TASK1 - สื่อสารหลัก & เช็คการกดปุ่มเปลี่ยนใจระหว่างรันงาน
// =====================================================
void Task1code(void * pvParameters) {
    for (;;) {
        // เงื่อนไขข้อที่ 3: ระหว่างรันงานปกติ หากผู้ใช้กดปุ่ม GPIO 35 ให้สลับเข้า Web Server ทันที
        if (digitalRead(BUTTON_PIN) == LOW) {
            delay(50); // Debounce ป้องกันปุ่มเบิ้ล
            if (digitalRead(BUTTON_PIN) == LOW) {
                startConfigMode(); 
            }
        }

        // รับค่าจาก RS232 (Serial1) และส่งผ่าน ESP-NOW
        if (Serial1.available() > 0) {
            // เงื่อนไขข้อที่ 5: มีข้อมูลเข้ามาทาง RS232 ไฟสีเขียวต้องกะพริบอย่างรวดเร็ว
            digitalWrite(LED_RS, HIGH);
            String msgWire = Serial1.readStringUntil('\r');
            digitalWrite(LED_RS, LOW);

            String data_send = "MB2L:" + msgWire + "\r\n";
            if (peerReady) {
                esp_now_send(peerMac, (uint8_t*)data_send.c_str(), data_send.length());
            }
            delay(50);
        }

        // รับค่าจาก Serial หลัก (สำหรับทดสอบผ่านคอมพิวเตอร์)
        if (Serial.available() > 0) {
            String msgWire = Serial.readStringUntil('\r');
            String data_send = "MB2L:" + msgWire + "\r\n";
            if (peerReady) {
                esp_now_send(peerMac, (uint8_t*)data_send.c_str(), data_send.length());
            }
            delay(50);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1)); // คืนเวลาให้ระบบตามระเบียบ FreeRTOS
    }
}

void Task2code(void * pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void loop() {
    // ปล่อยว่างไว้ตามรูปแบบสไตล์ FreeRTOS
    delay(1000);
}
