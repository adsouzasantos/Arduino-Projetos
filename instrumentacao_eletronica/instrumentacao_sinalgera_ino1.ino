/*
  Arquivo: gerador_sinais_v5.ino
  Projeto: Gerador de Sinais Interativo com Filtros (LCD/Keypad)
  Grupo: Anderson, Hiarley, Yuri
  Disciplina: Instrumentação Eletrônica
  Curso: Engenharia de Controle e Automação
  Instituição: IFPB - Campus Cajazeiras
*/

#define PI 3.14159265358979323846

#include <LiquidCrystal.h>
#include <Keypad.h>

// --- LCD 16x2 (RS, E, D4, D5, D6, D7) ---
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// --- Keypad 4x4 ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},   // C será usado para apagar
  {'*','0','#','D'}    // * = ponto decimal / D = voltar
};
byte rowPins[ROWS] = {10, 9, 8, 7};
byte colPins[COLS] = {A3, A2, A1, A0};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Configurações Globais ---
#define BAUD_RATE       115200
#define PIN_PWM         6
#define NUM_AMOSTRAS    500
#define T_AMOSTRAGEM_MS 1
const float T_amostragem = T_AMOSTRAGEM_MS * 0.001;

// --- Estados do Menu ---
enum MenuState {
  MENU_PRINCIPAL = 0,
  ENTRADA_AMPLITUDE,
  ENTRADA_FREQ_SINAL,
  ENTRADA_CICLO_ATIVO,
  ENTRADA_RUIDO,
  SELECAO_FILTRO,
  ENTRADA_FC,
  GERANDO_DADOS
};
MenuState estado_menu = MENU_PRINCIPAL;

// --- Parâmetros do Sinal ---
int   tipo_sinal        = 0;   // 1:DC  2:Senoidal  3:Pulso
int   tipo_filtro       = 0;   // 1:PB  2:PA  3:BP  4:BS
float freq_corte_rad    = 0.0;
float amplitude_sinal   = 0.0;
float freq_sinal        = 0.0;
float intensidade_ruido = 0.0;
float ciclo_ativo       = 0.0;

String inputBuffer = "";

// --- Estados dos Filtros IIR ---
float y_anterior_lpf = 0.0;
float y_anterior_hpf = 0.0;
float y_anterior_bpf_hpf = 0.0, y_anterior_bpf_lpf = 0.0;
float y_anterior_bsf_hpf = 0.0, y_anterior_bsf_lpf = 0.0;

// --- Controle exportação ---
bool exportando = false;

// --- Protótipos ---
void mostrarMenu();
void processarKey(char key);
void exportarDados();
float gerarSinalBase(int i);
float adicionarRuido(float sinal);
float aplicarFiltro(float sinal_com_ruido);
float filtro_lpf_1ordem(float x, float wc, float &y_prev);
float filtro_hpf_1ordem(float x, float wc, float &y_lpf_prev);
float filtro_bpf(float x, float fc, float &y_hpf_prev, float &y_lpf_prev);
float filtro_bsf(float x, float fc, float &y_hpf_prev, float &y_lpf_prev);

// ====================== SETUP ======================
void setup() {
  Serial.begin(BAUD_RATE);
  lcd.begin(16, 2);
  pinMode(PIN_PWM, OUTPUT);
  randomSeed(analogRead(A4));
  mostrarMenu();
}

// ====================== LOOP ======================
void loop() {
  char key = keypad.getKey();
  if (key != NO_KEY) processarKey(key);

  if (estado_menu == GERANDO_DADOS && !exportando) {
    exportando = true;
    exportarDados();
  }
}

