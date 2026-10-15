#include "sensors.h"
#include "dataTypes.h"
#include "atuadores.h"
#include <DHT.h>   // DHT sensor library (Adafruit)

static DHT dht(PINO_DHT, DHTTYPE);

static const float TEMP_LIGA_SERVO    = 26.0; // acima disso, servo liga
static const float TEMP_DESLIGA_SERVO = 25.0; // abaixo disso, servo desliga
static const int   LUZ_LIMITE = 400;          // abaixo disso é "luz baixa" (0-1023)

// Faixas plausíveis para validar as leituras (detectar sensor com defeito/desconectado)
static const float TEMP_MIN_VALIDA = -20.0;
static const float TEMP_MAX_VALIDA = 100.0;
static const int   LEITURA_ADC_MIN = 1;    // 0 constante geralmente indica pino flutuando/desconectado
static const int   LEITURA_ADC_MAX = 1022; // 1023 constante geralmente indica curto/erro

// leitura periódica dos sensores (a cada 5 segundos: temperatura e luz variam devagar)
static unsigned long ultimaLeitura = 0;
static const unsigned long INTERVALO_LEITURA = 5000;
static unsigned long numeroLeitura = 0;

// ---------- CONFIRMAÇÃO (ACK) DO ESP32 ----------
static const unsigned long TIMEOUT_ACK = 2000; // ms esperando a resposta
static unsigned long ultimoEnvio = 0;
static bool aguardandoAck = false;

// ---------- FORMATAÇÃO DA SAÍDA NO MONITOR SERIAL ----------
static const char LINHA_GROSSA[] = "==================================================";
static const char LINHA_FINA[]   = "--------------------------------------------------";

static bool validarLeituraADC(int leitura);
static void enviarEstadoParaESP32(const EstadoSensores* estado);

static bool validarLeituraADC(int leitura) {
  return (leitura >= LEITURA_ADC_MIN && leitura <= LEITURA_ADC_MAX);
}

void iniciarSensores() {
  dht.begin();
}

// Envia: DADOS,temperatura,luz,servo,led
// (temperatura/luz = NA se a leitura for inválida)
// Esta linha precisa começar com "DADOS," e ficar sozinha na linha: é o protocolo com o ESP32.
static void enviarEstadoParaESP32(const EstadoSensores* estado) {
  Serial.print("DADOS,");
  if (estado->temperaturaValida) Serial.print(estado->temperatura, 1); else Serial.print("NA");
  Serial.print(',');
  if (estado->luzValida) Serial.print(estado->luz); else Serial.print("NA");
  Serial.print(',');
  Serial.print(estado->servoLigado ? 1 : 0);
  Serial.print(',');
  Serial.println(estado->ledLigado ? 1 : 0);
  ultimoEnvio = millis();
  aguardandoAck = true;
}

// Lê a temperatura e a luz, valida as leituras, mostra no Serial Monitor e
// decide automaticamente se liga/desliga LED e servo. Recebe um PONTEIRO
// para a struct de estado, evitando cópias desnecessárias.
void lerSensoresEControlar(EstadoSensores* estado) {
  unsigned long agora = millis();
  if (agora - ultimaLeitura < INTERVALO_LEITURA) return;
  ultimaLeitura = agora;
  numeroLeitura++;

  Serial.println();
  Serial.println(LINHA_GROSSA);
  Serial.print("LEITURA #");
  Serial.println(numeroLeitura);
  Serial.println(LINHA_GROSSA);

  float temperatura = dht.readTemperature(); // NaN se a leitura falhar
  int valorLuz = analogRead(PINO_LDR);

  // Validação das leituras (trata falha de sensor como erro)
  estado->temperaturaValida = !isnan(temperatura) &&
                              (temperatura >= TEMP_MIN_VALIDA && temperatura <= TEMP_MAX_VALIDA);
  estado->luzValida = validarLeituraADC(valorLuz);

  if (estado->temperaturaValida) estado->temperatura = temperatura;
  if (estado->luzValida)         estado->luz = valorLuz;

  // Sensores
  Serial.print("  Temperatura : ");
  if (estado->temperaturaValida) { Serial.print(estado->temperatura, 1); Serial.println(" C"); }
  else                           { Serial.println("INVALIDA"); }

  Serial.print("  Luz         : ");
  if (estado->luzValida) Serial.println(estado->luz);
  else                   Serial.println("INVALIDA");

  if (!estado->temperaturaValida) {
    Serial.println("  >> ERRO: leitura de temperatura fora da faixa esperada (sensor desconectado/defeituoso?)");
  }
  if (!estado->luzValida) {
    Serial.println("  >> ERRO: leitura do LDR fora da faixa esperada (sensor desconectado/defeituoso?)");
  }

  // Lógica automática do servo (só age se a leitura for válida)
  if (estado->temperaturaValida) {
    if (estado->temperatura > TEMP_LIGA_SERVO && !servoEstaLigado()) {
      ligarServo();
    } else if (estado->temperatura < TEMP_DESLIGA_SERVO && servoEstaLigado()) {
      desligarServo();
    }
  }

  // Lógica automática do LED (modo discoteca), só age se a leitura for válida
  if (estado->luzValida) {
    if (estado->luz < LUZ_LIMITE && !ledEstaLigado()) {
      ligarLed();
    } else if (estado->luz >= LUZ_LIMITE && ledEstaLigado()) {
      desligarLed();
    }
  }

  // Guarda o estado dos atuadores na struct
  estado->servoLigado = servoEstaLigado();
  estado->ledLigado   = ledEstaLigado();

  // Atuadores
  Serial.print("  Servo (AR)  : ");
  Serial.println(estado->servoLigado ? "LIGADO" : "desligado");
  Serial.print("  LED RGB     : ");
  Serial.println(estado->ledLigado ? "DISCOTECA" : "desligado");
  Serial.println(LINHA_FINA);

  enviarEstadoParaESP32(estado);
}

// Lê (sem travar) o ACK do ESP32 e trata a perda de comunicação.
// O resultado fecha o bloco de cada leitura no Monitor Serial.
void verificarRespostaESP32(EstadoSensores* estado) {
  static char buffer[16];
  static uint8_t pos = 0;

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      buffer[pos] = '\0';
      if (strcmp(buffer, "ACK") == 0 && aguardandoAck) {
        aguardandoAck = false;
        estado->comunicacaoOk = true;
        Serial.println("  ESP32       : OK");
        Serial.println(LINHA_GROSSA);
      }
      pos = 0;
    } else if (c != '\r') {
      if (pos < sizeof(buffer) - 1) buffer[pos++] = c;
      else pos = 0; // linha longa demais: descarta
    }
  }

  if (aguardandoAck && millis() - ultimoEnvio > TIMEOUT_ACK) {
    aguardandoAck = false;
    estado->comunicacaoOk = false;
    Serial.println("  ESP32       : ERRO - sem resposta (comunicacao perdida)");
    Serial.println(LINHA_GROSSA);
  }
}
