#include "daemon.h"
#include "log.h"
#include "station_manager.h"
#include "mpv_controller.h"
#include "mqtt_publisher.h"
#include "ipc_server.h"
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>

using json = nlohmann::json;

static volatile bool g_running = true;

static void publish_full_state(MqttPublisher& mqtt, MpvController& mpv,
                                StationManager& sm) {
    json state;
    auto* st = sm.current();
    if (st) {
        state["station"] = {{"index", sm.current_index() + 1},
                            {"name", st->name},
                            {"url", st->url}};
    }
    state["playing"] = mpv.is_playing();
    state["paused"] = mpv.is_paused();
    state["volume"] = mpv.get_volume();
    state["metadata"] = mpv.get_metadata();
    state["station_count"] = sm.count();

    mqtt.publish_state(state.dump());
}

static void do_play_station(MpvController& mpv, StationManager& sm,
                             MqttPublisher& mqtt, int index = -1) {
    if (index >= 0) sm.select(index);
    auto* st = sm.current();
    if (!st) return;
    mpv.play(st->url);
    mqtt.publish_station(json({{"index", sm.current_index() + 1},
                                {"name", st->name},
                                {"url", st->url}}).dump());
    publish_full_state(mqtt, mpv, sm);
}

static json handle_ipc(const json& req, Config& cfg,
                        StationManager& sm, MpvController& mpv,
                        MqttPublisher& mqtt) {
    std::string cmd = req.value("command", "");
    json args = req.value("args", json::object());

    if (cmd == "play") {
        int station = args.value("station", 0);
        if (station > 0) {
            do_play_station(mpv, sm, mqtt, station - 1);
        } else {
            auto* st = sm.current();
            if (st) mpv.play(st->url);
            publish_full_state(mqtt, mpv, sm);
        }
        return {{"status", "ok"}};
    }

    if (cmd == "stop") {
        mpv.stop();
        mqtt.publish_state(json({{"playing", false}, {"paused", false}}).dump());
        return {{"status", "ok"}};
    }

    if (cmd == "toggle") {
        if (!mpv.is_playing() && !mpv.is_paused()) {
            // Nothing loaded — start playing
            if (!sm.current() && sm.count() > 0) {
                sm.select(0);
            }
            if (sm.current()) {
                do_play_station(mpv, sm, mqtt);
            } else {
                return {{"status", "error"}, {"message", "no stations available"}};
            }
        } else {
            mpv.toggle_pause();
            publish_full_state(mqtt, mpv, sm);
        }
        return {{"status", "ok"}};
    }

    if (cmd == "next") {
        sm.next();
        do_play_station(mpv, sm, mqtt);
        return {{"status", "ok"}};
    }

    if (cmd == "prev") {
        sm.prev();
        do_play_station(mpv, sm, mqtt);
        return {{"status", "ok"}};
    }

    if (cmd == "volume") {
        std::string val = args.value("value", "");
        if (val.empty()) {
            int v = mpv.get_volume();
            return {{"status", "ok"}, {"data", v}};
        }
        int cur = mpv.get_volume();
        int target;
        if (val == "up") {
            target = cur + 5;
        } else if (val == "down") {
            target = cur - 5;
        } else {
            target = std::atoi(val.c_str());
        }
        mpv.set_volume(target);
        mqtt.publish_volume(target);
        return {{"status", "ok"}, {"data", target}};
    }

    if (cmd == "list") {
        json arr = json::array();
        for (auto& s : sm.list()) {
            arr.push_back({{"name", s.name}, {"url", s.url}});
        }
        return {{"status", "ok"}, {"data", arr}};
    }

    if (cmd == "status") {
        json state;
        auto* st = sm.current();
        if (st) {
            state["station"] = {{"index", sm.current_index() + 1},
                                {"name", st->name},
                                {"url", st->url}};
        }
        state["playing"] = mpv.is_playing();
        state["paused"] = mpv.is_paused();
        state["volume"] = mpv.get_volume();
        state["metadata"] = mpv.get_metadata();
        state["station_count"] = sm.count();
        return {{"status", "ok"}, {"data", state}};
    }

    if (cmd == "reload") {
        cfg = config_load();
        log_init(cfg.log_level);
        sm.load(cfg.m3u_path);
        LOG_INFO("config reloaded");
        return {{"status", "ok"}};
    }

    return {{"status", "error"}, {"message", "unknown command: " + cmd}};
}

int daemon_run(Config& cfg) {
    LOG_INFO("rpiRadio daemon starting");

    StationManager sm;
    if (!sm.load(cfg.m3u_path)) {
        LOG_WARN("no stations loaded — continue anyway");
    }

    MpvController mpv;
    if (!mpv.start(cfg.mpv_socket_path, cfg.mpv_extra_args)) {
        LOG_ERROR("failed to start mpv");
        return 1;
    }

    MqttPublisher mqtt;
    mqtt.set_prefix(cfg.topic_prefix);
    if (!mqtt.connect(cfg.mqtt_host, cfg.mqtt_port)) {
        LOG_WARN("MQTT connection failed — continuing without MQTT");
    }

    IpcServer ipc;
    if (!ipc.start(cfg.ipc_socket_path)) {
        LOG_ERROR("failed to start IPC server");
        mpv.shutdown();
        return 1;
    }

    ipc.set_handler([&](const json& req) -> json {
        return handle_ipc(req, cfg, sm, mpv, mqtt);
    });

    mpv.on_metadata([&](const std::string& title) {
        LOG_INFO("metadata: %s", title.c_str());
        mqtt.publish_metadata(title);
    });

    mpv.on_pause([&](bool paused) {
        publish_full_state(mqtt, mpv, sm);
        (void)paused;
    });

    // Block signals and use signalfd
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    int sig_fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (sig_fd < 0) {
        LOG_ERROR("signalfd: %s", strerror(errno));
        ipc.stop();
        mpv.shutdown();
        return 1;
    }

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        LOG_ERROR("epoll_create1: %s", strerror(errno));
        close(sig_fd);
        ipc.stop();
        mpv.shutdown();
        return 1;
    }

    auto add_fd = [&](int fd, uint32_t events = EPOLLIN) {
        if (fd < 0) return;
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    };

    add_fd(sig_fd);
    add_fd(ipc.fd());
    if (mpv.fd() >= 0) add_fd(mpv.fd());

    LOG_INFO("daemon ready, entering main loop");

    struct epoll_event events[8];
    while (g_running) {
        int nfds = epoll_wait(epfd, events, 8, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("epoll_wait: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == sig_fd) {
                struct signalfd_siginfo si{};
                if (read(sig_fd, &si, sizeof(si)) == sizeof(si)) {
                    if (si.ssi_signo == SIGHUP) {
                        LOG_INFO("SIGHUP — reloading config");
                        cfg = config_load();
                        log_init(cfg.log_level);
                        sm.load(cfg.m3u_path);
                    } else {
                        LOG_INFO("signal %d — shutting down", si.ssi_signo);
                        g_running = false;
                    }
                }
            } else if (fd == ipc.fd()) {
                ipc.handle_connection();
            } else if (fd == mpv.fd()) {
                mpv.process_events();
            }
        }
    }

    LOG_INFO("shutting down");
    close(epfd);
    close(sig_fd);
    ipc.stop();
    mpv.shutdown();
    mqtt.disconnect();

    return 0;
}