// ====================== INTERFACE ======================
void mostrarMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  switch (estado_menu) {
    case MENU_PRINCIPAL:
      lcd.print("Escolha o Sinal:");
      lcd.setCursor(0, 1);
      lcd.print("1:DC 2:SEN 3:PUL");
      break;
    case ENTRADA_AMPLITUDE:
      lcd.print("Amplitude (V):");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      break;
    case ENTRADA_FREQ_SINAL:
      lcd.print("Frequencia (Hz):");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      break;
    case ENTRADA_CICLO_ATIVO:
      lcd.print("Ciclo Ativo (%):");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      break;
    case ENTRADA_RUIDO:
      lcd.print("Ruido (0-1):");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      break;
    case SELECAO_FILTRO:
      lcd.print("Selecione Filtro:");
      lcd.setCursor(0, 1);
      lcd.print("1:PB 2:PA 3:BP 4:BS");
      break;
    case ENTRADA_FC:
      lcd.print("Fc (Hz):");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      break;
    case GERANDO_DADOS:
      lcd.print("GERANDO DADOS...");
      lcd.setCursor(0, 1);
      lcd.print("Aguarde...");
      break;
  }
}

// ====================== PROCESSAMENTO DE TECLAS ======================
void processarKey(char key) {

  // MENU PRINCIPAL
  if (estado_menu == MENU_PRINCIPAL) {
    if (key >= '1' && key <= '3') {
      tipo_sinal = key - '0';
      inputBuffer = "";
      estado_menu = ENTRADA_AMPLITUDE;
      mostrarMenu();
    }
    return;
  }

  // SELEÇÃO DE FILTRO
  if (estado_menu == SELECAO_FILTRO) {
    if (key >= '1' && key <= '4') {
      tipo_filtro = key - '0';
      estado_menu = ENTRADA_FC;
      inputBuffer = "";
      mostrarMenu();
    }
    return;
  }

  // TECLA PARA APAGAR (BACKSPACE)
  if (key == 'C') {
    if (inputBuffer.length() > 0) {
      inputBuffer.remove(inputBuffer.length() - 1);
      mostrarMenu();
    }
    return;
  }

  // DIGITAÇÃO DE NÚMEROS
  if (key >= '0' && key <= '9') {
    if (inputBuffer.length() < 8) {
      inputBuffer += key;
      mostrarMenu();
    }
    return;
  }

  // PONTO DECIMAL
  if (key == '*') {
    if (inputBuffer.indexOf('.') == -1 && inputBuffer.length() < 7) {
      inputBuffer += '.';
      mostrarMenu();
    }
    return;
  }

  // CONFIRMAR
  if (key == '#') {
    if (inputBuffer.length() == 0) return;
    float valor = inputBuffer.toFloat();

    if (estado_menu == ENTRADA_AMPLITUDE) {
      if (valor >= 0.0 && valor <= 5.0) {
        amplitude_sinal = valor;
        if (tipo_sinal == 1) estado_menu = ENTRADA_RUIDO;
        else estado_menu = ENTRADA_FREQ_SINAL;
      } else {
        lcd.clear(); lcd.print("Amp 0-5V!"); delay(1000);
      }
    }
    else if (estado_menu == ENTRADA_FREQ_SINAL) {
      if (valor > 0 && valor <= 400) {
        freq_sinal = valor;
        estado_menu = (tipo_sinal == 3) ? ENTRADA_CICLO_ATIVO : ENTRADA_RUIDO;
      } else {
        lcd.clear(); lcd.print("Freq 1-400Hz!"); delay(1000);
      }
    }
    else if (estado_menu == ENTRADA_CICLO_ATIVO) {
      if (valor >= 0 && valor <= 100) {
        ciclo_ativo = valor;
        estado_menu = ENTRADA_RUIDO;
      } else {
        lcd.clear(); lcd.print("0-100%!"); delay(1000);
      }
    }
    else if (estado_menu == ENTRADA_RUIDO) {
      if (valor >= 0 && valor <= 1.0) {
        intensidade_ruido = valor;
        estado_menu = SELECAO_FILTRO;
      } else {
        lcd.clear(); lcd.print("Ruido 0-1!"); delay(1000);
      }
    }
    else if (estado_menu == ENTRADA_FC) {
      if (valor > 0 && valor < 500) {
        freq_corte_rad = 2.0 * PI * valor;
        estado_menu = GERANDO_DADOS;
      } else {
        lcd.clear(); lcd.print("Fc >0!"); delay(1000);
      }
    }

    inputBuffer = "";
    mostrarMenu();
    return;
  }

  // VOLTAR
  if (key == 'D') {
    inputBuffer = "";
    estado_menu = MENU_PRINCIPAL;
    exportando = false;
    mostrarMenu();
  }
}

