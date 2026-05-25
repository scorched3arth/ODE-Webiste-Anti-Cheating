#define SIMULATION_MODE true

// =====================================================
// PIN DEFINITIONS
// =====================================================

const int SOUND_PIN = 12;
const int TILT_PIN = 13;
//const int CAM_TRIGGER_PIN = 25;

const int OLED_SCL_PIN = 21;
const int OLED_SDA_PIN = 22;
const int BUZZER_PIN = 23;


// =====================================================
// SENSOR SETTINGS
// =====================================================

const int SOUND_ACTIVE_STATE = HIGH;
const int TILT_ACTIVE_STATE = HIGH;

const unsigned long EVENT_COOLDOWN_MS = 3000;


// =====================================================
// VARIABLES
// =====================================================

unsigned long lastEventTime = 0;
unsigned long simulationTimer = 0;
int simulationStep = 0;


// =====================================================
// EVENT DATA FORMAT
// =====================================================

struct SensorEvent {
  bool soundDetected;
  bool tiltDetected;
  bool cameraTriggered;
  int soundRaw;
  String eventStatus;
  unsigned long timestampMs;
};


// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(SOUND_PIN, INPUT);
  pinMode(TILT_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Serial.println();
  Serial.println("================================");
  Serial.println("cheatCHER Sensor Input System");
  Serial.println("================================");

  if (SIMULATION_MODE) {
    Serial.println("Mode: SIMULATION");
  } else {
    Serial.println("Mode: HARDWARE");
  }

  Serial.println("System ready.");
  Serial.println();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  SensorEvent currentEvent;

  if (SIMULATION_MODE) {
    currentEvent = readSimulatedSensors();
  } else {
    currentEvent = readHardwareSensors();
  }

  printLiveStatus(currentEvent);

  if (shouldTriggerEvent(currentEvent)) {
    currentEvent.cameraTriggered = true;
    currentEvent.eventStatus = "FLAGGED";

    activateBuzzer();
    printDetectedEvent(currentEvent);
  }

  delay(500);
}


// =====================================================
// SIMULATION SENSOR READING
// =====================================================

SensorEvent readSimulatedSensors() {
  SensorEvent eventData;

  if (millis() - simulationTimer >= 5000) {
    simulationTimer = millis();
    simulationStep++;

    if (simulationStep > 3) {
      simulationStep = 0;
    }
  }

  eventData.soundDetected = false;
  eventData.tiltDetected = false;
  eventData.cameraTriggered = false;
  eventData.soundRaw = 0;
  eventData.eventStatus = "NORMAL";
  eventData.timestampMs = millis();

  if (simulationStep == 1) {
    eventData.soundDetected = true;
    eventData.soundRaw = 1;
    eventData.eventStatus = "SOUND_DETECTED";
  }

  else if (simulationStep == 2) {
    eventData.tiltDetected = true;
    eventData.soundRaw = 0;
    eventData.eventStatus = "TILT_DETECTED";
  }

  else if (simulationStep == 3) {
    eventData.soundDetected = true;
    eventData.tiltDetected = true;
    eventData.soundRaw = 1;
    eventData.eventStatus = "TILT_AND_SOUND_DETECTED";
  }

  return eventData;
}


// =====================================================
// HARDWARE SENSOR READING
// =====================================================

SensorEvent readHardwareSensors() {
  SensorEvent eventData;

  int soundValue = digitalRead(SOUND_PIN);
  int tiltValue = digitalRead(TILT_PIN);

  eventData.soundDetected = (soundValue == SOUND_ACTIVE_STATE);
  eventData.tiltDetected = (tiltValue == TILT_ACTIVE_STATE);
  eventData.cameraTriggered = false;
  eventData.soundRaw = soundValue;
  eventData.timestampMs = millis();

  if (eventData.soundDetected && eventData.tiltDetected) {
    eventData.eventStatus = "TILT_AND_SOUND_DETECTED";
  }

  else if (eventData.soundDetected) {
    eventData.eventStatus = "SOUND_DETECTED";
  }

  else if (eventData.tiltDetected) {
    eventData.eventStatus = "TILT_DETECTED";
  }

  else {
    eventData.eventStatus = "NORMAL";
  }

  return eventData;
}


// =====================================================
// EVENT DECISION LOGIC
// =====================================================

bool shouldTriggerEvent(SensorEvent eventData) {
  bool detected = eventData.soundDetected || eventData.tiltDetected;
  bool cooldownDone = millis() - lastEventTime >= EVENT_COOLDOWN_MS;

  if (detected && cooldownDone) {
    lastEventTime = millis();
    return true;
  }

  return false;
}


// =====================================================
// BUZZER ALERT
// =====================================================

void activateBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}


// =====================================================
// LIVE SERIAL MONITOR DISPLAY
// =====================================================

void printLiveStatus(SensorEvent eventData) {
  Serial.print("Mode: ");
  Serial.print(SIMULATION_MODE ? "SIMULATION" : "HARDWARE");

  Serial.print(" | Sound: ");
  Serial.print(eventData.soundDetected ? "DETECTED" : "Normal");

  Serial.print(" | Tilt: ");
  Serial.print(eventData.tiltDetected ? "DETECTED" : "Normal");

  Serial.print(" | Sound Raw: ");
  Serial.print(eventData.soundRaw);

  Serial.print(" | Status: ");
  Serial.println(eventData.eventStatus);
}


// =====================================================
// DETECTED EVENT DISPLAY
// =====================================================

void printDetectedEvent(SensorEvent eventData) {
  Serial.println();
  Serial.println("========== EVENT DETECTED ==========");

  Serial.print("Time: ");
  Serial.println(eventData.timestampMs);

  Serial.print("Sound Detected: ");
  Serial.println(eventData.soundDetected ? "YES" : "NO");

  Serial.print("Tilt Detected: ");
  Serial.println(eventData.tiltDetected ? "YES" : "NO");

  Serial.print("Sound Raw: ");
  Serial.println(eventData.soundRaw);

  Serial.print("Event Status: ");
  Serial.println(eventData.eventStatus);

  Serial.print("Camera Triggered: ");
  Serial.println(eventData.cameraTriggered ? "YES" : "NO");

  Serial.println("====================================");
  Serial.println();
}