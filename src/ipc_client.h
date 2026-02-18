#pragma once

#include <string>
#include <nlohmann/json.hpp>

class IpcClient {
public:
    nlohmann::json send(const std::string& socket_path, const nlohmann::json& request);
};
