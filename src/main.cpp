#include "config.h"
#include "log.h"
#include "daemon.h"
#include "cli.h"
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
              << "  reload              Reload config and stations\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char* cmd = argv[1];

    if (std::strcmp(cmd, "daemon") == 0) {
        Config cfg = config_load();
        log_init(cfg.log_level);
        return daemon_run(cfg);
    }

    return cli_dispatch(DEFAULT_IPC_SOCKET_PATH, argc - 1, argv + 1);
}
