/*
  Projeto: Gerador de Sinais Interativo com Filtros (Comunicação Serial)
  Baseado no projeto original do Grupo: Anderson, Hiarley, Yuri
  Refatorado para controle 100% via Serial.
*/

#include <Arduino.h> // Para funções como constrain, fmod, etc.
#define PI 3.14159265358979323846

// --- Configurações Globais ---
#define BAUD_RATE       115200
#define PIN_PWM         6
#define NUM_AMOSTRAS    500
#define T_AMOSTRAGEM_MS 1
const float T_amostragem = T_AMOSTRAGEM_MS * 0.001;

// --- Parâmetros do Sinal ---
int   tipo_sinal        = 0;   // 1:DC  2:Senoidal  3:Pulso
int   tipo_filtro       = 0;   // 0:Nenhum 1:PB  2:PA  3:BP  4:BS
float freq_corte_rad    = 0.0;
float amplitude_sinal   = 0.0;
float freq_sinal        = 0.0;
float intensidade_ruido = 0.0;
float ciclo_ativo       = 0.0;

// --- Buffer de Comunicação Serial ---
String inputString = "";         // String para armazenar dados recebidos
bool stringComplete = false;     // Flag para indicar que a string foi recebida

// --- Estados dos Filtros IIR ---
float y_anterior_lpf = 0.0;
float y_anterior_hpf = 0.0;
float y_anterior_bpf_hpf = 0.0, y_anterior_bpf_lpf = 0.0;
float y_anterior_bsf_hpf = 0.0, y_anterior_bsf_lpf = 0.0;

// --- Protótipos ---
void serialEvent();
void processarComando(String comando);
void enviarStatus();
void exportarDados();
void resetParametros();
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
  pinMode(PIN_PWM, OUTPUT);
  randomSeed(analogRead(A4));
  inputString.reserve(64); // Reserva memória para a string de entrada
  resetParametros();
  Serial.println("Gerador de Sinais Serial - Pronto.");
  Serial.println("Envie 'STA' para ver os comandos e o status atual.");
}

// ====================== LOOP ======================
void loop() {
  if (stringComplete) {
    processarComando(inputString);
    inputString = "";
    stringComplete = false;
  }
}

// ====================== COMUNICAÇÃO SERIAL ======================

// Função chamada automaticamente quando dados seriais estão disponíveis
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }
}

// ====================== PROCESSAMENTO DE COMANDOS ======================

void processarComando(String comando) {
  comando.trim(); // Remove espaços em branco
  comando.toUpperCase(); // Converte para maiúsculas para facilitar a comparação

  // Comandos sem parâmetro
  if (comando.equals("GEN")) {
    exportarDados();
    return;
  }
  if (comando.equals("RST")) {
    resetParametros();
    Serial.println("OK:RST:0");
    return;
  }
  if (comando.equals("STA")) {
    enviarStatus();
    return;
  }

  // Comandos com parâmetro
  int separatorIndex = comando.indexOf(':');
  if (separatorIndex == -1) {
    Serial.println("ERRO:SINTAXE_INVALIDA. Formato esperado: COMANDO:VALOR");
    return;
  }

  String cmd = comando.substring(0, separatorIndex);
  String valStr = comando.substring(separatorIndex + 1);
  float valor = valStr.toFloat();

  if (cmd.equals("SIG")) { // Tipo de Sinal (1:DC, 2:Senoidal, 3:Pulso)
    if (valor >= 1 && valor <= 3) {
      tipo_sinal = (int)valor;
      Serial.print("OK:SIG:"); Serial.println(tipo_sinal);
    } else {
      Serial.println("ERRO:SIG_INVALIDO. Use 1 (DC), 2 (Senoidal) ou 3 (Pulso).");
    }
  } else if (cmd.equals("AMP")) { // Amplitude (0.0 a 5.0 V)
    if (valor >= 0.0 && valor <= 5.0) {
      amplitude_sinal = valor;
      Serial.print("OK:AMP:"); Serial.println(amplitude_sinal, 2);
    } else {
      Serial.println("ERRO:AMP_INVALIDA. Use valor entre 0.0 e 5.0.");
    }
  } else if (cmd.equals("FRQ")) { // Frequência (0.0 a 400.0 Hz)
    if (valor > 0.0 && valor <= 400.0) {
      freq_sinal = valor;
      Serial.print("OK:FRQ:"); Serial.println(freq_sinal, 2);
    } else {
      Serial.println("ERRO:FRQ_INVALIDA. Use valor entre 0.0 e 400.0.");
    }
  } else if (cmd.equals("DUT")) { // Ciclo Ativo (0 a 100 %)
    if (valor >= 0.0 && valor <= 100.0) {
      ciclo_ativo = valor;
      Serial.print("OK:DUT:"); Serial.println(ciclo_ativo, 2);
    } else {
      Serial.println("ERRO:DUT_INVALIDO. Use valor entre 0.0 e 100.0.");
    }
  } else if (cmd.equals("NOI")) { // Intensidade do Ruído (0.0 a 1.0)
    if (valor >= 0.0 && valor <= 1.0) {
      intensidade_ruido = valor;
      Serial.print("OK:NOI:"); Serial.println(intensidade_ruido, 2);
    } else {
      Serial.println("ERRO:NOI_INVALIDO. Use valor entre 0.0 e 1.0.");
    }
  } else if (cmd.equals("FIL")) { // Tipo de Filtro (0:Nenhum, 1:PB, 2:PA, 3:BP, 4:BS)
    if (valor >= 0 && valor <= 4) {
      tipo_filtro = (int)valor;
      Serial.print("OK:FIL:"); Serial.println(tipo_filtro);
    } else {
      Serial.println("ERRO:FIL_INVALIDO. Use 0 a 4.");
    }
  } else if (cmd.equals("FCC")) { // Frequência de Corte (0.0 a 500.0 Hz)
    if (valor > 0.0 && valor < 500.0) {
      freq_corte_rad = 2.0 * PI * valor; // Armazena em rad/s
      Serial.print("OK:FCC:"); Serial.println(valor, 2); // Retorna o valor em Hz
    } else {
      Serial.println("ERRO:FCC_INVALIDA. Use valor entre 0.0 e 500.0.");
    }
  } else {
    Serial.print("ERRO:COMANDO_DESCONHECIDO. Comando: "); Serial.println(cmd);
  }
}

