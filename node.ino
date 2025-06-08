
#include "config.h"
#include "EEPROM.h"
#include <SPI.h>

// ============================================================================

uint64_t joinEUI = RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  = RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[16] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[16] = { RADIOLIB_LORAWAN_NWK_KEY };

//definicoes iniciais de conexao
const LoRaWANBand_t Region = AU915;
const uint8_t subBand = 2;

SX1262 radio = new Module(41, 39, 42, 40);
LoRaWANNode node(&radio, &Region, subBand);


// ============================================================================




void setup() {
  Serial.begin(115200);
  EEPROM.begin(36);
  for (int i = 0; i < 36; i++) EEPROM.write(i, 0);
  EEPROM.commit();

  radio.begin();
  // radio.setRfSwitchPins(38, RADIOLIB_NC);

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  Serial.println("Conectando...");
  while (node.activateOTAA(LORAWAN_UPLINK_DATA_RATE) != RADIOLIB_LORAWAN_NEW_SESSION) {
    delay(15000);
    Serial.println("Tentando novamente...");
  }

  node.setADR(false);
  node.setDutyCycle(false);
  node.setClass(RADIOLIB_LORAWAN_CLASS_C);
  node.setDatarate(LORAWAN_UPLINK_DATA_RATE);


  Serial.println("✅ Sistema pronto!");
}

void loop() {
  // put your main code here, to run repeatedly:
}
