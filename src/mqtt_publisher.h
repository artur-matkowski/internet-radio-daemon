#pragma once

#include <string>
#include <mosquitto.h>

class MqttPublisher {
public:
    MqttPublisher();
    ~MqttPublisher();

    bool connect(const std::string& host, int port);
    void disconnect();

    void publish_state(const std::string& state);
    void publish_station(const std::string& json_str);
    void publish_metadata(const std::string& title);
    void publish_volume(int vol);

    void set_prefix(const std::string& prefix) { prefix_ = prefix; }

private:
    void pub(const std::string& subtopic, const std::string& payload);

    struct mosquitto* mosq_ = nullptr;
    std::string prefix_ = "rpiradio";
    bool connected_ = false;
};
