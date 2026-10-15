#!/usr/bin/env bash
set -e

echo "Compilando..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --clean-first

echo "Iniciando ponte serial/MQTT..."
# Porta padrao: /dev/ttyACM0. Para outra porta, passe-a como argumento:
# ./build_and_run.sh /dev/ttyUSB0
# Wokwi: ainda precisa de uma ponte de serial da simulacao para o programa.
./build/ESP32Mock "${1:-/dev/ttyACM0}"