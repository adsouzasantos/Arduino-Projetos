// =================== BIBLIOTECAS ===================
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =================== PINAGEM (ESP32) ===================
#define SS_PIN    5
#define RST_PIN   4
#define BOTAO_PIN 13
#define BUZZER_PIN 12  

// =================== OBJETOS ===================
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

// =================== DADOS DO JOGO ===================
const int totalPerguntas = 12;

/**
 * CHARADAS FORMATADAS PARA LCD 16x2
 * Cada linha tem no máximo 16 caracteres.
 */
const String perguntasOriginais[] = {
  "Eu faco miau e  sou fofinho...",   // Gato
  "Sou amarela e o  macaco adora!",   // Banana
  "Tenho listras e sou um felino!",   // Tigre
  "Sou vermelha e  tenho pintinhas",  // Morango
  "Eu dou leite e  faco muuu...",     // Vaca
  "Sou redonda e   faco suco!",       // Laranja
  "Sou o melhor    amigo do homem",   // Cachorro
  "Moro no mar e   sou gigante!",     // Baleia
  "Tenho coroa mas nao sou rei...",   // Abacaxi
  "Tenho bico      grande e lindo!",  // Tucano
  "Quanto e o dobro de 2?",        // 4
  "Quanto e 3+3?"         // 6
};


const String respostasOriginais[] = {
  "29 E8 92 05 ", // Gato
  "A1 CE 90 05 ", // Banana
  "0A DA 90 05 ", // Tigre
  "2B 40 93 05 ", // Morango
  "B6 CA 90 05 ", // Vaca
  "BD E5 90 05 ", // Laranja
  "2E ED F7 04 ", // Cachorro
  "51 18 93 05 ", // Baleia
  "82 27 91 05 ", // Abacaxi
  "71 0C F9 04 ", // Tucano
  "81 13 91 05 ", // 4
  "D3 00 91 05 "  // 6
};

String perguntas[totalPerguntas];
String respostasUID[totalPerguntas];

// =================== VARIÁVEIS DE CONTROLE ===================
int indicePergunta = 0;
int errosSeguidos = 0;
int pontuacao = 0;
bool jogoIniciado = false;
unsigned long tempoBloqueioRFID = 0;

int ultimoEstadoBotao = HIGH;
unsigned long ultimoTempoDebounce = 0;
const unsigned long delayDebounce = 50;

// =================== FUNÇÕES DE SOM DO BUZZER ===================

// Som de clique ao apertar botão
void somBotao() {
  tone(BUZZER_PIN, 1000, 100); // 1000 Hz por 100ms
}

// Som de acerto 
void somAcerto() {
  tone(BUZZER_PIN, 523, 150);  
  delay(150);
  tone(BUZZER_PIN, 659, 150);  
  delay(150);
  tone(BUZZER_PIN, 784, 200);  
  delay(200);
  noTone(BUZZER_PIN);
}

// Som de erro (tom descendente)
void somErro() {
  tone(BUZZER_PIN, 400, 150);  
  delay(150);
  tone(BUZZER_PIN, 300, 200);  
  delay(200);
  noTone(BUZZER_PIN);
}

// Som de vitória/fim de jogo
void somVitoria() {
  tone(BUZZER_PIN, 523, 150);  
  delay(150);
  tone(BUZZER_PIN, 659, 150);  
  delay(150);
  tone(BUZZER_PIN, 784, 150);  
  delay(150);
  tone(BUZZER_PIN, 1047, 300); 
  delay(300);
  tone(BUZZER_PIN, 784, 150);  
  delay(150);
  tone(BUZZER_PIN, 1047, 400); 
  delay(400);
  noTone(BUZZER_PIN);
}

// Som de início de jogo
void somInicio() {
  tone(BUZZER_PIN, 784, 100);  
  delay(120);
  tone(BUZZER_PIN, 1047, 150); 
  delay(150);
  noTone(BUZZER_PIN);
}

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  pinMode(BOTAO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);  
  
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  
  SPI.begin();
  rfid.PCD_Init();
  
  telaInicial();
}

// =================== LOOP PRINCIPAL ===================
void loop() {
  verificarBotao();
  if (!jogoIniciado) return;
  if (millis() < tempoBloqueioRFID) return;

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uidLido = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uidLido += "0";
      uidLido += String(rfid.uid.uidByte[i], HEX);
      uidLido += " ";
    }
    uidLido.toUpperCase();
    processarResposta(uidLido);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  delay(10); 
}

// =================== FUNÇÕES DE LÓGICA ===================

