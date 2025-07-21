
#include "config.h"
#include "EEPROM.h"
#include <SPI.h>

// ============================================================================

uint64_t joinEUI = RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI = RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[16] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[16] = { RADIOLIB_LORAWAN_NWK_KEY };

//definicoes iniciais de conexao
const LoRaWANBand_t Region = AU915;
const uint8_t subBand = 2;

SX1262 radio = new Module(41, 39, 42, 40);
LoRaWANNode node(&radio, &Region, subBand);


// ============================================================================

// === ECG Simulado ===
#define MAX_AMOSTRAS 15000
#define TAM_PACOTE 100
int16_t ecgBuffer[MAX_AMOSTRAS];
uint16_t total = 0;
uint16_t indiceEnvio = 0;

// === Controle de estado ===

enum Estado { COLETANDO,
              ENVIANDO,
              PAUSANDO };
Estado estadoAtual = COLETANDO;

// ============================================================================


unsigned long tempoInicio, ultimoEnvio;
const unsigned long INTERVALO_COLETA = 2;    // 500Hz = a cada 2ms
const unsigned long DURACAO_COLETA = 30000;  // 30 segundos
const unsigned long INTERVALO_ENVIO = 200;   // a cada 0.2 segundos
const unsigned long PAUSA_FINAL = 15000;     // 1 minuto



void setup() {
  Serial.begin(115200);

  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("❌ Erro ao iniciar rádio: %d\n", state);
    while (true)
      ;
  }

  EEPROM.begin(36);
  for (int i = 0; i < 36; i++) EEPROM.write(i, 0);
  EEPROM.commit();

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  Serial.println("🔌 Conectando via OTAA...");

  while (node.activateOTAA(LORAWAN_UPLINK_DATA_RATE) != RADIOLIB_LORAWAN_NEW_SESSION) {
    delay(15000);
    Serial.println("🔄 Tentando novamente...");
  }

  node.setADR(false);
  node.setDutyCycle(false);
  node.setClass(RADIOLIB_LORAWAN_CLASS_C);  // modo sem espera de downlink
  node.setDatarate(LORAWAN_UPLINK_DATA_RATE);

  Serial.println("✅ Sistema pronto para envio Classe C!");

  // simular ECG
  for (int i = 0; i < MAX_AMOSTRAS; i++) {
    ecgBuffer[i] = 1500;  // Simula 1.5V em mV
  }

  tempoInicio = millis();
  ultimoEnvio = millis();
}

void loop() {
  unsigned long agora = millis();
  static int indiceEnvio = 0;

  if (estadoAtual == COLETANDO) {
    if ((agora - tempoInicio < DURACAO_COLETA) && total < MAX_AMOSTRAS) {
      static unsigned long ultimoColeta = 0;
      if (agora - ultimoColeta >= INTERVALO_COLETA) {
        ultimoColeta = agora;
        ecgBuffer[total++] = 1500;  // valor fixo simulado (1.5V em mV)
      }
    } else {
      Serial.println("⏫ Iniciando envio...");
      estadoAtual = ENVIANDO;
      tempoInicio = agora;
      indiceEnvio = 0;
      tempoTransmissaoInicio = millis();  // ⏱ início da transmissão
    }
  }

  else if (estadoAtual == ENVIANDO) {
    if ((agora - ultimoEnvio) >= INTERVALO_ENVIO && indiceEnvio < total) {
      ultimoEnvio = agora;

      uint8_t payload[2 * TAM_PACOTE];
      int blocos = min(TAM_PACOTE, total - indiceEnvio);
      for (int i = 0; i < blocos; i++) {
        payload[2 * i] = ecgBuffer[indiceEnvio + i] >> 8;
        payload[2 * i + 1] = ecgBuffer[indiceEnvio + i] & 0xFF;
      }

      int16_t status = node.sendReceive(payload, blocos * 2, LORAWAN_UPLINK_USER_PORT);
      if (status == RADIOLIB_ERR_NONE || status == RADIOLIB_LORAWAN_NO_DOWNLINK) {
        Serial.printf("📤 Pacote [%d-%d] enviado com sucesso\n", indiceEnvio, indiceEnvio + blocos - 1);
      } else {
        Serial.printf("❌ Erro ao enviar pacote [%d-%d]: %s\n", indiceEnvio, indiceEnvio + blocos - 1, stateDecode(status).c_str());
      }

      indiceEnvio += blocos;
    }

    if (indiceEnvio >= total) {
      tempoTransmissaoFim = millis();  // ⏱ fim da transmissão
      unsigned long duracao = tempoTransmissaoFim - tempoTransmissaoInicio;
      Serial.printf("✅ Transmissão completa em %.2f segundos\n", duracao / 1000.0);

      Serial.println("🕒 Pausa de 1 minutos...");
      tempoInicio = agora;
      estadoAtual = PAUSANDO;
    }
  }

  else if (estadoAtual == PAUSANDO) {
    if (agora - tempoInicio >= PAUSA_FINAL) {
      total = 0;
      tempoInicio = millis();
      estadoAtual = COLETANDO;
      Serial.println("🔁 Novo ciclo de coleta iniciado.");
    }
  }
}
