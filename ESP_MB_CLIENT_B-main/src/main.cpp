#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include <SPIFFS.h>

// =======================================================
// CLIENT B NODE (ฉบับแก้ไข: ล้างค่าเก่าทันทีเมื่อเข้าโหมดคอนฟิก)
// =======================================================

#define RXD1 25
#define TXD1 26
#define BUTTON_PIN 35  // ปุ่มกดสำหรับเข้าโหมด Web Server (ต่อแบบ Active LOW)

// ================== LED ==================
const int LED_wifi  = 18; // ไฟสีเหลือง (ESP-NOW Status)
const int LED_RS    = 17; // ไฟสีเขียว (RS232 Data Traffic)
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
bool    configMode = false;

int buttonState = LOW;
int lastButtonState = LOW;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

esp_now_peer_info_t peerInfo;

TaskHandle_t Task1;
TaskHandle_t Task2;

void Task1code(void * pvParameters);
void Task2code(void * pvParameters);

// =====================================================
// ตรวจสอบ MAC Format (XX:XX:XX:XX:XX:XX)
// =====================================================
bool isValidMac(String mac)
{
    if (mac.length() != 17)
        return false;

    for (int i = 0; i < 17; i++)
    {
        if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14)
        {
            if (mac[i] != ':')
                return false;
        }
        else
        {
            if (!isxdigit(mac[i]))
                return false;
        }
    }
    return true;
}

// =====================================================
// MAC STRING -> BYTE
// =====================================================
bool macStringToBytes(String macStr, uint8_t *mac)
{
    if (macStr.length() != 17)
        return false;

    int values[6];

    if (sscanf(macStr.c_str(),
               "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1],
               &values[2], &values[3],
               &values[4], &values[5]) != 6)
    {
        return false;
    }

    for (int i = 0; i < 6; ++i)
        mac[i] = (uint8_t)values[i];

    return true;
}

// =====================================================
// LOAD PEER MAC จาก NVS
// =====================================================
void loadPeer()
{
    prefs.begin("config", true);
    String macStr = prefs.getString("peer", "");
    prefs.end();

    if (macStr == "")
    {
        Serial.println("NO PEER");
        return;
    }

    Serial.print("PEER = ");
    Serial.println(macStr);

    if (!macStringToBytes(macStr, peerMac))
    {
        Serial.println("INVALID MAC");
        return;
    }

    memcpy(peerInfo.peer_addr, peerMac, 6);  
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_del_peer(peerMac); 

    esp_err_t result = esp_now_add_peer(&peerInfo);

    Serial.print("ADD PEER RESULT: ");
    Serial.println(esp_err_to_name(result));

    if (result == ESP_OK)
    {
        Serial.println("PEER ADDED");
        peerReady = true;
    }
    else
    {
        Serial.println("ADD PEER FAIL");
        peerReady = false;
    }
}

// =====================================================
// WEB HANDLERS
// =====================================================
void handleRoot()
{
    if (!SPIFFS.exists("/index.html"))
    {
        server.send(404, "text/plain", "index.html not found");
        return;
    }

    File f = SPIFFS.open("/index.html", "r");
    String html = "";
    while (f.available())
        html += (char)f.read();
    f.close();

    prefs.begin("config", true);
    String savedPeer = prefs.getString("peer", "NOT SET");
    prefs.end();

    html.replace("%MY_MAC%",   WiFi.macAddress());
    html.replace("%PEER_MAC%", savedPeer);

    server.send(200, "text/html", html);
}

