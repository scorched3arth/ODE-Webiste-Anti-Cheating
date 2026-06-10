#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ================= CAMERA PINS =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       36
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= SENSOR PINS =================
#define TILT_PIN 14
#define SOUND_PIN 15

// ================= WIFI =================
const char* ssid = "replace with wifi name";
const char* password = "repalce with wifi pass";

const String standardEspIP = "http://192.168.100.197";  //IP address of receiver

// ================= VARIABLES =================
WebServer server(80);

camera_fb_t *global_fb = NULL;

bool newPhotoAvailable = false;

String lastEventTime = "None";
String lastEventSensor = "None";

bool lastTiltState = false;
bool lastSoundState = false;
bool cameraOperational = false; // Added to fix declaration scope error

unsigned long lastTriggerTime = 0;
const unsigned long triggerCooldown = 2000;

// =======================================================
// PHOTO ENDPOINT
// =======================================================
void handleGetLatestPhoto()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");

    if (!global_fb)
    {
        server.send(404, "text/plain", "No photo available");
        return;
    }

    server.send_P(
        200,
        "image/jpeg",
        (const char *)global_fb->buf,
        global_fb->len
    );
}

// =======================================================
// STATUS ENDPOINT
// =======================================================
void handleStatus()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");

    String json = "{";
    json += "\"newPhoto\":" + String(newPhotoAvailable ? "true" : "false") + ",";
    json += "\"sensor\":\"" + lastEventSensor + "\",";
    json += "\"tiltActive\":" + String(lastTiltState ? "true" : "false") + ",";
    json += "\"soundActive\":" + String(lastSoundState ? "true" : "false") + ",";
    json += "\"time\":\"" + lastEventTime + "\"";
    json += "}";

    server.send(200, "application/json", json);

    newPhotoAvailable = false;
}

// =======================================================
// SETUP
// =======================================================
void setup()
{
    Serial.begin(115200);

    pinMode(TILT_PIN, INPUT_PULLUP);
    pinMode(SOUND_PIN, INPUT_PULLUP);

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
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);

    if (err == ESP_OK)
    {
        Serial.println("Camera initialized successfully");
        cameraOperational = true;
    }
    else
    {
        Serial.printf("Camera Init Failed: 0x%x\n", err);
        cameraOperational = false;
    }

    if (psramFound())
    {
        Serial.println("PSRAM Found");
    }
    else
    {
        Serial.println("PSRAM NOT Found");
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) 
    {
        Serial.printf("Detected PID: 0x%X\n", s->id.PID);
        if (s->id.PID == OV3660_PID) 
        {
            Serial.println("OV3660 detected");
            s->set_vflip(s, 1);
            s->set_brightness(s, 1);
            s->set_saturation(s, -2);
        }
    }
    else
    {
        Serial.println("Camera sensor NOT detected!");
    }

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi Connected");
    Serial.print("ESP32-CAM IP: ");
    Serial.println(WiFi.localIP());

    server.on("/get-photo", handleGetLatestPhoto);
    server.on("/status", handleStatus);

    server.begin();
    Serial.println("Camera Server Started");
    delay(2000);
}   

// =======================================================
// LOOP
// ======================================================= 
void loop() 
{
    server.handleClient();

    int currentTilt = digitalRead(TILT_PIN);
    int currentSound = digitalRead(SOUND_PIN);

    // 1. Trigger if Tilt goes LOW (Active-LOW) OR if Sound goes HIGH (Active-HIGH)
    if ((currentTilt == LOW || currentSound == HIGH) && (millis() - lastTriggerTime > triggerCooldown)) 
    {
        // 2. Micro-sampling window: Wait 50ms for states to fully settle
        delay(50); 
        
        // 3. Read finalized hardware states
        int finalTiltState = digitalRead(TILT_PIN);
        int finalSoundState = digitalRead(SOUND_PIN);
        
        lastTriggerTime = millis();
        
        // 4. Map signals accurately into clean boolean states
        lastTiltState = (finalTiltState == LOW);
        lastSoundState = (finalSoundState == HIGH);

        // 5. Evaluate combined states
        if (lastTiltState && lastSoundState) {
            lastEventSensor = "Both Sensors";
        } else if (lastTiltState) {
            lastEventSensor = "Tilt Sensor";
        } else if (lastSoundState) {
            lastEventSensor = "Sound Sensor";
        } else {
            lastEventSensor = "None";
        }

        // --- GENERATE RUNTIME TIMESTAMP ---
        unsigned long allSeconds = millis() / 1000;
        int runHours = allSeconds / 3600;
        int secsRemaining = allSeconds % 3600;
        int runMinutes = secsRemaining / 60;
        int runSeconds = secsRemaining % 60;

        lastEventTime = (runHours < 10 ? "0" : "") + String(runHours) + ":" +
                        (runMinutes < 10 ? "0" : "") + String(runMinutes) + ":" +
                        (runSeconds < 10 ? "0" : "") + String(runSeconds);

        Serial.println("Violation Flagged: " + lastEventSensor);

        // --- SAFE CAMERA CAPTURE WITH ERROR HANDLING ---
        if (cameraOperational) 
        {
            if (global_fb) 
            {
                esp_camera_fb_return(global_fb);
                global_fb = NULL;
            }
            
            global_fb = esp_camera_fb_get();
            
            if (global_fb) 
            {
                newPhotoAvailable = true; 
                Serial.print("Capture Success. Size = ");
                Serial.println(global_fb->len);
            } 
            else 
            {
                Serial.println("Camera sensor capture failed dynamically.");
                newPhotoAvailable = true; // Still triggers web UI log row
            }
        }
        else 
        {
            // Fallback tracking if camera hardware module fails to initialize on boot
            newPhotoAvailable = true;
        }

        // --- SEND ALERT TO STANDARD ESP32 WITH RETRIES ---
        if (WiFi.status() == WL_CONNECTED) 
        {
            bool alertDelivered = false;
            int retryCount = 0;
            const int maxRetries = 3;

            while (!alertDelivered && retryCount < maxRetries) 
            {
                HTTPClient http;
                String url = standardEspIP + "/alert?tilt=" + String(lastTiltState ? "1" : "0") + "&sound=" + String(lastSoundState ? "1" : "0");
                
                http.begin(url);
                http.setTimeout(1000); 
                
                int responseCode = http.GET();
                
                if (responseCode == 200) 
                {
                    Serial.println("OLED Alert sent successfully!");
                    alertDelivered = true; 
                } 
                else 
                {
                    retryCount++;
                    Serial.print("OLED Alert failed. Retrying... (Attempt ");
                    Serial.print(retryCount);
                    Serial.println(")");
                    delay(100); 
                }
                http.end();
            }
        }
    }
}