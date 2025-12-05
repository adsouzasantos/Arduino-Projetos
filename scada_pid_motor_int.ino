// ---------------------- BIBLIOTECA MODBUS ----------------------
#include <ModbusRtu.h>

// ---------------------- PINOS ----------------------
#define ENCA 2
#define ENCB 4
#define PWM 5
#define IN1 6
#define IN2 7

// ---------------------- VARIÁVEIS ----------------------
volatile long pos_i = 0;
volatile long prevT_i = 0;
float velocity_i = 0;
float v1Filt = 0, v1Prev = 0;
float vtFiltered = 0;
float kp = 1.6;
float ki = 1.2;
float kd = 3.0;
float e = 0, eprev = 0, eintegral = 0;
float e_integral_max = 600;
int pwr = 0;
long prevT = 0;
int posPrev = 0;

// ---------------------- REGISTRADORES MODBUS ----------------------
uint16_t regs[20]; // tabela usada pelo Laquis

// Slave ID = 1, usando Serial, sem pino DE
Modbus slave(1, Serial, 0);

// ==================================================================
// SETUP
// ==================================================================
void setup() {
  Serial.begin(115200);
  slave.start(); // inicia Modbus

  pinMode(ENCA, INPUT);
  pinMode(ENCB, INPUT);
  pinMode(PWM, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(ENCA), readEncoder, RISING);
}

// ==================================================================
// LOOP
// ==================================================================
void loop() {
  // ---------------- MODBUS ATIVO ----------------
  slave.poll(regs, 20);

  // ---------- KP, KI, KD RECEBIDOS DO LAQUIS (escala x100) ----------
  kp = regs[0] / 100.0;
  ki = regs[1] / 100.0;
  kd = regs[2] / 100.0;

  // ---------- SETPOINT VIRTUAL VINDO DO SCADA ----------
  float vt = regs[6]; // Setpoint virtual em RPM

  // filtro suave do setpoint
  vtFiltered = 0.9 * vtFiltered + 0.1 * vt;

  // ==================================================================
  // LEITURA DO ENCODER
  // ==================================================================
  int pos; float velEnc;
  noInterrupts();
  pos = pos_i;
  velEnc = velocity_i;
  interrupts();

  float deltaT;
  float velPos = computeVelocity(pos, deltaT);
  filterVelocity(velPos);

  // ==================================================================
  // PID
  // ==================================================================
  float u = computePID(vtFiltered, v1Filt, deltaT);
  controlMotor(u);

  // ==================================================================
  // ENVIO DOS VALORES PARA O SCADA
  // ==================================================================
  // Caso queira que o Laquis visualize o valor REAL de KP/KI/KD em tempo real
  regs[0] = (int)(kp * 100);
  regs[1] = (int)(ki * 100);
  regs[2] = (int)(kd * 100);

  regs[8] = (int)vtFiltered; // SP final filtrado
  regs[9] = (int)v1Filt;    // Velocidade RPM medida
  regs[10] = pwr;           // PWM atual

  delay(50);
}

// ==================================================================
// FUNÇÕES
// ==================================================================
// Calcula velocidade derivada da posição SEM PICO
float computeVelocity(int pos, float &deltaT) {
  long currT = micros();
  deltaT = (currT - prevT) / 1e6;
  if (deltaT < 0.001) return v1Filt;
  float vel = (pos - posPrev) / deltaT;
  posPrev = pos;
  prevT = currT;
  float rpm = vel / 600.0 * 60.0;
  if (rpm > 300) rpm = 300;
  if (rpm < -300) rpm = -300;
  return rpm;
}

// Filtro exponencial da velocidade
void filterVelocity(float v) {
  v1Filt = 0.92 * v1Filt + 0.08 * v;
}

// PID completo
float computePID(float vt, float vm, float dt) {
  e = vt - vm;
  eintegral += e * dt;
  if (eintegral > e_integral_max) eintegral = e_integral_max;
  if (eintegral < -e_integral_max) eintegral = -e_integral_max;
  float dedt = (e - eprev) / dt;
  eprev = e;
  float u = kp * e + ki * eintegral + kd * dedt;
  return u;
}

// controle do motor
void controlMotor(float u) {
  int pwrLocal = constrain(abs(u), 0, 255);
  if (u > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }
  analogWrite(PWM, pwrLocal);
  pwr = pwrLocal;
}

// interrupção
void readEncoder() {
  int b = digitalRead(ENCB);
  if (b > 0) pos_i++;
  else pos_i--;
}