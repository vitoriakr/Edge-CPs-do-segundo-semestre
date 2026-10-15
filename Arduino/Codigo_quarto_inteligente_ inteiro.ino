/*
  PROJETO: QUARTO INTELIGENTE (Arduino MEGA)
  -------------------------------------------
  - Sensor de temperatura (LM35) -> controla um servo (simula ar-condicionado)
      * Temperatura > 26°C  -> servo LIGA e fica oscilando (pra cima e pra baixo)
      * Temperatura < 25°C  -> servo DESLIGA e volta pra posição inicial (0°)
  - Sensor de luz (LDR) -> controla um LED RGB
      * Luz baixa  -> LED RGB entra em modo "DISCOTECA" (trocando de cor aleatoriamente)
      * Luz boa    -> LED RGB desliga
  - Serial Monitor mostra: temperatura, luz, erros de leitura e mudanças de estado
*/
 
#include <Servo.h>
#include <DHT.h>   // no lugar de #include <DHT11.h>

#include <avr/wdt.h> // watchdog
 
// ---------- PINOS ----------
DHT11 dht11(6);
const int PINO_LDR    = A1;   // sensor de luz (LDR)
const int PINO_SERVO  = 9;    // servomotor
 
// LED RGB - pinos PWM (~) do Mega.
const int PINO_LED_R = 12;
const int PINO_LED_G = 11;
const int PINO_LED_B = 10;
 
// Se o seu LED RGB for de ÂNODO COMUM, mude para true.
const bool LED_ANODO_COMUM = false;
 
// ---------- LIMITES (ajuste conforme seu sensor/ambiente) ----------
const float TEMP_LIGA_SERVO    = 26.0; // acima disso, servo liga
const float TEMP_DESLIGA_SERVO = 25.0; // abaixo disso, servo desliga
const int   LUZ_LIMITE = 400;          // abaixo disso é "luz baixa" (0-1023)
 
// Faixas plausíveis para validar as leituras (detectar sensor com defeito/desconectado)
const float TEMP_MIN_VALIDA = -20.0;
const float TEMP_MAX_VALIDA = 100.0;
const int   LEITURA_ADC_MIN = 1;    // 0 constante geralmente indica pino flutuando/desconectado
const int   LEITURA_ADC_MAX = 1022; // 1023 constante geralmente indica curto/erro
 
// ---------- STRUCT: agrupa o estado dos sensores ----------
struct EstadoSensores {
  float temperatura;
  int   luz;
  bool  temperaturaValida;
  bool  luzValida;
};
 
EstadoSensores estadoAtual = {0.0, 0, false, false};
 
// ---------- OBJETOS E VARIÁVEIS DE ESTADO ----------
Servo servoAr;
 
bool servoLigado = false;
bool ledLigado = false; // representa o modo "discoteca" ativo
 
// controle do movimento do servo (pra cima e pra baixo) sem usar delay()
int anguloServo = 0;
int direcaoServo = 1; // 1 = subindo, -1 = descendo
unsigned long ultimoMovimentoServo = 0;
const unsigned long INTERVALO_SERVO = 15; // velocidade do movimento (ms)
const int ANGULO_MIN = 0;
const int ANGULO_MAX = 90;
 
// controle do efeito discoteca do LED RGB
unsigned long ultimaTrocaCor = 0;
const unsigned long INTERVALO_DISCO = 120; // troca de cor a cada 120ms
 
// controle de leitura periódica dos sensores (a cada 1 segundo)
unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO_LEITURA = 1000;
 
void setup() {
  Serial.begin(9600);
 
  pinMode(PINO_LED_R, OUTPUT);
  pinMode(PINO_LED_G, OUTPUT);
  pinMode(PINO_LED_B, OUTPUT);
 
  servoAr.attach(PINO_SERVO);
  servoAr.write(0); // posição inicial
 
  escreverRGB(0, 0, 0); // LED apagado no início
 
  randomSeed(analogRead(A5)); // "semente" aleatória usando ruído de um pino não usado
 
  Serial.println("=== QUARTO INTELIGENTE INICIADO ===");
 
  wdt_enable(WDTO_2S); // watchdog: se o programa travar por 2s, o Arduino reinicia sozinho
}
 
