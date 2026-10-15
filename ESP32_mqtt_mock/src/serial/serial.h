
#ifndef SERIAL_H
#define SERIAL_H

#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <string>
#include <cstdint>

class SerialCom {
public:
    SerialCom();
    ~SerialCom();

    void begin(int baudRate);
    int writeBytes(const char* data, int length);
    int readBytes(char* buffer, int length);
    int available();

    void setPortName(const char* port) {
        portname = port;
    }

private:
    int fd = -1;
    std::string portname = "/dev/ttyACM0";
    std::atomic<int> bytesToRead = 0;
    std::atomic<bool> running = false;

    std::thread readThread;
    std::queue<char> readBuffer;
    std::mutex bufferMtx;

    void readLoop();
};

#endif
