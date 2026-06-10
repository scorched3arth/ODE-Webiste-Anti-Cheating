#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BUZZER_PIN 2

const char* ssid = "Agner Fam";
const char* password = "agner123";

// Clear global tracking states
bool tiltActive = false;
bool soundActive = false;

WebServer server(80);

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

bool alarmActive = false;
unsigned long alarmStartTime = 0;
const unsigned long alarmDuration = 2000; // Increased to 2 seconds to match camera cooldown stability

void showStandbyScreen()
{
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 12);
  display.println("ACTIVE");

  display.setTextSize(1);
  display.setCursor(10, 45);
  display.println("Waiting: ESP32-CAM");

  display.display();
}

void showConnectingScreen()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Connecting Wi-Fi...");
  
  display.setCursor(15, 40);
  display.println(ssid);
  
  display.display();
}

void handleAlert()
{
  // FIX: Map incoming arguments directly to the global variables
  if(server.hasArg("tilt"))
    tiltActive = (server.arg("tilt") == "1");
  else
    tiltActive = false;

  if(server.hasArg("sound"))
    soundActive = (server.arg("sound") == "1");
  else
    soundActive = false;

  Serial.println("========== ALERT RECEIVED ==========");
  Serial.print("Tilt State: "); Serial.println(tiltActive);
  Serial.print("Sound State: "); Serial.println(soundActive);
  
  server.send(200, "text/plain", "OK");

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // FIX: Shifted header up to row 2 to open up real estate below it
  display.setTextSize(2);
  display.setCursor(16, 2); 
  display.println("WARNING!");

  // FIX: Uniform pixel offsets (12px line gaps) to guarantee zero layout overlapping
  display.setTextSize(1);
  if(tiltActive && soundActive)
  {
    display.setCursor(24, 28);
    display.println("Tilt Detected");
    display.setCursor(24, 44);
    display.println("Sound Detected");
  }
  else if(tiltActive)
  {
    display.setCursor(24, 36);
    display.println("Tilt Detected");
  }
  else if(soundActive)
  {
    display.setCursor(24, 36);
    display.println("Sound Detected");
  }

  display.display();

  digitalWrite(BUZZER_PIN, HIGH);

  alarmActive = true;
  alarmStartTime = millis();
}

void setup()
{
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED failed!");
    while(true);
  }

  showConnectingScreen();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  showStandbyScreen();

  server.on("/alert", handleAlert);
  server.begin();
  Serial.println("Web server started");
}

void loop()
{
  server.handleClient();

  if(alarmActive)
  {
    if(millis() - alarmStartTime >= alarmDuration)
    {
      digitalWrite(BUZZER_PIN, LOW);
      alarmActive = false;
      
      // FIX: Reset global triggers so next alert starts completely fresh
      tiltActive = false;
      soundActive = false;
      
      showStandbyScreen(); 
    }
  }
}