#include <Arduino.h>
#include "WiFi.h"
#include <esp_now.h>

//#define RXD2 16 //iTSD 
//#define TXD2 17 //iTSD
#define RXD1 25 //Readout
#define TXD1 26 //Readout

//0C:B8:15:C3:3C:64
//0C:B8:15:C3:3C:64

uint8_t broadcastAddress_server[] = {0xc4,0xd8,0xd5,0x95,0xa7,0xd8}; // Comp node

TaskHandle_t Task1;
TaskHandle_t Task2;
//----------------------------------
void Task1code(void * pvParameters);
void Task2code(void * pvParameters);
esp_now_peer_info_t peerInfo;


// ================== ✅ LED ==================
const int LED_wifi = 18;
const int LED_RS   = 17;
const int LED_power = 19;


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
 
      if (status ==0){
        Serial.println("OK - OnDataSent()");
        digitalWrite(LED_wifi, HIGH);   // 🔥 ติดค้าง
      }
      else{
        Serial.println("FAIL - OnDataSent()");
        digitalWrite(LED_wifi, LOW);    // 🔥 ดับ
      }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  
  String dataIn;
  
  for (int i=0;i<len;i++) {
    dataIn += (char)incomingData[i];
  }

  
  Serial.print(dataIn);
  Serial1.print(dataIn);
  Serial.flush();
  //delay(500);

  
}


void setup(){
  pinMode(2 , OUTPUT);
  pinMode(4 , OUTPUT);
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);
  //Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(LED_power, OUTPUT);
  digitalWrite(LED_power, HIGH);
  // ✅ เพิ่ม LED
  pinMode(LED_wifi, OUTPUT);
  pinMode(LED_RS, OUTPUT);

  digitalWrite(LED_wifi, LOW);
  digitalWrite(LED_RS, LOW);

  WiFi.mode(WIFI_STA);
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Register peer
  
  memcpy(peerInfo.peer_addr, broadcastAddress_server, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    0,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    0,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
 
    delay(500);
}

//Task2
void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
      //wail data from device (mobile device) then send data to server node (connected with computer)
      if(Serial1.available() > 0){

        digitalWrite(LED_RS, HIGH);   // 🔥 ติด

        String  msgWire = Serial1.readStringUntil('\r');

        digitalWrite(LED_RS, LOW);    // 🔥 ดับ
            //String  msgWire = Serial1.readString();
            //This device needs to be limited the length of strings, so reducing character before send to server node 
          // msg.trim();
          // msg.replace(" ", "");
          // msg.replace("\r", "");

          // int denIndex = msg.indexOf("Density");
          // String data_send = msg.substring(denIndex);
          // esp_err_t result = esp_now_send( broadcastAddress_server, (uint8_t*)data_send.c_str(), data_send.length());
          // Serial.print(data_send);
           
          String data_send = "MB2L:" + msgWire + "\r\n";
          esp_err_t result = esp_now_send( broadcastAddress_server, (uint8_t*)data_send.c_str(), data_send.length());
            delay(100);
      }
      if(Serial.available() > 0){ //for debugging by keyboard
          String msgWire = Serial.readStringUntil('\r');
          String data_send = "MB2L:" + msgWire + "\r\n";
          esp_err_t result = esp_now_send( broadcastAddress_server, (uint8_t*)data_send.c_str(), data_send.length());
          delay(100);
      }
      
  } 
  delay(10);
}

//Task2
void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
     
    }

  delay(10);
}
 
void loop(){
   delay(100);    //Do nothing
}
