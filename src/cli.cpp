#include "cli.h"
#include "ipc_client.h"
#include "input_handler.h"
#include "keybind_manager.h"
#include "log.h"
#include <iostream>
#include <cstring>

using json = nlohmann::json;

static json ipc(const Config& cfg, const json& request) {
    IpcClient client;
    return client.send(cfg.ipc_socket_path, request);
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

static int cmd_play(const Config& cfg, int argc, char* argv[]) {
    json req = {{"command", "play"}};
    if (argc > 1) {
        req["args"] = {{"station", std::atoi(argv[1])}};
    }
    print_json(ipc(cfg, req));
    return 0;
}

static int cmd_stop(const Config& cfg) {
    print_json(ipc(cfg, {{"command", "stop"}}));
    return 0;
}

static int cmd_next(const Config& cfg) {
    print_json(ipc(cfg, {{"command", "next"}}));
    return 0;
}

static int cmd_prev(const Config& cfg) {
    print_json(ipc(cfg, {{"command", "prev"}}));
    return 0;
}

static int cmd_volume(const Config& cfg, int argc, char* argv[]) {
    if (argc < 2) {
        print_json(ipc(cfg, {{"command", "volume"}}));
        return 0;
    }
    std::string val = argv[1];
    json req = {{"command", "volume"}, {"args", {{"value", val}}}};
    print_json(ipc(cfg, req));
    return 0;
}

static int cmd_list(const Config& cfg) {
    auto resp = ipc(cfg, {{"command", "list"}});
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

static int cmd_status(const Config& cfg) {
    print_json(ipc(cfg, {{"command", "status"}}));
    return 0;
}

static int cmd_reload(const Config& cfg) {
    print_json(ipc(cfg, {{"command", "reload"}}));
    return 0;
}

static int cmd_bind(const Config& cfg, int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: bind <scan|list|set|remove> ...\n";
        return 1;
    }

    std::string sub = argv[1];

    if (sub == "list") {
        auto resp = ipc(cfg, {{"command", "bind_list"}});
        if (resp.contains("data") && resp["data"].is_object()) {
            for (auto& [k, v] : resp["data"].items()) {
                std::cout << k << " → " << v.get<std::string>() << "\n";
            }
        } else {
            print_json(resp);
        }
        return 0;
    }

    if (sub == "set") {
        if (argc < 4) {
            std::cerr << "Usage: bind set <key> <action>\n";
            return 1;
        }
        json req = {{"command", "bind_set"},
                     {"args", {{"key", argv[2]}, {"action", argv[3]}}}};
        print_json(ipc(cfg, req));
        return 0;
    }

    if (sub == "remove") {
        if (argc < 3) {
            std::cerr << "Usage: bind remove <key>\n";
            return 1;
        }
        json req = {{"command", "bind_remove"}, {"args", {{"key", argv[2]}}}};
        print_json(ipc(cfg, req));
        return 0;
    }

    if (sub == "scan") {
        if (argc < 3) {
            std::cerr << "Usage: bind scan <action>\n";
            return 1;
        }
        std::string action = argv[2];

        if (cfg.evdev_name.empty()) {
            std::cerr << "Error: no evdev_name configured\n";
            return 1;
        }

        std::string dev_path = InputHandler::resolve_by_name(cfg.evdev_name);
        if (dev_path.empty()) {
            std::cerr << "Error: device '" << cfg.evdev_name << "' not found\n";
            return 1;
        }

        std::cout << "Press a key on the remote..." << std::flush;
        std::string key = InputHandler::scan_key(dev_path);
        if (key.empty()) {
            std::cerr << "\nFailed to scan key\n";
            return 1;
        }
        std::cout << " captured: " << key << "\n";

        json req = {{"command", "bind_set"},
                     {"args", {{"key", key}, {"action", action}}}};
        auto resp = ipc(cfg, req);
        if (resp.value("status", "") == "ok") {
            std::cout << "Bound " << key << " → " << action << "\n";
        } else {
            print_json(resp);
        }
        return 0;
    }

    std::cerr << "Unknown bind subcommand: " << sub << "\n";
    return 1;
}

int cli_dispatch(const Config& cfg, int argc, char* argv[]) {
    if (argc < 1) return 1;
    std::string cmd = argv[0];

    if (cmd == "play")    return cmd_play(cfg, argc, argv);
    if (cmd == "stop")    return cmd_stop(cfg);
    if (cmd == "next")    return cmd_next(cfg);
    if (cmd == "prev")    return cmd_prev(cfg);
    if (cmd == "volume")  return cmd_volume(cfg, argc, argv);
    if (cmd == "list")    return cmd_list(cfg);
    if (cmd == "status")  return cmd_status(cfg);
    if (cmd == "reload")  return cmd_reload(cfg);
    if (cmd == "bind")    return cmd_bind(cfg, argc, argv);

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}
