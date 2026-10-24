#include "mqtt.h"

//------------------------------------------------------------------------------------------------//
ArduinoMQTT::ArduinoMQTT(){
}
//------------------------------------------------------------------------------------------------//
ArduinoMQTT::~ArduinoMQTT(){

}
//------------------------------------------------------------------------------------------------//
void ArduinoMQTT::setServer(const char * Domain, uint16_t Port){
    this->domain = Domain;
    this->port = Port;
}
//------------------------------------------------------------------------------------------------//
void ArduinoMQTT::setCallback(std::function<void(const char*, uint8_t*, unsigned int)> Callback){
   callbackPaho.set_callback_arduino(Callback);
}
//------------------------------------------------------------------------------------------------//
bool ArduinoMQTT::connected(){
    if(!mqttClient)
    {
        return false;
    }

    return mqttClient->is_connected();
}
//------------------------------------------------------------------------------------------------//
bool ArduinoMQTT::connect(const char* id){
    std::string addrMqtt;

    addrMqtt.append(domain);
    addrMqtt.append(":");
    addrMqtt.append(std::to_string(port));
    
    if(!mqttClient){
        mqttClient = std::make_unique<mqtt::async_client>(addrMqtt, id);

        mqttClient->set_callback(callbackPaho);
    }else{
        if(mqttClient->is_connected()) {return true;}
    }
        
    // Connection options
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);

    // Connect and wait until the connection is established
    try{
        mqttClient->connect(connOpts)->wait();
        return true;

    }catch (const mqtt::exception& e) 
    {
        std::cerr << "MQTT error: " << e.what() << '\n';
        return false;
    }

    return false;
}
//------------------------------------------------------------------------------------------------//
void ArduinoMQTT::disconnect(){
    if (!mqttClient || !mqttClient->is_connected()) return;
    try{
        mqttClient->disconnect()->wait();
    }catch (const mqtt::exception& e) 
    {
        std::cerr << "MQTT error: " << e.what() << '\n';
    }
}
//------------------------------------------------------------------------------------------------//
bool ArduinoMQTT::subscribe(const char* topic){
    if (!mqttClient || !mqttClient->is_connected() || topic == nullptr) {
        std::cerr << "Nao foi possivel assinar o topico MQTT: cliente desconectado ou topico invalido."
                  << std::endl;
        return false;
    }
    try{
        mqttClient->subscribe(topic, 0)->wait();
        return true;
    }
    catch(const mqtt::exception& e)
    {
        std::cerr << "Failed to subscribe: "
              << e.what() << std::endl;
        return false;
    }

    return false;
}
//------------------------------------------------------------------------------------------------//
bool ArduinoMQTT::publish(const char* topic, const char* payload){
    if (!mqttClient || !mqttClient->is_connected() ||
        topic == nullptr || payload == nullptr) {
        std::cerr << "Publicacao MQTT ignorada: cliente desconectado ou dados invalidos."
                  << std::endl;
        return false;
    }
    std::cout << "Publish: " << topic << " | " << payload << std::endl;
    auto message = mqtt::make_message(topic, payload);
    message->set_qos(0);

    try{
        mqttClient->publish(message)->wait();
        return true;
    }catch(const mqtt::exception& e){
        std::cerr << "Publish error: " << e.what() << std::endl;
        return false;
    }
}
//------------------------------------------------------------------------------------------------//
void ArduinoMQTT::loop(){
    callbackPaho.dispatch_msgs();
}

//------------------------------------------------------------------------------------------------//
void MqttCallback::message_arrived(mqtt::const_message_ptr msg)
{
    mqttQueueMsg m;
    m.topic = msg->get_topic();
    m.payload =  msg->to_string();
  
    std::lock_guard<std::mutex> lock(queueMutex);
    msgQueue.push(m);
}

void MqttCallback::dispatch_msgs(){
    while (true) {
        mqttQueueMsg m;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (msgQueue.empty()) return;
            m = msgQueue.front();
            msgQueue.pop();
        }
        if (callbackArduino) {
            callbackArduino(m.topic.c_str(),
                            reinterpret_cast<uint8_t*>(m.payload.data()),
                            static_cast<unsigned int>(m.payload.size()));
        }
    }
}

void MqttCallback::set_callback_arduino(std::function<void(const char*, uint8_t*, unsigned int)> cb)
{
    callbackArduino = cb;
}