void loop() {
  lerSensoresEControlar(&estadoAtual);
  moverServoSeLigado();
  atualizarDiscoteca();
 
  wdt_reset(); // avisa o watchdog que o programa está vivo e funcionando
}
 
// ---------------------------------------------------
// Lê a temperatura e a luz, valida as leituras, mostra
// no Serial Monitor e decide automaticamente se
// liga/desliga LED e servo. Recebe um PONTEIRO para a
// struct de estado, evitando cópias desnecessárias.
// ---------------------------------------------------
void lerSensoresEControlar(EstadoSensores* estado) {
  unsigned long agora = millis();
  if (agora - ultimaLeitura < INTERVALO_LEITURA) return;
  ultimaLeitura = agora;
 
  // --- Temperatura (fórmula para sensor tipo LM35) ---
  int temperatura = dht11.readTemperature();
 
  // --- Luz (LDR) ---
  int valorLuz = analogRead(PINO_LDR);
 
  // --- Validação das leituras (trata falha de sensor como erro) ---
  estado->temperaturaValida = (temperatura > 0) &&
                             (temperatura >= TEMP_MIN_VALIDA && temperatura <= TEMP_MAX_VALIDA);
  estado->luzValida = validarLeituraADC(valorLuz);
 
  if (!estado->temperaturaValida) {
    Serial.println(">> ERRO: leitura de temperatura fora da faixa esperada (sensor desconectado/defeituoso?)");
  } else {
    estado->temperatura = temperatura;
  }
 
  if (!estado->luzValida) {
    Serial.println(">> ERRO: leitura do LDR fora da faixa esperada (sensor desconectado/defeituoso?)");
  } else {
    estado->luz = valorLuz;
  }
 
  Serial.print("Temperatura: ");
  if (estado->temperaturaValida) Serial.print(estado->temperatura); else Serial.print("INVALIDA");
  Serial.print(" C   |   Luz: ");
  if (estado->luzValida) Serial.println(estado->luz); else Serial.println("INVALIDA");
 
  // --- Lógica automática do servo (só age se a leitura for válida) ---
  if (estado->temperaturaValida) {
    if (estado->temperatura > TEMP_LIGA_SERVO && !servoLigado) {
      ligarServo();
    } else if (estado->temperatura < TEMP_DESLIGA_SERVO && servoLigado) {
      desligarServo();
    }
  }
 
  // --- Lógica automática do LED (modo discoteca), só age se a leitura for válida ---
  if (estado->luzValida) {
    if (estado->luz < LUZ_LIMITE && !ledLigado) {
      ligarLed();
    } else if (estado->luz >= LUZ_LIMITE && ledLigado) {
      desligarLed();
    }
  }
}
 
// ---------------------------------------------------
// Verifica se uma leitura de ADC (0-1023) está dentro
// de uma faixa plausível, indicando sensor conectado.
// ---------------------------------------------------
bool validarLeituraADC(int leitura) {
  return (leitura >= LEITURA_ADC_MIN && leitura <= LEITURA_ADC_MAX);
}
 
// ---------------------------------------------------
// Funções que ligam/desligam o LED (modo discoteca) e o servo,
// e avisam no Serial Monitor
// ---------------------------------------------------
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
 
// ---------------------------------------------------
// Move o servo pra cima e pra baixo enquanto estiver ligado
// (sem travar o programa, usando millis)
// ---------------------------------------------------
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
 
// ---------------------------------------------------
// Enquanto o modo discoteca estiver ativo, troca a cor do
// LED RGB aleatoriamente em intervalos curtos (sem delay!)
// ---------------------------------------------------
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
 
// ---------------------------------------------------
// Escreve os valores de brilho no LED RGB, já tratando
// o caso de ânodo comum (sinal invertido)
// ---------------------------------------------------
void escreverRGB(int r, int g, int b) {
  if (LED_ANODO_COMUM) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  analogWrite(PINO_LED_R, r);
  analogWrite(PINO_LED_G, g);
  analogWrite(PINO_LED_B, b);
}
