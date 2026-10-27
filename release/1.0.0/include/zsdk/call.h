#pragma once

#include "verbs.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace zsdk
{
    struct Refused : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct Fault : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct Fn
    {
        const void *function = nullptr;
        const void *owner = nullptr;
        ue::Sign sign;
        bool missing = true;
        bool widened = false;
    };

    Fn &Lookup(const char *owner, const char *name, const void *target = nullptr);

#pragma warning(push)
#pragma warning(disable : 4324)
    class Call
    {
    public:
        Call(std::span<const verbs::Spec> table, int verb, const void *on = nullptr);
        Call(const char *owner, const char *name, const void *on = nullptr);

        bool Has(const char *name) const;
        const ue::Param *Find(const char *name) const;
        const uint8_t *Data() const;
        const ue::Param &Parameter(const char *name) const;
        void Put(const char *name, const void *source, size_t bytes);

        template<class T> void Set(const char *name, const T &value)
        {
            Put(name, &value, sizeof(T));
        }

        template<class T> void Try(const char *name, const T &value)
        {
            const ue::Param *p = Find(name);
            if (p && p->size == static_cast<int32_t>(sizeof(T))) std::memcpy(block.data() + p->at, &value, sizeof(T));
        }

        void Wide(const char *name, int64_t value);
        int64_t Widened(const char *name) const;
        void CopyFrom(const char *name, const Call &source, const char *sourceName);

        template<class T> T Get(const char *name) const
        {
            const ue::Param &p = Parameter(name);
            if (p.size != static_cast<int32_t>(sizeof(T))) throw std::runtime_error(std::string("unexpected result size ") + name);
            T out{};
            std::memcpy(&out, block.data() + p.at, sizeof(T));
            return out;
        }

        bool Yes(const char *name) const
        {
            return Get<uint8_t>(name) != 0;
        }

        std::wstring CopyText(const char *name) const;

        const std::string &Key() const;
        const Fn &Function() const;
        const void *Target() const;
        bool Risky() const;

    private:
        void Bind(const char *owner, const char *name, const void *on, bool fresh);
        friend void Fire(Call &call);
        friend void Fire(Call &call, std::string_view key);

        Fn *fn = nullptr;
        const void *target = nullptr;
        std::string key;
        bool risky = false;
        alignas(16) std::array<uint8_t, 2048> block{};
    };
#pragma warning(pop)

    void Fire(Call &call);
    void Fire(Call &call, std::string_view key);
}
