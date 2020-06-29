
#define M5STACK_MPU6886
#include <ArduinoJson.h>
#include <M5Stack.h>
#include <WiFi.h>

#include "yeelight.h"

#define SSID "Nacho wifi"
#define WIFI_PWD "Nacho pass"
#define SLEEP_WAIT 3000
#define ACCEL_THRESHOLD 200
#define MAX_DIMMER_VALUE 50000
#define DIMMER_TO_PERCENT(param) ((100 * (param)) / MAX_DIMMER_VALUE)

void angleDimmer(void* xParam);
void sleepWait(void* xParam);

void waitForWiFi();
void lightPowerToggle();

void lightAdjustBrightness();

void power_on_rutine();

TaskHandle_t sleepTaskHandle;
TaskHandle_t angleTaskHandle;
Yeelight* yeelight;

bool initialized = false;
bool inactive = false;

time_t last_activity = millis();
int32_t dimmer_value = 0;
int current_bright = 1;
int toggle_counter = 1;

String brightness_params;

void setup() {
  M5.begin();

  M5.Power.begin();
  M5.Power.setWakeupButton(BUTTON_B_PIN);
  M5.IMU.Init();

  WiFi.begin(SSID, WIFI_PWD);

  waitForWiFi();

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.setTextSize(2);

  yeelight = new Yeelight();
  yeelight->lookup();
}

void loop() {
  M5.update();

  if (!initialized) {
    power_on_rutine();
    initialized = true;
    inactive = false;
  }

  if (M5.BtnC.wasPressed()) {
    lightPowerToggle();
  }

  Serial.println(current_bright);

  lightAdjustBrightness();

  if (inactive) {
    vTaskDelete(angleTaskHandle);
    vTaskDelete(sleepTaskHandle);
    initialized = false;
    M5.Power.lightSleep();
  }
  delay(100);
}

void lightPowerToggle() {
  last_activity = millis();

  M5.Lcd.setCursor(0, 60);
  M5.Lcd.print("Sending power toggle ");
  M5.Lcd.println(toggle_counter++);
  yeelight->sendCommand("toggle", "[]");
}

void lightAdjustBrightness() {
  int new_bright = (int)(dimmer_value / (MAX_DIMMER_VALUE / 100));
  String brightness_params;

  if (new_bright < 1) {
    new_bright = 1;
  }

  if (new_bright != current_bright) {
    current_bright = new_bright;
    brightness_params = String("[") + new_bright + ", \"smooth\", 500]";

    Serial.println(yeelight->sendCommand("set_bright", brightness_params));
  }
}

void power_on_rutine() {
  M5.Lcd.fillScreen(BLACK);

  last_activity = millis();
  xTaskCreate(angleDimmer, "angleDimmer", 4096, NULL, 10, &angleTaskHandle);
  xTaskCreate(sleepWait, "sleepWait", XT_STACK_MIN_SIZE, NULL, 9,
              &sleepTaskHandle);

  waitForWiFi();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Connected to Wifi");

  while (!yeelight->feedback()) {
    yeelight->lookup();
    delay(50);
  }
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.println("Light detected");
}

void waitForWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(F("."));
  }
}

void angleDimmer(void* xParam) {
  int16_t gx, gy, gz;

  while (1) {
    M5.IMU.getGyroAdc(&gx, &gy, &gz);

    if (gz >= ACCEL_THRESHOLD || gz <= -ACCEL_THRESHOLD) {
      dimmer_value -= gz;  // I want right turns to increment the brightness
      last_activity = millis();
    }

    if (dimmer_value >= MAX_DIMMER_VALUE) {
      dimmer_value = MAX_DIMMER_VALUE;
    } else if (dimmer_value <= 0) {
      dimmer_value = 0;
    }
    vTaskDelay(100 * portTICK_PERIOD_MS);
  }
}

void sleepWait(void* xParam) {
  while (1) {
    if (millis() > last_activity + SLEEP_WAIT) {
      inactive = true;
    }
    M5.Lcd.setCursor(0, 100);
    M5.Lcd.print("Brightness:    %");
    M5.Lcd.setCursor(140, 100);
    M5.Lcd.println(current_bright);
    vTaskDelay(500 * portTICK_PERIOD_MS);
  }
}
