#include "mpv_controller.h"
#include "log.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <sstream>

bool MpvController::connect_socket(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    sock_fd_ = fd;
    return true;
}

bool MpvController::start(const std::string& socket_path,
                           const std::vector<std::string>& extra_args) {
    socket_path_ = socket_path;
    unlink(socket_path.c_str());

    int err_pipe[2];
    if (pipe(err_pipe) < 0) {
        LOG_ERROR("pipe failed: %s", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork failed: %s", strerror(errno));
        close(err_pipe[0]);
        close(err_pipe[1]);
        return false;
    }

    if (pid == 0) {
        close(err_pipe[0]);
        dup2(err_pipe[1], STDERR_FILENO);
        close(err_pipe[1]);

        // Reset signal mask so mpv can receive SIGTERM/SIGINT normally.
        // The parent daemon may have blocked signals before this fork,
        // or may do so in a future refactor — always clear in the child.
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, nullptr);

        std::vector<std::string> args = {
            "mpv", "--idle", "--no-video", "--no-terminal",
            "--input-ipc-server=" + socket_path
        };
        for (auto& a : extra_args) args.push_back(a);

        std::vector<char*> argv;
        for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp("mpv", argv.data());
        _exit(127);
    }

    close(err_pipe[1]);
    err_fd_ = err_pipe[0];
    fcntl(err_fd_, F_SETFL, fcntl(err_fd_, F_GETFL, 0) | O_NONBLOCK);

    mpv_pid_ = pid;
    LOG_INFO("started mpv pid=%d socket=%s", pid, socket_path.c_str());

    for (int i = 0; i < 50; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check if child already exited (e.g. mpv not found)
        int status;
        pid_t ret = waitpid(mpv_pid_, &status, WNOHANG);
        if (ret > 0) {
            // drain mpv stderr
            {
                char buf[2048];
                std::string output;
                ssize_t n;
                while ((n = read(err_fd_, buf, sizeof(buf) - 1)) > 0) {
                    buf[n] = '\0';
                    output.append(buf, n);
                }
                close(err_fd_);
                err_fd_ = -1;
                while (!output.empty() && output.back() == '\n') output.pop_back();
                if (!output.empty()) {
                    LOG_ERROR("mpv stderr: %s", output.c_str());
                }
            }
            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                LOG_ERROR("mpv not found — is it installed? (apt install mpv)");
            } else {
                LOG_ERROR("mpv exited prematurely with status %d", status);
            }
            mpv_pid_ = -1;
            return false;
        }

        if (connect_socket(socket_path)) {
            if (err_fd_ >= 0) { close(err_fd_); err_fd_ = -1; }
            LOG_INFO("connected to mpv IPC socket");

            int flags = fcntl(sock_fd_, F_GETFL, 0);
            fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

            send_command({{"command", {"observe_property", 1, "metadata"}},
                          {"request_id", 0}});
            send_command({{"command", {"observe_property", 2, "pause"}},
                          {"request_id", 0}});
            return true;
        }
    }

    // drain mpv stderr
    {
        char buf[2048];
        std::string output;
        ssize_t n;
        while ((n = read(err_fd_, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            output.append(buf, n);
        }
        close(err_fd_);
        err_fd_ = -1;
        while (!output.empty() && output.back() == '\n') output.pop_back();
        if (!output.empty()) {
            LOG_ERROR("mpv stderr: %s", output.c_str());
        }
    }
    LOG_ERROR("timeout connecting to mpv IPC socket");
    // Clean up the orphaned mpv child process
    if (mpv_pid_ > 0) {
        kill(mpv_pid_, SIGTERM);
        int status;
        waitpid(mpv_pid_, &status, 0);
        mpv_pid_ = -1;
    }
    return false;
}

void MpvController::shutdown() {
    if (err_fd_ >= 0) { close(err_fd_); err_fd_ = -1; }
    if (sock_fd_ >= 0) {
        send_command({{"command", {"quit"}}});
        close(sock_fd_);
        sock_fd_ = -1;
    }
    if (mpv_pid_ > 0) {
        int status;
        if (waitpid(mpv_pid_, &status, WNOHANG) == 0) {
            kill(mpv_pid_, SIGTERM);
            waitpid(mpv_pid_, &status, 0);
        }
        mpv_pid_ = -1;
    }
    playing_ = false;
    paused_ = false;
}

bool MpvController::send_command(const nlohmann::json& cmd) {
    if (sock_fd_ < 0) return false;
    std::string msg = cmd.dump() + "\n";
    ssize_t n = write(sock_fd_, msg.c_str(), msg.size());
    return n == static_cast<ssize_t>(msg.size());
}

nlohmann::json MpvController::send_command_sync(const nlohmann::json& cmd, int request_id) {
    nlohmann::json c = cmd;
    c["request_id"] = request_id;
    if (!send_command(c)) return nullptr;

    int prev_flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, prev_flags & ~O_NONBLOCK);

    struct pollfd pfd{};
    pfd.fd = sock_fd_;
    pfd.events = POLLIN;

    std::string buf;
    char tmp[4096];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() < deadline) {
        int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count());
        if (ms <= 0) break;
        if (poll(&pfd, 1, ms) <= 0) break;

        ssize_t n = read(sock_fd_, tmp, sizeof(tmp) - 1);
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));

        size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            try {
                auto j = nlohmann::json::parse(line);
                if (j.contains("request_id") && j["request_id"] == request_id) {
                    fcntl(sock_fd_, F_SETFL, prev_flags);
                    return j;
                }
            } catch (...) {}
        }
    }

    fcntl(sock_fd_, F_SETFL, prev_flags);
    return nullptr;
}

