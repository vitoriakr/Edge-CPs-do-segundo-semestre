/**...para o pull request dois
 * @file esp32.cpp
 * @brief
 * Firmware do ESP32 (simulado com o ESP32_mqtt_mock do professor).
 *
 * - Lê pela Serial (UART) as linhas enviadas pelo Arduino:
 *       DADOS,temperatura,luz,servo,led      (temperatura/luz = NA se a leitura for inválida)
 * - Converte para o formato UltraLight 2.0 do FIWARE e publica por MQTT:
 *       t|26.8|l|421|s|1|d|0
 * - Responde "ACK\n" ao Arduino quando a publicação deu certo
 *   (o Arduino usa o ACK para validar a comunicação).
 *
 * Atributos (object_id) que o dispositivo precisa ter no FIWARE:
 *   t = temperatura | l = luz | s = servo (0/1) | d = LED discoteca (0/1)
 */
#include "esp32.h"
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>

// ---------- CONFIGURAÇÃO ----------
const char* broker_mqtt      = "localhost";
const int   broker_mqtt_port = 1883;

// device_id: o mesmo usado no provisionamento do dispositivo no FIWARE
const char* topic_subscribe_cmd = "/TEF/quarto001/cmd";
const char* topic_publish_attrs = "/TEF/quarto001/attrs";
const char* default_ID_MQTT     = "quarto_001";

// ---------- OBJETOS ----------
ArduinoMQTT MQTT;
SerialCom   Serial;

static std::string linhaRx; // linha que está sendo montada com os bytes recebidos

// Divide "a,b,c" em partes
static std::vector<std::string> dividir(const std::string& texto, char separador) {
  std::vector<std::string> partes;
  std::string atual;
  for (char c : texto) {
    if (c == separador) { partes.push_back(atual); atual.clear(); }
    else atual += c;
  }
  partes.push_back(atual);
  return partes;
}

// Verifica se o texto é um número (inteiro ou decimal) válido
static bool ehNumero(const std::string& texto) {
  if (texto.empty()) return false;
  char* fim = nullptr;
  const double valor = std::strtod(texto.c_str(), &fim);
  return fim != texto.c_str() && *fim == '\0' && std::isfinite(valor);
}

// Converte "DADOS,26.8,421,1,0" em "t|26.8|l|421|s|1|d|0".
// Atributo com leitura inválida (NA) é omitido. Retorna false se a linha for inválida.
bool montarPayload(const std::string& linha, std::string& payload) {
  std::vector<std::string> campos = dividir(linha, ',');
  if (campos.size() != 5 || campos[0] != "DADOS") return false;

  const std::string& temp  = campos[1];
  const std::string& luz   = campos[2];
  const std::string& servo = campos[3];
  const std::string& led   = campos[4];

  if (servo != "0" && servo != "1") return false;
  if (led   != "0" && led   != "1") return false;
  if (temp != "NA" && !ehNumero(temp)) return false;
  if (luz  != "NA" && !ehNumero(luz))  return false;

  payload.clear();
  if (temp != "NA") payload += "t|" + temp + "|";
  if (luz  != "NA") payload += "l|" + luz  + "|";
  payload += "s|" + servo + "|d|" + led;
  return true;
}

// Toda vez que um comando chega, essa função é chamada (comandos ficam para o CP3)
void mqtt_callback(const char* topic, uint8_t* payload, unsigned int length) {
  String msg((char*)payload, length);
  std::cout << "\n----------Topico: " << topic << " | Payload: " << msg << std::endl;
}

// Garante conexão com o broker (tenta reconectar se caiu)
static bool garantirConexaoMQTT() {
  if (MQTT.connected()) return true;
  std::cout << "[MQTT] Desconectado. Tentando reconectar..." << std::endl;
  if (MQTT.connect(default_ID_MQTT)) {
    MQTT.subscribe(topic_subscribe_cmd);
    std::cout << "[MQTT] Reconectado." << std::endl;
    return true;
  }
  std::cout << "[MQTT] Falha ao reconectar." << std::endl;
  return false;
}

// Trata uma linha completa recebida do Arduino
static void tratarLinha(const std::string& linha) {
  if (linha.empty()) return;

  if (linha.compare(0, 5, "DADOS") != 0) {
    std::cout << "[Arduino] " << linha << std::endl; // mensagem de debug do Arduino
    return;
  }

  std::string payload;
  if (!montarPayload(linha, payload)) {
    std::cout << "[Aviso] Linha invalida ignorada: " << linha << std::endl;
    return;
  }

  if (garantirConexaoMQTT() && MQTT.publish(topic_publish_attrs, payload.c_str())) {
    std::cout << "[MQTT] Enviado em " << topic_publish_attrs << ": " << payload << std::endl;
    Serial.writeBytes("ACK\n", 4); // confirma para o Arduino
  } else {
    std::cout << "[MQTT] Nao enviado (sem conexao). Sem ACK para o Arduino." << std::endl;
  }
}

// Lê os bytes disponíveis na Serial, sem travar, e monta as linhas.
// Lê apenas a quantidade de bytes disponível, respeitando o tamanho do buffer.
static void lerSerial() {
  char buffer[512];

  int disponiveis = Serial.available();
  if (disponiveis <= 0) return;
  if (disponiveis > 256) disponiveis = 256;

  int lidos = Serial.readBytes(buffer, disponiveis);
  for (int i = 0; i < lidos; i++) {
    char c = buffer[i];
    if (c == '\n') {
      tratarLinha(linhaRx);
      linhaRx.clear();
    } else if (c != '\r') {
      linhaRx += c;
      if (linhaRx.size() > 100) linhaRx.clear(); // linha longa demais: descarta
    }
  }
}

void setup() {
  Serial.begin(9600); // mesma velocidade do Arduino

  MQTT.setServer(broker_mqtt, broker_mqtt_port);
  MQTT.setCallback(mqtt_callback);

  if (MQTT.connect(default_ID_MQTT)) {
    MQTT.subscribe(topic_subscribe_cmd);
    std::cout << "[MQTT] Conectado em " << broker_mqtt << ":" << broker_mqtt_port << std::endl;
  } else {
    std::cout << "[MQTT] Nao consegui conectar no broker (o FIWARE esta rodando?)" << std::endl;
  }

  delay(2000); // aguarda a serial inicializar
}

void loop() {
  MQTT.loop(); // processa mensagens recebidas (comandos)
  lerSerial();
  delay(50);
}