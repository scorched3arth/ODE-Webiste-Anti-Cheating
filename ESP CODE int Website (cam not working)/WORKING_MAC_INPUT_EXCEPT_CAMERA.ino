#include <esp_now.h>
#include <WiFi.h>
#include "esp_camera.h"

// --- 1. NEW SAFE SENSOR PIN DEFINITIONS ---
#define TILT_PIN 14  // Safe Input
#define SOUND_PIN 15 // Safe Input
#define BUZZER_PIN 12

// --- 2. Camera Hardware Pin Mapping (AI-Thinker Board) ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// !!! DOUBLE CHECK THIS MAC ADDRESS MATCHES RECEIVER !!!
uint8_t receiverAddress[] = {0xA0, 0xDD, 0x6C, 0x68, 0xF2, 0x40}; 

// FIXED: Matching the receiver struct exactly
typedef struct struct_message {
    bool motionTriggered;
    bool soundTriggered;
    bool isTriggered; 
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

void OnDataSent(uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW Packet Delivery: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS! Receiver got it." : "FAILED! Receiver didn't hear it.");
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  
  config.xclk_freq_hz = 20000000;       
  config.frame_size = FRAMESIZE_QVGA;   
  config.pixel_format = PIXFORMAT_JPEG; 
  config.jpeg_quality = 12;             
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    config.ledc_channel = LEDC_CHANNEL_1;
    config.ledc_timer = LEDC_TIMER_1;
    err = esp_camera_init(&config);
  }
  return (err == ESP_OK);
}

void setup() {
  Serial.begin(115200);
  
  // Stabilize inputs
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(SOUND_PIN, INPUT_PULLUP);
  
  // FIXED: Added missing pinMode for sender's local buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!initCamera()) {
    Serial.println("Critical Error: Camera hardware failed to start. Halting.");
    while(true); 
  }
  Serial.println("Camera successfully initialized.");

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
  
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 1;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("System initialized. Move sensors to test live readings...");
}

void loop() {
  int rawTilt = digitalRead(TILT_PIN);
  int rawSound = digitalRead(SOUND_PIN);

  if (rawSound == HIGH) {
    Serial.println("🔴 TRIGGERED: Loud Sound Detected!");
  } else {
    Serial.println("🟢 Quiet...");
  }
  delay(100); 

  Serial.printf("Live States -> Tilt Pin(14): %d | Sound Pin(15): %d\n", rawTilt, rawSound);

  bool tiltDetected = (rawTilt == LOW); 
  bool soundDetected = (rawSound == HIGH);

  if (tiltDetected || soundDetected) {
    Serial.println("\n*** SENSOR TRIP CONDITION MET! *");
    digitalWrite(BUZZER_PIN, HIGH);
    
    // Fill the struct
    myData.isTriggered = true;
    myData.motionTriggered = tiltDetected;
    myData.soundTriggered = soundDetected;
    
    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Camera hardware capture failed!");
    } else {
      Serial.printf("Snapshot captured! Size: %d bytes\n", fb->len);
      esp_camera_fb_return(fb);
    } 
    
    Serial.println("Sending ESP-NOW Packet...");
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
    
    if (result != ESP_OK) {
      Serial.println("Internal ESP-NOW framework transmission error.");
    }

    delay(80); // Combined your two separate delays cleanly
    
  } else {
    // Turn off local buzzer and reset transmission values
    digitalWrite(BUZZER_PIN, LOW);
    myData.isTriggered = false;
    myData.motionTriggered = false; 
    myData.soundTriggered = false;  
    
    esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData)); 
  }
}