bool MpvController::play(const std::string& url) {
    LOG_INFO("play: %s", url.c_str());
    bool ok = send_command({{"command", {"loadfile", url, "replace"}}});
    if (ok) {
        playing_ = true;
        paused_ = false;
    }
    return ok;
}

bool MpvController::stop() {
    LOG_INFO("stop");
    bool ok = send_command({{"command", {"stop"}}});
    if (ok) {
        playing_ = false;
        paused_ = false;
    }
    return ok;
}

bool MpvController::toggle_pause() {
    int rid = next_req_id_++;
    auto resp = send_command_sync(
        {{"command", {"cycle", "pause"}}}, rid);
    if (resp.is_null()) {
        LOG_WARN("toggle_pause: no response from mpv");
        return false;
    }
    paused_ = !paused_;
    LOG_INFO("pause toggled → %s", paused_ ? "paused" : "playing");
    return true;
}

bool MpvController::set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 150) vol = 150;
    LOG_INFO("set volume: %d", vol);
    return send_command({{"command", {"set_property", "volume", vol}}});
}

int MpvController::get_volume() {
    int rid = next_req_id_++;
    auto resp = send_command_sync(
        {{"command", {"get_property", "volume"}}}, rid);
    if (!resp.is_null() && resp.contains("data"))
        return resp["data"].get<int>();
    return -1;
}

std::string MpvController::get_metadata() {
    int rid = next_req_id_++;
    auto resp = send_command_sync(
        {{"command", {"get_property", "media-title"}}}, rid);
    if (!resp.is_null() && resp.contains("data") && resp["data"].is_string())
        return resp["data"].get<std::string>();
    return "";
}

void MpvController::process_events() {
    if (sock_fd_ < 0) return;

    char buf[8192];
    static std::string remainder;

    ssize_t n = read(sock_fd_, buf, sizeof(buf) - 1);
    if (n <= 0) return;

    remainder.append(buf, static_cast<size_t>(n));

    size_t pos;
    while ((pos = remainder.find('\n')) != std::string::npos) {
        std::string line = remainder.substr(0, pos);
        remainder.erase(0, pos + 1);

        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            if (!j.contains("event")) continue;

            std::string event = j["event"];
            if (event == "property-change") {
                std::string name = j.value("name", "");
                if (name == "metadata" && meta_cb_) {
                    std::string title;
                    if (j.contains("data") && j["data"].is_object()) {
                        auto& d = j["data"];
                        if (d.contains("icy-title"))
                            title = d["icy-title"].get<std::string>();
                        else if (d.contains("title"))
                            title = d["title"].get<std::string>();
                    }
                    meta_cb_(title);
                } else if (name == "pause" && pause_cb_) {
                    bool p = j.value("data", false);
                    paused_ = p;
                    pause_cb_(p);
                }
            } else if (event == "end-file") {
                playing_ = false;
            }
        } catch (...) {}
    }
}
