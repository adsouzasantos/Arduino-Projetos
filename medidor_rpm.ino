/**
 * Projeto: Medidor de RPM para Motor a Vapor
 * Descrição: Este código utiliza um sensor infravermelho (IR) conectado ao pino 27 
 * para medir a velocidade de rotação (RPM) do motor através de interrupções.
 */

#define SENSOR_PIN 27         // Pino do sensor IR
#define DEBOUNCE_TIME 10000   // 10ms em microssegundos (evita leituras falsas)
#define TIMEOUT 3000000       // 3 segundos sem pulso = motor parado

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
volatile bool newPulse = false;

// Função de interrupção (armazenada na IRAM para maior velocidade no ESP32)
void IRAM_ATTR sensorInterrupt() {
  unsigned long now = micros();
  
  // Debounce: ignora pulsos muito próximos (ruído elétrico)
  if (now - lastPulseTime < DEBOUNCE_TIME) {
    return;
  }

  if (lastPulseTime > 0) {
    pulseInterval = now - lastPulseTime;
    newPulse = true;
  }
  
  lastPulseTime = now;
}

void setup() {
  Serial.begin(115200);

  // Configura o pino do sensor com resistor de pull-up interno
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  
  // Configura a interrupção para borda de descida (FALLING)
  attachInterrupt(
    digitalPinToInterrupt(SENSOR_PIN),
    sensorInterrupt,
    FALLING
  );
  
  Serial.println("Medidor de RPM iniciado...");
}

void loop() {
  static unsigned long lastDisplayTime = 0;
  unsigned long currentTime = micros();

  if (newPulse) {
    // Seção crítica: desabilita interrupções brevemente para ler a variável volátil
    noInterrupts();
    unsigned long interval = pulseInterval;
    newPulse = false;
    interrupts();

    if (interval > 0) {
      // Cálculo: 60 segundos * 1.000.000 microssegundos / intervalo
      float rpm = 60000000.0 / interval;
      
      Serial.print("RPM: ");
      Serial.print(rpm, 1);  // Exibe com 1 casa decimal
      Serial.print(" | Intervalo: ");
      Serial.print(interval);
      Serial.println(" us");

      lastDisplayTime = currentTime;
    }
  }

  // Detecta motor parado (se o tempo desde o último pulso exceder o TIMEOUT)
  if (currentTime - lastPulseTime > TIMEOUT && lastPulseTime > 0) {
    if (currentTime - lastDisplayTime > 1000000) {  // Atualiza a cada 1 segundo
      Serial.println("RPM: 0 (motor parado)");
      lastDisplayTime = currentTime;
    }
  }

  delay(10);  // Pequeno delay para estabilidade do loop
}
