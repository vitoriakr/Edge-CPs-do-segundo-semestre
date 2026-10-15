#ifndef DATATYPES_H
#define DATATYPES_H

#include <Arduino.h>

// ---------- TIPO DO SENSOR DE TEMPERATURA ----------
// Wokwi   -> DHT22
// Prototipo -> DHT11
// #define DHTTYPE DHT22
#define DHTTYPE DHT11

// ---------- PINOS ----------
const int PINO_DHT   = 6;     // sensor de temperatura (DHT)
const int PINO_LDR   = A1;    // sensor de luz (LDR; potenciômetro no Wokwi)
const int PINO_SERVO = 9;     // servomotor

// LED RGB - pinos PWM (~) do Mega.
const int PINO_LED_R = 12;
const int PINO_LED_G = 11;
const int PINO_LED_B = 10;

// Se o seu LED RGB for de ÂNODO COMUM, mude para true.
const bool LED_ANODO_COMUM = false;

// ---------- STRUCT: agrupa o estado dos sensores e dos atuadores ----------
// Facilita montar a mensagem para o ESP32 na próxima etapa.
struct EstadoSensores {
  float temperatura;
  int   luz;
  bool  temperaturaValida;
  bool  luzValida;
  bool  servoLigado;
  bool  ledLigado;
  bool  comunicacaoOk;
};

#endif
