#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <ICM42688.h>

// ---------------- PINS ----------------
#define ESC_PIN        4
#define SERVO_PIN      5
#define AD0_GROUND_PIN 6
#define IMU_INT_PIN    7
#define I2C_SCL        8
#define I2C_SDA        9

Servo steeringServo;
Servo motorESC;

#define IMU_ADDR 0x68
ICM42688 imu(Wire, IMU_ADDR);

// ---------------- CONTROL PACKET ----------------
struct __attribute__((packed)) ControlData {
  int16_t steering;
  int16_t throttle;
  uint8_t flags;
  uint8_t driveMode;
};
ControlData incomingData = {0, 0, 0, 0};

volatile int16_t remoteSteeringInput = 0;
volatile int16_t remoteThrottleInput = 0;
volatile unsigned long lastPacketMillis = 0;
volatile bool remoteKillRequested = false;
volatile bool remoteResetRequested = false;
volatile uint8_t currentDriveMode = 0;

enum DriveMode : uint8_t {
  MODE_NORMAL       = 0,
  MODE_DRIFT        = 1,
  MODE_MANUAL_STEER = 2
};
DriveMode lastReportedMode = MODE_NORMAL;

// ---------------- KILL SWITCH ----------------
enum KillReason : uint8_t {
  KILL_NONE = 0,
  KILL_CRASH_IMPACT,
  KILL_SIGNAL_LOST,
  KILL_REMOTE_SWITCH,
  KILL_ROLLOVER,
  KILL_SENSOR_FAULT
};

volatile bool       signalLost  = false;
volatile bool       latchedStop = false;
volatile KillReason killReason  = KILL_NONE;
KillReason lastReportedReason   = KILL_NONE;

#define FAILSAFE_TIMEOUT_MS      300
#define CRASH_G_THRESHOLD        7.0
#define ROLLOVER_Z_THRESHOLD    -0.5
#define SENSOR_FAULT_LIMIT       5
#define SENSOR_CHECK_INTERVAL_MS 100
#define RESET_THROTTLE_DEADBAND  50

const uint16_t NEUTRAL_US = 1500;

uint8_t sensorFaultCount = 0;
unsigned long lastSensorCheckMillis = 0;

inline bool emergencyActive() {
  return signalLost || latchedStop;
}

void forceNeutralOutputs() {
  motorESC.writeMicroseconds(NEUTRAL_US);
  steeringServo.writeMicroseconds(NEUTRAL_US);
}

void triggerLatchedStop(KillReason reason) {
  latchedStop = true;
  killReason = reason;
  forceNeutralOutputs();
}

esp_timer_handle_t safetyTimer;

void safetyTimerCallback(void* arg) {
  unsigned long now = millis();

  if (!emergencyActive() && (now - lastPacketMillis > FAILSAFE_TIMEOUT_MS)) {
    signalLost = true;
    killReason = KILL_SIGNAL_LOST;
  }

  if (emergencyActive()) {
    forceNeutralOutputs();
  }
}

void IRAM_ATTR imuInterruptHandler() {
}

void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len == sizeof(ControlData)) {
    memcpy(&incomingData, data, sizeof(ControlData));
  } else if (len == (int)(sizeof(int16_t) * 2 + sizeof(uint8_t))) {
    memcpy(&incomingData, data, sizeof(int16_t) * 2 + sizeof(uint8_t));
    incomingData.driveMode = 0;
  } else if (len == (int)(sizeof(int16_t) * 2)) {
    memcpy(&incomingData, data, sizeof(int16_t) * 2);
    incomingData.flags = 0;
    incomingData.driveMode = 0;
  } else {
    return;
  }

  remoteSteeringInput = incomingData.steering;
  remoteThrottleInput = incomingData.throttle;
  currentDriveMode = (incomingData.driveMode <= MODE_MANUAL_STEER) ? incomingData.driveMode : MODE_NORMAL;
  lastPacketMillis = millis();
  signalLost = false;

  if (incomingData.flags & 0x01) remoteKillRequested = true;
  if (incomingData.flags & 0x02) remoteResetRequested = true;
}

// ---------------- TUNING ----------------
float Kp = 0.8;
float KpDrift = 0.8;
#define THROTTLE_DEADZONE 40

unsigned long lastLoopTime = 0;
const unsigned long LOOP_INTERVAL = 4;

void setup() {
  Serial.begin(115200);

  pinMode(AD0_GROUND_PIN, OUTPUT);
  digitalWrite(AD0_GROUND_PIN, LOW);
  delay(100);

  pinMode(IMU_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imuInterruptHandler, RISING);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  steeringServo.setPeriodHertz(50);
  steeringServo.attach(SERVO_PIN, 500, 2500);

  motorESC.setPeriodHertz(50);
  motorESC.attach(ESC_PIN, 1000, 2000);
  motorESC.writeMicroseconds(NEUTRAL_US);

  steeringServo.writeMicroseconds(NEUTRAL_US);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  int status = imu.begin();
  int retries = 0;
  while (status < 0 && retries < 5) {
    Serial.println("Waiting for ICM42688... retrying...");
    delay(200);
    status = imu.begin();
    retries++;
  }

  if (status < 0) {
    Serial.println("ICM42688 Error! Check SDA/SCL connections.");
    forceNeutralOutputs();
    while (1) { delay(1000); }
  }

  Serial.println("ICM42688 Connected successfully!");
  imu.setAccelFS(ICM42688::gpm16);
  imu.setGyroFS(ICM42688::dps2000);
  imu.setFilters(true, true);

  Serial.println("Calibrating Gyro... KEEP CAR STILL!");
  delay(1000);
  imu.calibrateGyro();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setChannel(1);
  esp_now_init();
  esp_now_register_recv_cb(onReceive);
  lastPacketMillis = millis();

  const esp_timer_create_args_t safetyTimerArgs = {
    .callback = &safetyTimerCallback,
    .name = "safety_monitor"
  };
  esp_timer_create(&safetyTimerArgs, &safetyTimer);
  esp_timer_start_periodic(safetyTimer, 20000);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms = 1000,
    .idle_core_mask = 0x0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdtConfig);
