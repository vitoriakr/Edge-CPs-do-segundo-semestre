#include "serial.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

SerialCom::SerialCom() = default;

SerialCom::~SerialCom() {
    running = false;
    if (readThread.joinable()) {
        readThread.join();
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void SerialCom::begin(int baudRate) {
    speed_t speed;

    switch (baudRate) {
        case 1200: speed = B1200; break;
        case 2400: speed = B2400; break;
        case 4800: speed = B4800; break;
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default:
            throw std::runtime_error("Velocidade serial nao suportada: " +
                                     std::to_string(baudRate));
    }

    fd = open(portname.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        throw std::runtime_error("Erro ao abrir " + portname + ": " +
                                 std::string(std::strerror(errno)));
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        const std::string erro = std::strerror(errno);
        close(fd);
        fd = -1;
        throw std::runtime_error("tcgetattr(): " + erro);
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // Configuracao 8N1: 8 bits, sem paridade e 1 stop bit.
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    // Modo raw e leitura nao bloqueante.
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                     PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        const std::string erro = std::strerror(errno);
        close(fd);
        fd = -1;
        throw std::runtime_error("tcsetattr(): " + erro);
    }

    running = true;
    std::cout << "Porta serial iniciada: " << portname
              << " (" << baudRate << " baud)" << std::endl;
    readThread = std::thread(&SerialCom::readLoop, this);
}

void SerialCom::readLoop() {
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    char buffer[256];

    while (running) {
        const int resultado = poll(&pfd, 1, 250);
        if (resultado < 0) {
            if (errno == EINTR) continue;
            if (running) {
                std::cerr << "poll(): " << std::strerror(errno) << std::endl;
            }
            break;
        }
        if (resultado == 0) continue;

        if (pfd.revents & POLLIN) {
            const ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0) {
                {
                    std::lock_guard<std::mutex> lock(bufferMtx);
                    for (ssize_t i = 0; i < n; ++i) {
                        readBuffer.push(buffer[i]);
                    }
                }
                bytesToRead.fetch_add(static_cast<int>(n));
            } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
                if (running) {
                    std::cerr << "read(): " << std::strerror(errno) << std::endl;
                }
                break;
            }
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (running) std::cerr << "Erro ou desconexao da porta serial." << std::endl;
            break;
        }
    }
    running = false;
}

int SerialCom::available() {
    return bytesToRead.load();
}

int SerialCom::readBytes(char* buffer, int length) {
    if (buffer == nullptr || length <= 0) return 0;

    std::lock_guard<std::mutex> lock(bufferMtx);
    int bytesRead = 0;
    while (bytesRead < length && !readBuffer.empty()) {
        buffer[bytesRead++] = readBuffer.front();
        readBuffer.pop();
    }
    bytesToRead.fetch_sub(bytesRead);
    return bytesRead;
}

int SerialCom::writeBytes(const char* data, int length) {
    if (fd < 0 || data == nullptr || length <= 0) return 0;

    int totalWritten = 0;
    while (totalWritten < length) {
        const ssize_t n = write(fd, data + totalWritten,
                                static_cast<size_t>(length - totalWritten));
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Erro ao escrever na serial: "
                      << std::strerror(errno) << std::endl;
            return totalWritten;
        }
        if (n == 0) break;
        totalWritten += static_cast<int>(n);
    }
    return totalWritten;
}
