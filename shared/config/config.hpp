#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <stdexcept>

class Config {
public:
    static Config& instance() {
        static Config inst;
        return inst;
    }

    void load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open())
            throw std::runtime_error("config: cannot open " + path);

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key   = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));

            if (value.size() >= 2 &&
                value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);

            data_[key] = value;
        }
    }

    std::string get(const std::string& key,
                    const std::string& default_val = "") const {
        auto it = data_.find(key);
        return it != data_.end() ? it->second : default_val;
    }

    int getInt(const std::string& key, int default_val = 0) const {
        auto it = data_.find(key);
        if (it == data_.end()) return default_val;
        try { return std::stoi(it->second); }
        catch (...) { return default_val; }
    }

    bool getBool(const std::string& key, bool default_val = false) const {
        auto val = get(key);
        if (val == "true" || val == "1" || val == "yes") return true;
        if (val == "false" || val == "0" || val == "no") return false;
        return default_val;
    }

private:
    Config() = default;

    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end   = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }

    std::unordered_map<std::string, std::string> data_;
};

#define CFG(key)              Config::instance().get(key)
#define CFG_DEF(key, def)     Config::instance().get(key, def)
#define CFG_INT(key, def)     Config::instance().getInt(key, def)
#define CFG_BOOL(key, def)    Config::instance().getBool(key, def)