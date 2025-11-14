#include <Arduino.h>
#include <WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include "SinricProTemperaturesensor.h"

#define WIFI_SSID     "RESTRITO"
#define WIFI_PASS     "123456789"

#define APP_KEY       "6827f19c-1982-4b28-9f26-613d65d62e8b"
#define APP_SECRET    "67306f70-724f-4644-a0d0-afc2845fc9d6-d6b66907-0934-4558-9a69-b01344ee8d74"

// IDs do Sinric Pro
#define SWITCH_ID_1   "69170fd56ebb39d664b7e22c"      // LED
#define TEMP_ID       "69171c5000f870dd77b92e70"        

// Pinos
#define LED_PIN       18
#define LM35_PIN      34   // Entrada analógica

unsigned long lastSend = 0;

// Callback para o LED
bool onPowerState1(const String &deviceId, bool &state) {
  digitalWrite(LED_PIN, state ? HIGH : LOW);
  Serial.printf("LED está agora: %s\n", state ? "Ligado" : "Desligado");
  return true;
}

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao WiFi ");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println(" Conectado!");
}

float lerTemperatura() {
  int leitura = analogRead(LM35_PIN);
  float volts = leitura * (3.3 / 4095.0);
  float temperatura = volts * 100.0;  // LM35 = 10mV por grau
  return temperatura;
}

void setupSinricPro() {
  pinMode(LED_PIN, OUTPUT);

  SinricProSwitch& mySwitch = SinricPro[SWITCH_ID_1];
  mySwitch.onPowerState(onPowerState1);

  SinricPro.begin(APP_KEY, APP_SECRET);
  Serial.println("Sinric Pro iniciado.");
}

// AQUI está o setup() que estava faltando
void setup() {
  Serial.begin(115200);
  setupWiFi();
  setupSinricPro();
}

// AQUI está o loop() que o erro estava reclamando
void loop() {
  SinricPro.handle();

  unsigned long now = millis();

  if (now - lastSend > 30000) {
    float temperatura = lerTemperatura();

    Serial.printf("Temperatura atual: %.2f °C\n", temperatura);

    SinricProTemperaturesensor &tempSensor = SinricPro[TEMP_ID];
    tempSensor.sendTemperatureEvent(temperatura);

    lastSend = now;
  }
}
