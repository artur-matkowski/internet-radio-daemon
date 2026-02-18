#include "ipc_client.h"
#include "log.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

nlohmann::json IpcClient::send(const std::string& socket_path,
                                const nlohmann::json& request) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return {{"status", "error"}, {"message", "socket() failed"}};
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return {{"status", "error"},
                {"message", "cannot connect to daemon (is it running?)"}};
    }

    struct timeval tv{};
    tv.tv_sec = 5;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string msg = request.dump() + "\n";
    write(fd, msg.c_str(), msg.size());

    std::string buf;
    char tmp[4096];
    while (true) {
        ssize_t n = read(fd, tmp, sizeof(tmp) - 1);
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));
        if (buf.find('\n') != std::string::npos) break;
    }

    close(fd);

    auto pos = buf.find('\n');
    if (pos == std::string::npos) {
        return {{"status", "error"}, {"message", "no response from daemon"}};
    }

    try {
        return nlohmann::json::parse(buf.substr(0, pos));
    } catch (const nlohmann::json::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}
