#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void setFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.open(path, std::ios::app);
    }

    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = timestamp();
        std::cout << now << " " << msg << "\n";
        if (file_.is_open())
            file_ << now << " " << msg << "\n" << std::flush;
    }

    void err(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = timestamp();
        std::cerr << now << " " << msg << "\n";
        if (file_.is_open())
            file_ << now << " " << msg << "\n" << std::flush;
    }

private:
    Logger() = default;

    std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::mutex   mutex_;
    std::ofstream file_;
};

#define LOG(msg)  Logger::instance().log(msg)
#define LOGE(msg) Logger::instance().err(msg)