#else
  esp_task_wdt_init(1, true);
#endif
  esp_task_wdt_add(NULL);

  Serial.println("System Ready!");
}

void loop() {
  esp_task_wdt_reset();
  yield();

  if (remoteKillRequested) {
    remoteKillRequested = false;
    triggerLatchedStop(KILL_REMOTE_SWITCH);
  }

  if (remoteResetRequested) {
    remoteResetRequested = false;
    if (latchedStop && abs(remoteThrottleInput) < RESET_THROTTLE_DEADBAND) {
      latchedStop = false;
      killReason = KILL_NONE;
      sensorFaultCount = 0;
      Serial.println("Kill switch reset by remote command.");
    } else if (latchedStop) {
      Serial.println("Reset request ignored: bring throttle to neutral first.");
    }
  }

  if (currentDriveMode != lastReportedMode) {
    lastReportedMode = (DriveMode)currentDriveMode;
    switch (lastReportedMode) {
      case MODE_NORMAL:       Serial.println("MODE: normal (stability)"); break;
      case MODE_DRIFT:        Serial.println("MODE: drift assist"); break;
      case MODE_MANUAL_STEER: Serial.println("MODE: manual steering (no gyro correction)"); break;
    }
  }

  if (killReason != lastReportedReason) {
    lastReportedReason = killReason;
    switch (killReason) {
      case KILL_CRASH_IMPACT:  Serial.println("KILL SWITCH: crash impact detected."); break;
      case KILL_SIGNAL_LOST:   Serial.println("KILL SWITCH: radio signal lost."); break;
      case KILL_REMOTE_SWITCH: Serial.println("KILL SWITCH: remote switch engaged."); break;
      case KILL_ROLLOVER:      Serial.println("KILL SWITCH: rollover detected."); break;
      case KILL_SENSOR_FAULT:  Serial.println("KILL SWITCH: IMU sensor fault."); break;
      default: break;
    }
  }

  if (emergencyActive()) {
    return;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - lastLoopTime >= LOOP_INTERVAL) {
    lastLoopTime = currentMillis;

    imu.getAGT();
    float accX = imu.accX();
    float accY = imu.accY();
    float accZ = imu.accZ();
    float gyrZ = imu.gyrZ();

    float totalG = sqrt(accX * accX + accY * accY + accZ * accZ);

    if (totalG > CRASH_G_THRESHOLD) {
      triggerLatchedStop(KILL_CRASH_IMPACT);
      Serial.println("CRASH DETECTED! ESC/servo SHUTDOWN.");
      return;
    }

    if (accZ < ROLLOVER_Z_THRESHOLD) {
      triggerLatchedStop(KILL_ROLLOVER);
      Serial.println("ROLLOVER DETECTED! ESC/servo SHUTDOWN.");
      return;
    }

    if (currentMillis - lastSensorCheckMillis >= SENSOR_CHECK_INTERVAL_MS) {
      lastSensorCheckMillis = currentMillis;
      Wire.beginTransmission(IMU_ADDR);
      uint8_t i2cResult = Wire.endTransmission();
      if (i2cResult != 0) {
        sensorFaultCount++;
        if (sensorFaultCount >= SENSOR_FAULT_LIMIT) {
          triggerLatchedStop(KILL_SENSOR_FAULT);
          Serial.println("IMU SENSOR FAULT! ESC/servo SHUTDOWN.");
          return;
        }
      } else {
        sensorFaultCount = 0;
      }
    }

    int throttleInput = (abs(remoteThrottleInput) < THROTTLE_DEADZONE) ? 0 : remoteThrottleInput;
    int finalThrottleInput = throttleInput;

    if (currentDriveMode != MODE_DRIFT && throttleInput > 100) {
      float yawRate = abs(gyrZ);
      float slideFactor = yawRate / 100.0;

      if (slideFactor > 1.5 && accX < 0.2) {
        float powerReduction = constrain(1.0 - (slideFactor * 0.2), 0.3, 1.0);
        finalThrottleInput = throttleInput * powerReduction;
      }
    }

    motorESC.writeMicroseconds(map(finalThrottleInput, -1000, 1000, 1000, 2000));

    int baseSteeringPWM = map(remoteSteeringInput, -1000, 1000, 1150, 1850);
    int finalSteeringPWM;

    switch (currentDriveMode) {
      case MODE_DRIFT: {
        int driftAssist = gyrZ * KpDrift;
        finalSteeringPWM = constrain(baseSteeringPWM - driftAssist, 1150, 1850);
        break;
      }
      case MODE_MANUAL_STEER:
        finalSteeringPWM = constrain(baseSteeringPWM, 1150, 1850);
        break;
      case MODE_NORMAL:
      default: {
        int gyroCorrection = gyrZ * Kp;
        finalSteeringPWM = constrain(baseSteeringPWM + gyroCorrection, 1150, 1850);
        break;
      }
    }
    steeringServo.writeMicroseconds(finalSteeringPWM);
  }
}
