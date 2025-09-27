#define L_VERDE     14
#define L_AMARELO   12
#define L_VERMELHO  26

const int TEMPO = 500; // tempo de delay em milissegundos

void setup() {
  pinMode(L_VERDE, OUTPUT);
  pinMode(L_AMARELO, OUTPUT);
  pinMode(L_VERMELHO, OUTPUT);
}

void piscarLed(int pino) {
  digitalWrite(pino, HIGH);
  delay(TEMPO);
  digitalWrite(pino, LOW);
  delay(TEMPO);
}

void loop() {
  piscarLed(L_VERDE);
  piscarLed(L_AMARELO);
  piscarLed(L_VERMELHO);
}