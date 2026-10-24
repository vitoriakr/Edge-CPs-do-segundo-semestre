#ifndef ATUADORES_H
#define ATUADORES_H

#include <Arduino.h>

// Inicializa servo, LED RGB e semente aleatória
void iniciarAtuadores();

void ligarServo();
void desligarServo();
void ligarLed();
void desligarLed();

// Consulta o estado dos atuadores (as variáveis são privadas em atuadores.cpp)
bool servoEstaLigado();
bool ledEstaLigado();

// Chamadas a cada loop (sem delay)
void moverServoSeLigado();
void atualizarDiscoteca();

#endif
