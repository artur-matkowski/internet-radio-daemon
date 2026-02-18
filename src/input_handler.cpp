#include "input_handler.h"
#include "log.h"
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

bool InputHandler::open(const std::string& device_path, bool grab) {
    fd_ = ::open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd_ < 0) {
        LOG_ERROR("cannot open evdev device %s: %s",
                  device_path.c_str(), strerror(errno));
        return false;
    }

    if (grab) {
        if (ioctl(fd_, EVIOCGRAB, 1) < 0) {
            LOG_WARN("EVIOCGRAB failed on %s: %s",
                     device_path.c_str(), strerror(errno));
        }
    }

    LOG_INFO("opened evdev device: %s (fd=%d)", device_path.c_str(), fd_);
    return true;
}

void InputHandler::close_device() {
    if (fd_ >= 0) {
        ioctl(fd_, EVIOCGRAB, 0);
        ::close(fd_);
        fd_ = -1;
    }
}

std::string InputHandler::keycode_name(int code) {
    const char* name = libevdev_event_code_get_name(EV_KEY, static_cast<unsigned int>(code));
    if (name) return name;
    return "KEY_" + std::to_string(code);
}

void InputHandler::process_event(std::function<std::string(int keycode)> lookup) {
    struct input_event ev;
    while (true) {
        ssize_t n = read(fd_, &ev, sizeof(ev));
        if (n != sizeof(ev)) break;

        if (ev.type != EV_KEY || ev.value != 1) continue;

        std::string key_name = keycode_name(ev.code);
        LOG_DEBUG("key down: %s (%d)", key_name.c_str(), ev.code);

        std::string action = lookup(ev.code);
        if (!action.empty() && callback_) {
            LOG_INFO("key %s → action %s", key_name.c_str(), action.c_str());
            callback_(action);
        }
    }
}

std::string InputHandler::scan_key(const std::string& device_path) {
    int fd = ::open(device_path.c_str(), O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("cannot open device for scan: %s", strerror(errno));
        return "";
    }

    struct input_event ev;
    while (true) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) continue;
        if (ev.type == EV_KEY && ev.value == 1) {
            ::close(fd);
            return keycode_name(ev.code);
        }
    }
}
