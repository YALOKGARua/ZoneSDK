#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace zsdk::ledger
{
    bool CleanStart();
    bool Stuck();
    std::vector<std::string> Closed();
    std::string Current();
    std::string Last();
    bool Blocked(std::string_view key);
    std::string Proven(std::string_view key);
    std::vector<std::string> Disabled();
    std::vector<std::string> Booted();
    std::vector<std::string> Watched();
    void Retry();
    bool Watch(const std::string &key);
    bool Unwatch(const std::string &key, const std::string &build);
    bool Calm();
    bool Preloaded(std::string_view key);
    bool Preloaded(std::string_view key, std::string_view build);
    size_t PreloadCount();
    bool Full();
    size_t WatchLimit();
    std::string Addon();

    class Window
    {
    public:
        explicit Window(std::string key);
        ~Window();
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        void Close();
        bool Armed() const;
        bool Open() const;
        bool Refused() const;
        const std::string &Key() const;

    private:
        std::string key;
        bool open = false;
        bool armed = false;
        bool refused = false;
    };
}
