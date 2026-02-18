#pragma once

#include <string>
#include <map>

class KeybindManager {
public:
    void load(const std::map<std::string, std::string>& bindings);
    std::string lookup(int keycode) const;
    void set_binding(const std::string& key_name, const std::string& action);
    void remove_binding(const std::string& key_name);
    const std::map<std::string, std::string>& list() const { return bindings_; }

private:
    std::map<std::string, std::string> bindings_;
};
