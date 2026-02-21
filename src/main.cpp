#include "config.h"
#include "log.h"
#include "daemon.h"
#include "cli.h"
#include "input_handler.h"
#include <cstring>
#include <iostream>

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <command> [args...]\n"
              << "\nCommands:\n"
              << "  daemon              Start the radio daemon\n"
              << "  play [N]            Play station N (1-based) or resume\n"
              << "  stop                Stop playback\n"
              << "  next                Next station\n"
              << "  prev                Previous station\n"
              << "  volume <N|up|down>  Set or adjust volume\n"
              << "  list                List stations\n"
              << "  status              Show current status\n"
              << "  devices             Select input device (interactive)\n"
              << "  bind scan <action>  Scan a key and bind it to action\n"
              << "  bind list           List current key bindings\n"
              << "  bind set <key> <action>  Set a key binding\n"
              << "  bind remove <key>   Remove a key binding\n"
              << "  reload              Reload config and stations\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    Config cfg = config_load();
    log_init(cfg.log_level);

    const char* cmd = argv[1];

    if (std::strcmp(cmd, "daemon") == 0) {
        return daemon_run(cfg);
    }

    if (std::strcmp(cmd, "devices") == 0) {
        auto devices = InputHandler::list_devices();
        if (devices.empty()) {
            std::cerr << "No input devices found (need read access to /dev/input/)\n";
            return 1;
        }
        std::cout << "Available input devices:\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << devices[i].name
                      << "  [" << devices[i].path << "]\n";
        }
        std::cout << "\nSelect device (1-" << devices.size() << "): " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line) || line.empty()) {
            std::cerr << "No selection made.\n";
            return 1;
        }
        int choice = std::atoi(line.c_str());
        if (choice < 1 || choice > static_cast<int>(devices.size())) {
            std::cerr << "Invalid selection.\n";
            return 1;
        }
        auto& selected = devices[static_cast<size_t>(choice - 1)];
        cfg.evdev_name = selected.name;
        config_save(cfg);
        std::cout << "Saved evdev_name: " << selected.name << "\n";
        return 0;
    }

    return cli_dispatch(cfg, argc - 1, argv + 1);
}
