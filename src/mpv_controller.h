#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class MpvController {
public:
    using MetadataCallback = std::function<void(const std::string& title)>;
    using PauseCallback = std::function<void(bool paused)>;

    bool start(const std::string& socket_path,
               const std::vector<std::string>& extra_args = {});
    void shutdown();

    bool play(const std::string& url);
    bool stop();
    bool toggle_pause();
    bool set_volume(int vol);
    int get_volume();
    std::string get_metadata();
    bool is_playing() const { return playing_; }
    bool is_paused() const { return paused_; }

    int fd() const { return sock_fd_; }
    void process_events();

    void on_metadata(MetadataCallback cb) { meta_cb_ = std::move(cb); }
    void on_pause(PauseCallback cb) { pause_cb_ = std::move(cb); }

private:
    bool send_command(const nlohmann::json& cmd);
    nlohmann::json send_command_sync(const nlohmann::json& cmd, int request_id);
    void read_responses();
    bool connect_socket(const std::string& path);

    std::string socket_path_;
    int sock_fd_ = -1;
    int err_fd_ = -1;
    pid_t mpv_pid_ = -1;
    bool playing_ = false;
    bool paused_ = false;
    int next_req_id_ = 1;

    MetadataCallback meta_cb_;
    PauseCallback pause_cb_;
};