void verificarBotao() {
  int leitura = digitalRead(BOTAO_PIN);
  if (leitura != ultimoEstadoBotao) {
    ultimoTempoDebounce = millis();
  }
  if ((millis() - ultimoTempoDebounce) > delayDebounce) {
    if (leitura == LOW) { 
      somBotao(); // <<<< NOVO: Som ao apertar botão
      
      if (!jogoIniciado) {
        iniciarJogo();
      } else {
        pularPerguntaManual();
      }
      delay(500); 
      ultimoTempoDebounce = millis();
    }
  }
  ultimoEstadoBotao = leitura;
}

void iniciarJogo() {
  jogoIniciado = true;
  indicePergunta = 0;
  errosSeguidos = 0;
  pontuacao = 0;
  embaralharPerguntas();
  
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("LUDCARDS");
  lcd.setCursor(2, 1);
  lcd.print("VAMOS JOGAR!");
  
  somInicio(); // <<<< NOVO: Som ao iniciar jogo
  delay(2000);
  
  mostrarPerguntaAtual();
}

void pularPerguntaManual() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(">> PULANDO >>");
  delay(800);
  proximaPergunta();
}

void mostrarPerguntaAtual() {
  lcd.clear();
  String p = perguntas[indicePergunta];
  
  if (p.length() > 16) {
    int corte = p.lastIndexOf(' ', 16);
    if (corte == -1) corte = 16;
    lcd.setCursor(0, 0);
    lcd.print(p.substring(0, corte));
    lcd.setCursor(0, 1);
    lcd.print(p.substring(corte + 1));
  } else {
    int centro = (16 - p.length()) / 2;
    lcd.setCursor(centro, 0);
    lcd.print(p);
  }
}

void processarResposta(String uid) {
  tempoBloqueioRFID = millis() + 2000;

  if (uid == respostasUID[indicePergunta]) {
    pontuacao++;
    errosSeguidos = 0;
    
    somAcerto(); 
    
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("VOCE BRILHOU!");
    lcd.setCursor(0, 1);
    lcd.print("Pontos: " + String(pontuacao));
    delay(2000);
    proximaPergunta();
  } else {
    errosSeguidos++;
    
    somErro(); 
    
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("QUASE LA!");
    lcd.setCursor(0, 1);
    lcd.print("Pontos: " + String(pontuacao));
    delay(2000);
    
    if (errosSeguidos >= 3) {
      lcd.clear();
      lcd.print("Vamos mudar...");
      lcd.setCursor(0, 1);
      lcd.print("Proxima ->");
      delay(1500);
      moverPerguntaParaFinal();
      errosSeguidos = 0;
      proximaPergunta();
    } else {
      mostrarPerguntaAtual();
    }
  }
}

void proximaPergunta() {
  indicePergunta++;
  if (indicePergunta >= totalPerguntas) {
    finalizarJogo();
  } else {
    mostrarPerguntaAtual();
  }
}

void moverPerguntaParaFinal() {
  String pAtual = perguntas[indicePergunta];
  String rAtual = respostasUID[indicePergunta];
  for (int i = indicePergunta; i < totalPerguntas - 1; i++) {
    perguntas[i] = perguntas[i + 1];
    respostasUID[i] = respostasUID[i + 1];
  }
  perguntas[totalPerguntas - 1] = pAtual;
  respostasUID[totalPerguntas - 1] = rAtual;
  indicePergunta--; 
}

void finalizarJogo() {
  somVitoria(); 
  
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("PARABENS!");
  lcd.setCursor(0, 1);
  lcd.print("Pontuacao: " + String(pontuacao));
  delay(2000);
  
  // Tela de jogar de novo
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Jogar de novo?");
  lcd.setCursor(0, 1);
  lcd.print("Aperte o botao!");
  
  jogoIniciado = false;
}

void telaInicial() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("LUDCARDS");
  lcd.setCursor(0, 1);
  lcd.print("Aperte o botao!");
}

void embaralharPerguntas() {
  for (int i = 0; i < totalPerguntas; i++) {
    perguntas[i] = perguntasOriginais[i];
    respostasUID[i] = respostasOriginais[i];
  }
  for (int i = totalPerguntas - 1; i > 0; i--) {
    int j = random(0, i + 1);
    String tempP = perguntas[i];
    perguntas[i] = perguntas[j];
    perguntas[j] = tempP;
    String tempR = respostasUID[i];
    respostasUID[i] = respostasUID[j];
    respostasUID[j] = tempR;
  }
}
