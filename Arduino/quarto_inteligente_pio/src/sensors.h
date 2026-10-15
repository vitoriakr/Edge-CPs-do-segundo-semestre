#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "dataTypes.h"

// Inicializa o sensor DHT
void iniciarSensores();

// Lê os sensores, valida, atualiza a struct e controla servo e LED
void lerSensoresEControlar(EstadoSensores* estado);

// Lê a resposta e detecta perda de comunicação
void verificarRespostaESP32(EstadoSensores* estado);

#endif
