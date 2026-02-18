#include "keybind_manager.h"
#include "input_handler.h"
#include "log.h"

void KeybindManager::load(const std::map<std::string, std::string>& bindings) {
    bindings_ = bindings;
    LOG_INFO("loaded %zu key bindings", bindings_.size());
}

std::string KeybindManager::lookup(int keycode) const {
    std::string name = InputHandler::keycode_name(keycode);
    auto it = bindings_.find(name);
    if (it != bindings_.end()) return it->second;
    return "";
}

void KeybindManager::set_binding(const std::string& key_name,
                                  const std::string& action) {
    bindings_[key_name] = action;
    LOG_INFO("bound %s → %s", key_name.c_str(), action.c_str());
}

void KeybindManager::remove_binding(const std::string& key_name) {
    bindings_.erase(key_name);
    LOG_INFO("removed binding for %s", key_name.c_str());
}