void handleSave()
{
    if (!server.hasArg("mac"))
    {
        server.send(400, "text/plain", "No MAC");
        return;
    }

    String mac = server.arg("mac");
    mac.trim();
    mac.toUpperCase();

    if (!isValidMac(mac))
    {
        String errorHtml = "<html><meta charset='UTF-8'><body style='font-family:sans-serif; text-align:center; padding-top:50px;'>";
        errorHtml += "<h2 style='color:red;'>ERROR: รูปแบบ MAC Address ไม่ถูกต้อง!</h2>";
        errorHtml += "<p>กรุณากรอกในรูปแบบ XX:XX:XX:XX:XX:XX</p>";
        errorHtml += "<br><a href='/' style='padding:10px 20px; background:#ccc; text-decoration:none; color:black; border-radius:5px;'>กลับไปแก้ไข</a>";
        errorHtml += "</body></html>";
        server.send(200, "text/html", errorHtml);
        return;
    }

    prefs.begin("config", false);
    prefs.putString("peer", mac);      // บันทึกค่าใหม่ลงไปแทนที่
    prefs.putBool("force_web", false); // เคลียร์สถานะบังคับเปิดเว็บหลังบันทึกสำเร็จ
    prefs.end();

    Serial.println("MAC SAVED: " + mac);

    String successHtml = "<html><meta charset='UTF-8'><body style='font-family:sans-serif; text-align:center; padding-top:50px;'>";
    successHtml += "<div style='display:inline-block; border:2px solid #4CAF50; padding:30px; border-radius:10px; background-color:#f9f9f9;'>";
    successHtml += "<h2 style='color:#4CAF50;'>✓ บันทึกข้อมูลเรียบร้อยแล้ว!</h2>";
    successHtml += "<p style='font-size:18px;'>บันทึกค่าใหม่เป็น: <b>" + mac + "</b> สำเร็จ</p>";
    successHtml += "<p style='color:#666;'>กำลังเตรียมรีสตาร์ทบอร์ดเพื่อเข้าโหมดทำงานปกติ...</p>";
    successHtml += "</div>";
    successHtml += "</body></html>";

    server.send(200, "text/html", successHtml);

    delay(3000);   // รอให้ผู้ใช้เห็นข้อความ 3 วินาที
    ESP.restart();
}

// =====================================================
// ฟังก์ชันเริ่มรันระบบ WEB SERVER CONFIG (โหมดตั้งค่า)
// =====================================================
void startConfigMode()
{
    configMode = true;
    Serial.println(">>> START CONFIG MODE (WEB SERVER Active) <<<");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    delay(500);

    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/save",    HTTP_POST, handleSave);
    server.begin();

    while (configMode)
{
    server.handleClient();
    delay(1);
}
}

// =====================================================
// ESP-NOW SEND CALLBACK
// =====================================================
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        Serial.println("OK - OnDataSent()");
        digitalWrite(LED_wifi, HIGH); // ส่งสำเร็จ ไฟสีเหลืองติดค้าง
    }
    else
    {
        Serial.println("FAIL - OnDataSent()");
        digitalWrite(LED_wifi, LOW);  // ส่งล้มเหลว ไฟสีเหลืองดับ
    }
}

