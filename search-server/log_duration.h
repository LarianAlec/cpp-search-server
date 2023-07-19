#pragma once
#include <chrono>
#include <string>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(operation_name) LogDuration UNIQUE_VAR_NAME_PROFILE(operation_name)
#define LOG_DURATION_STREAM(operation_name, stream_out) LogDuration UNIQUE_VAR_NAME_PROFILE(operation_name, stream_out)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    explicit LogDuration(const std::string& name, std::ostream& ostr = std::cerr)
        :  operation_name_(name)
        ,  out_(ostr)
    {}

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        out_ << operation_name_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }
private:
    const std::string operation_name_;
    std::ostream& out_;
    const Clock::time_point start_time_= Clock::now();
};