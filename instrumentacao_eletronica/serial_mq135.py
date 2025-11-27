import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import time

# -----------------------------
# Configurações iniciais
# -----------------------------
baudrate = 115200
max_pontos = 100  # quantidade de pontos no gráfico
alpha = 0.25      # parâmetro do filtro passa-baixa (maior valor = mais responsivo)

# -----------------------------
# Detecta automaticamente a porta do ESP32
# -----------------------------
ports = list(serial.tools.list_ports.comports())
esp_port = None
for p in ports:
    if "USB" in p.description or "COM" in p.device:
        esp_port = p.device
        break

if esp_port is None:
    print("ESP32 não encontrado. Conecte e tente novamente.")
    exit()

print(f"ESP32 encontrado na porta: {esp_port}")

# -----------------------------
# Inicializa serial
# -----------------------------
try:
    ser = serial.Serial(esp_port, baudrate, timeout=1)
except Exception as e:
    print("Erro ao abrir porta serial:", e)
    exit()

time.sleep(2)               # espera ESP32 reiniciar
ser.reset_input_buffer()    # limpa qualquer dado antigo da serial

# -----------------------------
# Estruturas de dados para o gráfico
# -----------------------------
dados_cru = deque([0]*max_pontos, maxlen=max_pontos)
dados_filtrados = deque([0]*max_pontos, maxlen=max_pontos)

# -----------------------------
# Função de atualização do gráfico
# -----------------------------
def animate(i):
    try:
        linha = ser.readline().decode('utf-8').strip()
        if linha:
            valor = int(linha)
            dados_cru.append(valor)

            # filtro passa-baixa
            if len(dados_filtrados) == 0:
                dados_filtrados.append(valor)
            else:
                ultimo = dados_filtrados[-1]
                filtrado = alpha*valor + (1-alpha)*ultimo
                dados_filtrados.append(filtrado)

            # atualiza gráficos
            ax1.clear()
            ax2.clear()

            ax1.plot(list(dados_cru), color='blue')
            ax1.set_title("Sinal Cru")
            ax1.set_ylim(0, 4095)

            ax2.plot(list(dados_filtrados), color='red')
            ax2.set_title("Sinal Filtrado (Passa-Baixa)")
            ax2.set_ylim(0, 4095)

    except Exception as e:
        print("Erro na leitura:", e)

# -----------------------------
# Configura o matplotlib
# -----------------------------
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10,6))
ani = animation.FuncAnimation(fig, animate, interval=50)  # atualiza rápido

plt.tight_layout()
plt.show()
