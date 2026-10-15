#include <csignal>
#include <iostream>
#include <stdexcept>

#include "esp32.h"
#include "serial/serial.h"
#include "utils/utils.h"

namespace {
volatile std::sig_atomic_t executando = 1;

void tratarSinal(int) {
    executando = 0;
}
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // USB (arduino): use /dev/ttyACM0 (padrao) ou informe a porta.
    // Exemplo: ./build/ESP32Mock /dev/ttyUSB0
    // Wokwi: a simulacao nao cria automaticamente uma porta serial no WSL.
    // Para usar o Wokwi depois, sera necessario adaptar a entrada serial/mock.
    if (argc > 1) {
        Serial.setPortName(argv[1]);
    }

    std::signal(SIGINT, tratarSinal);
    std::signal(SIGTERM, tratarSinal);

    millis(); // Inicializa o relogio usado pelas funcoes que imitam o Arduino.

    try {
        setup();
        while (executando) {
            loop();
        }
        MQTT.disconnect();
        std::cout << "\nEncerrado." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Erro] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}




// #include <cerrno>
// #include <csignal>
// #include <cstring>
// #include <iostream>
// #include <poll.h>
// #include <stdexcept>
// #include <termios.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <memory.h>
// #include "esp32.h"
// #include "utils/utils.h"

// using namespace std;

// #include "serial/serial.h"

// int main(int argc, char* argv[])
// {
//     std::ios_base::sync_with_stdio(false); // Unsychronize C++ streams from C stdio
//     std::cin.tie(nullptr);                 // Untie cin from cout
//     millis(); // Obtém uma referência para mock do milllis (gambiarra forte)
//     setup();
    
//     while(true){loop();}

//      std::cout << "\nEncerrado.\n";

//     return 0;
// }