void enviarStatus() {
  Serial.println("STATUS:");
  Serial.print("  SIG (Tipo de Sinal): "); Serial.println(tipo_sinal);
  Serial.print("  AMP (Amplitude V): "); Serial.println(amplitude_sinal, 2);
  Serial.print("  FRQ (Frequencia Hz): "); Serial.println(freq_sinal, 2);
  Serial.print("  DUT (Ciclo Ativo %): "); Serial.println(ciclo_ativo, 2);
  Serial.print("  NOI (Intensidade Ruido): "); Serial.println(intensidade_ruido, 2);
  Serial.print("  FIL (Tipo de Filtro): "); Serial.println(tipo_filtro);
  Serial.print("  FCC (Frequencia Corte Hz): "); Serial.println(freq_corte_rad / (2.0 * PI), 2);
  Serial.print("  NUM_AMOSTRAS: "); Serial.println(NUM_AMOSTRAS);
  Serial.print("  T_AMOSTRAGEM_MS: "); Serial.println(T_AMOSTRAGEM_MS);
}

void resetParametros() {
  tipo_sinal        = 0;
  tipo_filtro       = 0;
  freq_corte_rad    = 0.0;
  amplitude_sinal   = 0.0;
  freq_sinal        = 0.0;
  intensidade_ruido = 0.0;
  ciclo_ativo       = 0.0;
  // Resetar estados dos filtros
  y_anterior_lpf = y_anterior_hpf = 0.0;
  y_anterior_bpf_hpf = y_anterior_bpf_lpf = 0.0;
  y_anterior_bsf_hpf = y_anterior_bsf_lpf = 0.0;
}

// ====================== GERAÇÃO E FILTROS (CÓDIGO ORIGINAL) ======================

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
  // O ruído é gerado entre -1 e 1, multiplicado pela intensidade e pela faixa de 5V
  float ruido = ((float)random(-1000,1000)/2000.0) * intensidade_ruido * 5.0;
  return constrain(sinal + ruido, 0.0, 5.0);
}

float filtro_lpf_1ordem(float x, float wc, float &y_prev) {
  // wc já está em rad/s
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
  // Aproximação simples para filtro passa-banda
  float fc_low = fc*0.9;
  float fc_high = fc*1.1;
  float hpf = filtro_hpf_1ordem(x, fc_low, y_hpf_prev);
  return filtro_lpf_1ordem(hpf, fc_high, y_lpf_prev);
}

float filtro_bsf(float x, float fc, float &y_hpf_prev, float &y_lpf_prev) {
  // Aproximação simples para filtro rejeita-banda
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
  // Validação mínima antes de gerar
  if (tipo_sinal == 0) {
    Serial.println("ERRO:PARAMETRO_FALTANDO. Defina o tipo de sinal (SIG).");
    return;
  }
  if (tipo_sinal != 1 && freq_sinal == 0.0) {
    Serial.println("ERRO:PARAMETRO_FALTANDO. Defina a frequencia (FRQ).");
    return;
  }
  if (tipo_filtro != 0 && freq_corte_rad == 0.0) {
    Serial.println("ERRO:PARAMETRO_FALTANDO. Defina a frequencia de corte (FCC).");
    return;
  }

  // Resetar estados dos filtros antes de cada exportação
  y_anterior_lpf = y_anterior_hpf = 0.0;
  y_anterior_bpf_hpf = y_anterior_bpf_lpf = 0.0;
  y_anterior_bsf_hpf = y_anterior_bsf_lpf = 0.0;

  Serial.println("--- INICIO EXPORTACAO ---");
  Serial.println("Sinal_Original(V),Sinal_Filtrado(V)");

  for(int i=0;i<NUM_AMOSTRAS;i++){
    float s_base = gerarSinalBase(i);
    float s_noisy = adicionarRuido(s_base);
    float s_filt = aplicarFiltro(s_noisy);

    // Geração do sinal PWM (saída analógica simulada)
    // O valor de 51.0 é 255 / 5.0V, para mapear 0-5V para 0-255 PWM
    int pwm = (int)(s_noisy*51.0);
    analogWrite(PIN_PWM,constrain(pwm,0,255));

    // Exportação dos dados
    Serial.print("DADOS:");
    Serial.print(s_noisy,4);
    Serial.print(",");
    Serial.println(s_filt,4);

    delay(T_AMOSTRAGEM_MS);
  }

  Serial.println("--- FIM DE EXPORTACAO ---");
}