// ====================== GERAÇÃO E FILTROS ======================
float gerarSinalBase(int i) {
  float t = i * T_amostragem;
  float s = 0.0;
  if (tipo_sinal == 1) s = amplitude_sinal;
  else if (tipo_sinal == 2) {
    float offset = 2.5;
    float amp = amplitude_sinal / 2.0;
    s = offset + amp * sin(2.0 * PI * freq_sinal * t);
  }
  else if (tipo_sinal == 3) {
    float periodo = 1.0 / freq_sinal;
    float t_mod = fmod(t, periodo);
    float t_on = periodo * (ciclo_ativo / 100.0);
    s = (t_mod < t_on) ? amplitude_sinal : 0.0;
  }
  return constrain(s, 0.0, 5.0);
}

float adicionarRuido(float sinal) {
  if (intensidade_ruido <= 0) return sinal;
  float ruido = ((float)random(-1000,1000)/2000.0) * intensidade_ruido * 5.0;
  return constrain(sinal + ruido, 0.0, 5.0);
}

float filtro_lpf_1ordem(float x, float wc, float &y_prev) {
  float alpha = T_amostragem / (T_amostragem + 1.0/wc);
  float y = alpha*x + (1.0-alpha)*y_prev;
  y_prev = y;
  return y;
}
float filtro_hpf_1ordem(float x, float wc, float &y_lpf_prev) {
  float lpf = filtro_lpf_1ordem(x, wc, y_lpf_prev);
  return x - lpf;
}
float filtro_bpf(float x, float fc, float &y_hpf_prev, float &y_lpf_prev) {
  float fc_low = fc*0.9;
  float fc_high = fc*1.1;
  float hpf = filtro_hpf_1ordem(x, fc_low, y_hpf_prev);
  return filtro_lpf_1ordem(hpf, fc_high, y_lpf_prev);
}
float filtro_bsf(float x, float fc, float &y_hpf_prev, float &y_lpf_prev) {
  return x - filtro_bpf(x, fc, y_hpf_prev, y_lpf_prev);
}

float aplicarFiltro(float x) {
  float y = x;
  switch(tipo_filtro){
    case 1: y=filtro_lpf_1ordem(x,freq_corte_rad,y_anterior_lpf); break;
    case 2: y=filtro_hpf_1ordem(x,freq_corte_rad,y_anterior_hpf); break;
    case 3: y=filtro_bpf(x,freq_corte_rad,y_anterior_bpf_hpf,y_anterior_bpf_lpf); break;
    case 4: y=filtro_bsf(x,freq_corte_rad,y_anterior_bsf_hpf,y_anterior_bsf_lpf); break;
  }
  return constrain(y,0.0,5.0);
}

// ====================== EXPORTAÇÃO ======================
void exportarDados() {
  y_anterior_lpf = y_anterior_hpf = 0.0;
  y_anterior_bpf_hpf = y_anterior_bpf_lpf = 0.0;
  y_anterior_bsf_hpf = y_anterior_bsf_lpf = 0.0;

  Serial.println("\n--- INICIO EXPORTACAO ---");
  Serial.println("Sinal_Original(V)\tSinal_Filtrado(V)");

  for(int i=0;i<NUM_AMOSTRAS;i++){
    float s_base=gerarSinalBase(i);
    float s_noisy=adicionarRuido(s_base);
    float s_filt=aplicarFiltro(s_noisy);

    int pwm = (int)(s_noisy*51.0);
    analogWrite(PIN_PWM,constrain(pwm,0,255));

    Serial.print(s_noisy,4);
    Serial.print("\t");
    Serial.println(s_filt,4);

    delay(T_AMOSTRAGEM_MS);
  }

  lcd.clear();
  lcd.print("Concluido!");
  lcd.setCursor(0,1);
  lcd.print("Press D p/ voltar");
}
