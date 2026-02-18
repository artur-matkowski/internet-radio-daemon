#include "station_manager.h"
#include "log.h"
#include <fstream>
#include <algorithm>

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string name_from_url(const std::string& url) {
    auto pos = url.rfind('/');
    if (pos != std::string::npos && pos + 1 < url.size())
        return url.substr(pos + 1);
    return url;
}

bool StationManager::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        LOG_ERROR("cannot open m3u file: %s", path.c_str());
        return false;
    }

    std::vector<Station> result;
    std::string line;
    std::string pending_name;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line.rfind("#EXTM3U", 0) == 0) continue;

        if (line.rfind("#EXTINF:", 0) == 0) {
            auto comma = line.find(',');
            if (comma != std::string::npos)
                pending_name = trim(line.substr(comma + 1));
            continue;
        }

        if (line[0] == '#') continue;

        Station st;
        st.url = line;
        st.name = pending_name.empty() ? name_from_url(line) : pending_name;
        pending_name.clear();
        result.push_back(std::move(st));
    }

    stations_ = std::move(result);
    if (!stations_.empty() && current_ < 0) current_ = 0;
    LOG_INFO("loaded %d stations from %s", count(), path.c_str());
    return true;
}

const Station* StationManager::get(int index) const {
    if (index < 0 || index >= count()) return nullptr;
    return &stations_[static_cast<size_t>(index)];
}

const Station* StationManager::current() const {
    return get(current_);
}

const Station* StationManager::next() {
    if (stations_.empty()) return nullptr;
    current_ = (current_ + 1) % count();
    return &stations_[static_cast<size_t>(current_)];
}

const Station* StationManager::prev() {
    if (stations_.empty()) return nullptr;
    current_ = (current_ - 1 + count()) % count();
    return &stations_[static_cast<size_t>(current_)];
}

bool StationManager::select(int index) {
    if (index < 0 || index >= count()) return false;
    current_ = index;
    return true;
}
