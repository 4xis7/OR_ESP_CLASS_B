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

void Task1code(void * pvParameters);

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
    pinMode(2,         OUTPUT);
    pinMode(4,         OUTPUT);
    pinMode(LED_power, OUTPUT);
    pinMode(LED_wifi,  OUTPUT);
    pinMode(LED_RS,    OUTPUT);
    pinMode(BUTTON_PIN, INPUT); 

    // เกณฑ์ข้อที่ 1: ไฟแสดงสถานะสีแดงต้องติดค้างทันที
    digitalWrite(LED_power, HIGH);
    digitalWrite(LED_wifi,  LOW);
    digitalWrite(LED_RS,    LOW);

    Serial.begin(9600);
    Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);

    if (!SPIFFS.begin(true))
        Serial.println("SPIFFS ERROR");

    // ตรวจสอบข้อมูลจาก Flash Memory
    prefs.begin("config", true);
    String macStr = prefs.getString("peer", "");
    prefs.end();

    if (macStr == "")
    {
        Serial.println("NO MASTER MAC");
        startConfigMode();
    }

    // หากผ่านเงื่อนไขด้านบนมาได้ บอร์ดจะรันในโหมดส่งข้อมูลปกติ
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); 
    Serial.println("NORMAL GATEWAY MODE ACTIVE");
    Serial.print("MY MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP NOW ERROR");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    loadPeer();

    // สตาร์ทงาน FreeRTOS แยกคอร์ประมวลผล
    xTaskCreatePinnedToCore(
        Task1code, "Task1",
        10000, NULL, 1, &Task1, 0); 
    delay(500);
}

// =====================================================
// TASK1 - Serial Relay -> ESP-NOW (และดักจับปุ่มกดเรียลไทม์)
// =====================================================
void Task1code(void * pvParameters)
{ 
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());
    Serial.println("Task1 start");
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

        // รับข้อมูล RS232
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
            data_send.length()
        );

        Serial.print("[RS232] ");
        Serial.println(data_send);
        Serial.println(esp_err_to_name(result));
    }
}

// ================= debug keyboard =================
if (Serial.available() > 0)
{
    String msgWire = Serial.readStringUntil('\n');
    msgWire.trim();

    String data_send = "MB2L:" + msgWire + "\n";

    esp_err_t result = esp_now_send(
        peerMac,
        (uint8_t*)data_send.c_str(),
        data_send.length()
    );

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
