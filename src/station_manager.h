#pragma once

#include <string>
#include <vector>

struct Station {
    std::string name;
    std::string url;
};

class StationManager {
public:
    bool load(const std::string& path);
    const std::vector<Station>& list() const { return stations_; }
    const Station* get(int index) const;
    const Station* current() const;
    const Station* next();
    const Station* prev();
    bool select(int index);
    int current_index() const { return current_; }
    int count() const { return static_cast<int>(stations_.size()); }

private:
    std::vector<Station> stations_;
    int current_ = -1;
};
