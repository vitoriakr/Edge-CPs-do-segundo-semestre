#include "atuadores.h"
#include "dataTypes.h"
#include <Servo.h>

// ---------- ESTADO INTERNO (visível só neste arquivo) ----------
static Servo servoAr;
static bool servoLigado = false;
static bool ledLigado = false; // representa o modo "discoteca" ativo

// movimento do servo (pra cima e pra baixo) sem delay()
static int anguloServo = 0;
static int direcaoServo = 1; // 1 = subindo, -1 = descendo
static unsigned long ultimoMovimentoServo = 0;
static const unsigned long INTERVALO_SERVO = 15; // velocidade do movimento (ms)
static const int ANGULO_MIN = 0;
static const int ANGULO_MAX = 90;

// efeito discoteca do LED RGB
static unsigned long ultimaTrocaCor = 0;
static const unsigned long INTERVALO_DISCO = 500; // troca de cor a cada 500ms

static void escreverRGB(int r, int g, int b);

void iniciarAtuadores() {
  pinMode(PINO_LED_R, OUTPUT);
  pinMode(PINO_LED_G, OUTPUT);
  pinMode(PINO_LED_B, OUTPUT);

  servoAr.attach(PINO_SERVO);
  servoAr.write(0); // posição inicial

  escreverRGB(0, 0, 0); // LED apagado no início

  randomSeed(analogRead(A5)); // semente aleatória usando ruído de um pino não usado
}

bool servoEstaLigado() { return servoLigado; }
bool ledEstaLigado()   { return ledLigado; }

void ligarLed() {
  ledLigado = true;
  ultimaTrocaCor = 0; // força trocar de cor já no próximo loop
  Serial.println(">> LED RGB - MODO DISCOTECA LIGADO");
}

void desligarLed() {
  ledLigado = false;
  escreverRGB(0, 0, 0);
  Serial.println(">> LED RGB DESLIGADO");
}

void ligarServo() {
  servoLigado = true;
  Serial.println(">> SERVO (AR-CONDICIONADO) LIGADO");
}

void desligarServo() {
  servoLigado = false;
  anguloServo = 0;
  direcaoServo = 1;
  servoAr.write(0); // volta para a posição inicial
  Serial.println(">> SERVO (AR-CONDICIONADO) DESLIGADO - posição inicial");
}

// Move o servo pra cima e pra baixo enquanto estiver ligado (sem travar o programa)
void moverServoSeLigado() {
  if (!servoLigado) return;

  unsigned long agora = millis();
  if (agora - ultimoMovimentoServo < INTERVALO_SERVO) return;
  ultimoMovimentoServo = agora;

  anguloServo += direcaoServo;
  if (anguloServo >= ANGULO_MAX) {
    anguloServo = ANGULO_MAX;
    direcaoServo = -1;
  } else if (anguloServo <= ANGULO_MIN) {
    anguloServo = ANGULO_MIN;
    direcaoServo = 1;
  }

  servoAr.write(anguloServo);
}

// Enquanto o modo discoteca estiver ativo, troca a cor do LED aleatoriamente
void atualizarDiscoteca() {
  if (!ledLigado) return;

  unsigned long agora = millis();
  if (agora - ultimaTrocaCor < INTERVALO_DISCO) return;
  ultimaTrocaCor = agora;

  int r = random(0, 256);
  int g = random(0, 256);
  int b = random(0, 256);

  // garante que a cor não fique quase apagada (todas muito baixas)
  if (r < 60 && g < 60 && b < 60) {
    int canal = random(0, 3);
    if (canal == 0) r = 255;
    else if (canal == 1) g = 255;
    else b = 255;
  }

  escreverRGB(r, g, b);
}

// Escreve o brilho no LED RGB, tratando ânodo comum (sinal invertido)
static void escreverRGB(int r, int g, int b) {
  if (LED_ANODO_COMUM) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  analogWrite(PINO_LED_R, r);
  analogWrite(PINO_LED_G, g);
  analogWrite(PINO_LED_B, b);
}
