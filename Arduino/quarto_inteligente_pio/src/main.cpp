#include <Arduino.h>// usado apenas para PlatformIO,
/**
 * @file quarto_inteligente.ino
 * @brief
 * QUARTO INTELIGENTE (Arduino MEGA)
 *
 * - Sensor de temperatura (DHT) -> controla um servo (simula ar-condicionado)
 *     * Temperatura > 26°C  -> servo LIGA e fica oscilando (pra cima e pra baixo)
 *     * Temperatura < 25°C  -> servo DESLIGA e volta pra posição inicial (0°)
 * - Sensor de luz (LDR; potenciômetro no Wokwi) -> controla um LED RGB
 *     * Luz baixa  -> LED RGB entra em modo "DISCOTECA" (trocando de cor aleatoriamente)
 *     * Luz boa    -> LED RGB desliga
 * - Serial Monitor mostra: temperatura, luz, erros de leitura e mudanças de estado
 *
 * Sensor DHT: Wokwi = DHT22 | Bancada = DHT11 (troque DHTTYPE em dataTypes.h)
 *
 */
#include <avr/wdt.h> // watchdog
#include "dataTypes.h"
#include "sensors.h"
#include "atuadores.h"

EstadoSensores estadoAtual = {0.0, 0, false, false, false, false, false};

void setup() {
  Serial.begin(9600);

  iniciarSensores();
  iniciarAtuadores();
  

  Serial.println("=== QUARTO INTELIGENTE INICIADO ===");

  wdt_enable(WDTO_2S); // watchdog: se o programa travar por 2s, o Arduino reinicia sozinho
}

void loop() {
  lerSensoresEControlar(&estadoAtual);
  verificarRespostaESP32(&estadoAtual);
  moverServoSeLigado();
  atualizarDiscoteca();

  wdt_reset(); // avisa o watchdog que o programa está vivo e funcionando
}
