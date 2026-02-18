#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class IpcServer {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json& request)>;

    bool start(const std::string& socket_path);
    void stop();
    int fd() const { return listen_fd_; }
    void handle_connection();
    void set_handler(Handler h) { handler_ = std::move(h); }

private:
    int listen_fd_ = -1;
    std::string socket_path_;
    Handler handler_;
};
