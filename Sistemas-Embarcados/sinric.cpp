#include <Arduino.h>
#include <WiFi.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <SinricProThermostat.h>

// ==== OLED ====
#include <Wire.h>
#include "SSD1306Wire.h"

#define OLED_ADDR 0x3C
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_GEOMETRY GEOMETRY_128_64
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL, OLED_GEOMETRY);

// ==== Relógio NTP ====
#include <time.h>

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -3 * 3600;  
const int   daylightOffset_sec = 0;

// ==== Controle da tela ====
bool mostrarMensagemLampada = false;
String mensagemLampada = "";
unsigned long tempoMensagem = 0;

// =============================
// WIFI
// =============================
#define WIFI_SSID "RESTRITO"
#define WIFI_PASS "123456789"

// =============================
// SINRIC PRO
// =============================
#define APP_KEY    "6827f19c-1982-4b28-9f26-613d65d62e8b"
#define APP_SECRET "67306f70-724f-4644-a0d0-afc2845fc9d6-d6b66907-0934-4558-9a69-b01344ee8d74"

#define LAMPADA_ID     "69170fd56ebb39d664b7e22c"
#define TERMOSTATO_ID  "6918fb33729a4887d7cf1a0e"

// =============================
#define LAMPADA_PIN 18
#define LM35_PIN    34

bool lampadaEstado = false;

// =============================
// VARIÁVEIS DE TEMPERATURA
// =============================
float temperaturaAtual = 0.0;
unsigned long ultimaLeituraTemp = 0;

// =============================
// FUNÇÃO: LER LM35
// =============================
float lerTemperatura() {
  int leitura = analogRead(LM35_PIN);
  float tensao = leitura * 3.3 / 4095.0;
  return tensao * 100.0;  // LM35 -> 10mV por °C
}

// =============================
// DATA/HORA
// =============================
String getHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--:--";

  char buffer[10];
  sprintf(buffer, "%02d:%02d:%02d",
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  return String(buffer);
}

String getData() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00/00/0000";

  char buffer[11];
  sprintf(buffer, "%02d/%02d/%04d",
          timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  return String(buffer);
}

// =============================
// OLED – TELA PRINCIPAL
// =============================
void desenhaTelaPrincipal(float temp) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "Temperatura");

  char tempStr[10];
  sprintf(tempStr, "%.1f C", temp);
  display.drawString(64, 20, tempStr);

  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 45, getHora());
  display.drawString(64, 55, getData());

  display.display();
}

// =============================
// OLED – MENSAGEM LÂMPADA
// =============================
void desenhaMensagemLampada() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 25, mensagemLampada);
  display.display();
}

// =============================
// CALLBACK LÂMPADA
// =============================
bool onLampadaPower(const String &deviceId, bool &state) {
  lampadaEstado = state;
  digitalWrite(LAMPADA_PIN, state ? HIGH : LOW);

  mensagemLampada = state ? "Lâmpada ON" : "Lâmpada OFF";
  mostrarMensagemLampada = true;
  tempoMensagem = millis();

  Serial.println(mensagemLampada);
  return true;
}

// =============================
// CALLBACKS TERMOSTATO
// =============================
bool onTargetTemperature(const String &deviceId, float &targetTemp) {
  return true;
}

bool onThermostatMode(const String &deviceId, String &mode) {
  return true;
}

// =============================
// WIFI + NTP
// =============================
void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando horário NTP...");
  delay(1000);
}

// =============================
// SINRIC
// =============================
void setupSinricPro() {
  SinricProSwitch &lampada = SinricPro[LAMPADA_ID];
  lampada.onPowerState(onLampadaPower);

  SinricProThermostat &termo = SinricPro[TERMOSTATO_ID];
  termo.onTargetTemperature(onTargetTemperature);
  termo.onThermostatMode(onThermostatMode);

  SinricPro.begin(APP_KEY, APP_SECRET);
}

// =============================
// SETUP
// =============================
void setup() {
  Serial.begin(115200);

  pinMode(LAMPADA_PIN, OUTPUT);
  digitalWrite(LAMPADA_PIN, LOW);

  setupWiFi();
  setupSinricPro();

  display.init();
  display.flipScreenVertically();
}

// =============================
// LOOP
// =============================
void loop() {
  SinricPro.handle();

  unsigned long agora = millis();

  // ===== Atualiza temperatura a cada 2s =====
  if (agora - ultimaLeituraTemp >= 2000) {
    temperaturaAtual = lerTemperatura();
    ultimaLeituraTemp = agora;

    SinricProThermostat &termo = SinricPro[TERMOSTATO_ID];
    termo.sendTemperatureEvent(temperaturaAtual);  // <-- Google Home usa isto

    Serial.print("Temp enviada: ");
    Serial.println(temperaturaAtual);
  }

  // ===== Mostrar mensagem da lâmpada =====
  if (mostrarMensagemLampada) {
    desenhaMensagemLampada();

    if (millis() - tempoMensagem >= 2000)
      mostrarMensagemLampada = false;

    return;
  }

  // ===== Tela principal usando a mesma temperatura enviada ao Sinric =====
  desenhaTelaPrincipal(temperaturaAtual);
}