// =====================================================
// ESP-NOW RECEIVE CALLBACK
// =====================================================
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    String dataIn = "";
    for (int i = 0; i < len; i++)
        dataIn += (char)incomingData[i];

    Serial.print(dataIn);
    Serial1.print(dataIn);
    Serial.flush();
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
    // กำหนดทิศทางการทำงานของ GPIO
    // LED_power : แสดงสถานะไฟเลี้ยงระบบ
    // LED_wifi  : แสดงสถานะการส่งข้อมูล ESP-NOW
    // LED_RS    : แสดงสถานะการรับส่งข้อมูล RS232
    // BUTTON_PIN: ปุ่มกดสำหรับเข้าสู่โหมดตั้งค่า
    pinMode(LED_power, OUTPUT);
    pinMode(LED_wifi,  OUTPUT);
    pinMode(LED_RS,    OUTPUT);
    pinMode(BUTTON_PIN, INPUT); 

    // กำหนดสถานะเริ่มต้นของ LED
    digitalWrite(LED_power, HIGH);
    digitalWrite(LED_wifi,  LOW);
    digitalWrite(LED_RS,    LOW);

    // เริ่มต้น Serial สำหรับ Debug และ RS232 Communication
    Serial.begin(9600);
    Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);

    // เปิดใช้งาน SPIFFS ใช้เก็บไฟล์หน้าเว็บสำหรับตั้งค่าระบบผ่าน Web Browser
    if (!SPIFFS.begin(true))
        Serial.println("SPIFFS ERROR");

    // อ่าน MAC Address ของ Master ที่บันทึกไว้ใน NVS Flash เพื่อใช้ตรวจสอบว่ามีการตั้งค่าระบบแล้วหรือไม่
    prefs.begin("config", true);
    String macStr = prefs.getString("peer", "");
    prefs.end();

    // เลือกโหมดการทำงานของอุปกรณ์
    // กรณียังไม่มี MAC Address:
    //  - เข้าโหมด Config Mode
    //  - เปิด WiFi AP และ Web Server
    //  - รอผู้ใช้กำหนดค่า Master MAC
    //
    // กรณีมี MAC Address แล้ว:
    //  - เข้าโหมด Normal Mode
    //  - เริ่มระบบ ESP-NOW สำหรับรับส่งข้อมูล
    if (macStr == "")
    {
        Serial.println("NO MASTER MAC");
        startConfigMode();
    }
    else
    {
        Serial.println("NORMAL GATEWAY MODE ACTIVE");

    // ตั้งค่า ESP32 ให้ทำงานในโหมด Station เพื่อใช้งาน ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); 
    Serial.println("NORMAL GATEWAY MODE ACTIVE");
    Serial.print("MY MAC: ");
    Serial.println(WiFi.macAddress());

    // เริ่มต้นระบบ ESP-NOW หากเริ่มต้นไม่สำเร็จจะหยุดการทำงาน
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP NOW ERROR");
        return;
    }

    // ลงทะเบียน Callback Function สำหรับตรวจสอบผลการส่งและรับข้อมูล
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // โหลด MAC Address ของ Peer จาก Flash Memory และเพิ่มเข้า Peer List ของ ESP-NOW
    loadPeer();

    // สร้าง Task1 ทำหน้าที่ตรวจสอบปุ่มกดเพื่อเข้าสู่ Config Mode
    xTaskCreatePinnedToCore(
        Task1code,
        "Task1",
        3000,
        NULL,
        3,
        &Task1,
        1);

    // =================================================
    // สร้าง Task2
    // ทำหน้าที่รับข้อมูลจาก RS232 และส่งต่อผ่าน ESP-NOW
    // =================================================
    xTaskCreatePinnedToCore(
        Task2code,
        "Task2",
        8192,
        NULL,
        1,
        &Task2,
        1);
    }
}

// =====================================================
// Task ปุ่มกด
// =====================================================
void Task1code(void *pvParameters)
{
    Serial.print("Task1 running on Core = ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        int reading = digitalRead(BUTTON_PIN);

        if (reading != lastButtonState)
        {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > debounceDelay)
        {
            if (reading != buttonState)
            {
                buttonState = reading;

                if (buttonState == HIGH)
                {
                    Serial.println("ENTER CONFIG MODE");
                    startConfigMode();
                }
            }
        }

        lastButtonState = reading;

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

// =====================================================
// Task Communication
// =====================================================

void Task2code(void *pvParameters)
{
    Serial.print("Task2 running on core = ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        // ================= RS232 =================
        if (Serial1.available() > 0)
        {
            digitalWrite(LED_RS, HIGH);

            String msgWire = Serial1.readStringUntil('\n');
            msgWire.trim();

            digitalWrite(LED_RS, LOW);

            if (msgWire.length() > 0 && peerReady)
            {
                String data_send = "MB2L:" + msgWire + "\r\n";

                esp_err_t result = esp_now_send(
                    peerMac,
                    (uint8_t*)data_send.c_str(),
                    data_send.length());

                Serial.print("[RS232] ");
                Serial.println(data_send);
                Serial.println(esp_err_to_name(result));
            }
        }

        // ================= DEBUG =================
        if (Serial.available() > 0)
        {
            String msgWire = Serial.readStringUntil('\n');
            msgWire.trim();

            String data_send = "MB2L:" + msgWire + "\n";

            esp_err_t result = esp_now_send(
                peerMac,
                (uint8_t*)data_send.c_str(),
                data_send.length());

            Serial.print("DEBUG SEND RESULT: ");
            Serial.println(esp_err_to_name(result));
        }

        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void loop()
{
 delay(1000);
}
