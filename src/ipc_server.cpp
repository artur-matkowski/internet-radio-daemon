#include "ipc_server.h"
#include "log.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

bool IpcServer::start(const std::string& socket_path) {
    socket_path_ = socket_path;
    unlink(socket_path.c_str());

    listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        LOG_ERROR("socket(): %s", strerror(errno));
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("bind(%s): %s", socket_path.c_str(), strerror(errno));
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, 4) < 0) {
        LOG_ERROR("listen(): %s", strerror(errno));
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    LOG_INFO("IPC server listening on %s", socket_path.c_str());
    return true;
}

void IpcServer::stop() {
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    if (!socket_path_.empty()) {
        unlink(socket_path_.c_str());
        socket_path_.clear();
    }
}

void IpcServer::handle_connection() {
    int client = accept(listen_fd_, nullptr, nullptr);
    if (client < 0) return;

    struct timeval tv{};
    tv.tv_sec = 2;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string buf;
    char tmp[4096];
    while (true) {
        ssize_t n = read(client, tmp, sizeof(tmp) - 1);
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));
        if (buf.find('\n') != std::string::npos) break;
    }

    auto pos = buf.find('\n');
    if (pos == std::string::npos) {
        close(client);
        return;
    }

    std::string line = buf.substr(0, pos);
    nlohmann::json response;
    try {
        auto request = nlohmann::json::parse(line);
        LOG_DEBUG("IPC request: %s", line.c_str());
        if (handler_) {
            response = handler_(request);
        } else {
            response = {{"status", "error"}, {"message", "no handler"}};
        }
    } catch (const nlohmann::json::exception& e) {
        response = {{"status", "error"}, {"message", e.what()}};
    }

    std::string resp_str = response.dump() + "\n";
    write(client, resp_str.c_str(), resp_str.size());
    close(client);
}
