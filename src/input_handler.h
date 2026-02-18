#pragma once

#include <string>
#include <functional>

class InputHandler {
public:
    using ActionCallback = std::function<void(const std::string& action)>;

    bool open(const std::string& device_path, bool grab = false);
    void close_device();
    int fd() const { return fd_; }
    void process_event(std::function<std::string(int keycode)> lookup);
    void set_callback(ActionCallback cb) { callback_ = std::move(cb); }

    static std::string scan_key(const std::string& device_path);
    static std::string keycode_name(int code);

private:
    int fd_ = -1;
    ActionCallback callback_;
};
