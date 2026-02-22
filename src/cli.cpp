#include "cli.h"
#include "ipc_client.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>

using json = nlohmann::json;

static json ipc(const std::string& socket_path, const json& request) {
    IpcClient client;
    return client.send(socket_path, request);
}

static void print_json(const json& j) {
    if (j.contains("status") && j["status"] == "error") {
        std::cerr << "Error: " << j.value("message", "unknown error") << "\n";
        return;
    }
    if (j.contains("data")) {
        auto& d = j["data"];
        if (d.is_string()) {
            std::cout << d.get<std::string>() << "\n";
        } else {
            std::cout << d.dump(2) << "\n";
        }
    } else {
        std::cout << j.dump(2) << "\n";
    }
}

static int cmd_play(const std::string& sock, int argc, char* argv[]) {
    json req = {{"command", "play"}};
    if (argc > 1) {
        req["args"] = {{"station", std::atoi(argv[1])}};
    }
    print_json(ipc(sock, req));
    return 0;
}

static int cmd_stop(const std::string& sock) {
    print_json(ipc(sock, {{"command", "stop"}}));
    return 0;
}

static int cmd_next(const std::string& sock) {
    print_json(ipc(sock, {{"command", "next"}}));
    return 0;
}

static int cmd_prev(const std::string& sock) {
    print_json(ipc(sock, {{"command", "prev"}}));
    return 0;
}

static int cmd_volume(const std::string& sock, int argc, char* argv[]) {
    if (argc < 2) {
        print_json(ipc(sock, {{"command", "volume"}}));
        return 0;
    }
    std::string val = argv[1];
    json req = {{"command", "volume"}, {"args", {{"value", val}}}};
    print_json(ipc(sock, req));
    return 0;
}

static int cmd_list(const std::string& sock) {
    auto resp = ipc(sock, {{"command", "list"}});
    if (resp.contains("data") && resp["data"].is_array()) {
        auto& arr = resp["data"];
        for (size_t i = 0; i < arr.size(); ++i) {
            auto& s = arr[i];
            std::cout << (i + 1) << ". " << s.value("name", "?")
                      << "  [" << s.value("url", "") << "]\n";
        }
    } else {
        print_json(resp);
    }
    return 0;
}

static int cmd_status(const std::string& sock) {
    print_json(ipc(sock, {{"command", "status"}}));
    return 0;
}

static int cmd_reload(const std::string& sock) {
    print_json(ipc(sock, {{"command", "reload"}}));
    return 0;
}

int cli_dispatch(const std::string& socket_path, int argc, char* argv[]) {
    if (argc < 1) return 1;
    std::string cmd = argv[0];

    if (cmd == "play")    return cmd_play(socket_path, argc, argv);
    if (cmd == "stop")    return cmd_stop(socket_path);
    if (cmd == "next")    return cmd_next(socket_path);
    if (cmd == "prev")    return cmd_prev(socket_path);
    if (cmd == "volume")  return cmd_volume(socket_path, argc, argv);
    if (cmd == "list")    return cmd_list(socket_path);
    if (cmd == "status")  return cmd_status(socket_path);
    if (cmd == "reload")  return cmd_reload(socket_path);

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}
