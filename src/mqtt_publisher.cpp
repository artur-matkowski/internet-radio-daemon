#include "mqtt_publisher.h"
#include "log.h"
#include <cstring>

MqttPublisher::MqttPublisher() {
    mosquitto_lib_init();
    mosq_ = mosquitto_new("rpiradio", true, nullptr);
    if (!mosq_) {
        LOG_ERROR("mosquitto_new failed");
    }
}

MqttPublisher::~MqttPublisher() {
    disconnect();
    if (mosq_) {
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
    }
    mosquitto_lib_cleanup();
}

bool MqttPublisher::connect(const std::string& host, int port) {
    if (!mosq_) return false;

    int rc = mosquitto_connect(mosq_, host.c_str(), port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("MQTT connect to %s:%d failed: %s",
                  host.c_str(), port, mosquitto_strerror(rc));
        return false;
    }

    connected_ = true;
    LOG_INFO("MQTT connected to %s:%d", host.c_str(), port);
    return true;
}

void MqttPublisher::disconnect() {
    if (mosq_ && connected_) {
        mosquitto_disconnect(mosq_);
        connected_ = false;
    }
}

void MqttPublisher::pub(const std::string& subtopic, const std::string& payload) {
    if (!mosq_ || !connected_) return;

    std::string topic = prefix_ + "/" + subtopic;
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(),
                               static_cast<int>(payload.size()),
                               payload.c_str(), 1, true);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_WARN("MQTT publish to %s failed: %s",
                 topic.c_str(), mosquitto_strerror(rc));
    } else {
        LOG_DEBUG("MQTT publish %s: %s", topic.c_str(), payload.c_str());
    }
}

void MqttPublisher::publish_state(const std::string& state) {
    pub("state", state);
}

void MqttPublisher::publish_station(const std::string& json_str) {
    pub("station", json_str);
}

void MqttPublisher::publish_metadata(const std::string& title) {
    pub("metadata", title);
}

void MqttPublisher::publish_volume(int vol) {
    pub("volume", std::to_string(vol));
}
