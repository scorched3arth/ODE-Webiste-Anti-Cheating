#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BUZZER_PIN 12

//-------Configuration (I2C) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// FIXED: Struct updated to exactly match the Transmitter side
typedef struct struct_message {
    bool motionTriggered;
    bool soundTriggered;
    bool isTriggered;
} struct_message;

struct_message incomingData;

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  
  Serial.println("\n--- ALERT RECEIVED FROM CAM ---");

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);

  display.setTextSize(1);
  display.setCursor(0, 32);

  if (incomingData.motionTriggered || incomingData.soundTriggered) {
    digitalWrite(BUZZER_PIN, HIGH); // Sound the alarm physical hardware buzzer
    
    if (incomingData.motionTriggered) {
      display.println("> TILT SENSOR TRIP");
      Serial.println("Source: Tilt Sensor");
    }
    if (incomingData.soundTriggered) {
      display.println("> SOUND SENSOR TRIP");
      Serial.println("Source: Sound Sensor");
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);  
    display.println("System Monitoring...");
  }
  
  display.display(); 
} 

void setup() {
  Serial.begin(115200);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 OLED allocation failed");
    for(;;);
  }
  
  // Splash screen message
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,20);
  display.println("Waiting for ESP-NOW...");
  display.display();

  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); // Force hardware to Channel 1
  esp_wifi_set_promiscuous(false);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv); 
  Serial.println("Receiver ready and listening on Channel 1.");
}

void loop() {
  // Free loop execution for seamless background ESP-NOW callbacks
}