#ifndef ESP32_H
#define ESP32_H

#include "utils/utils.h"
#include <iostream>
#include <string>
#include <cstring>
#include "serial/serial.h"
#include "mqtt/mqtt.h"

using namespace std;
typedef std::string String;

extern ArduinoMQTT MQTT;
extern SerialCom Serial;

void setup(void);
void loop(void);


#endif