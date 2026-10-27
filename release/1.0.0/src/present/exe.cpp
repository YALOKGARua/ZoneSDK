#include <zsdk/present.h>

#include "../detail/engine_core.h"

#include <windows.h>

#include <bcrypt.h>

#include <cstdio>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace zsdk::exe
{
    namespace
    {
        std::mutex gate;
        std::string sha;
        bool hashed = false;
        std::wstring pretended;

        wchar_t Low(wchar_t c)
        {
            return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c;
        }

        bool Same(std::wstring_view a, std::wstring_view b)
        {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (Low(a[i]) != Low(b[i])) return false;
            return true;
        }
    }

    std::wstring File()
    {
        {
            std::lock_guard<std::mutex> hold(gate);
            if (!pretended.empty()) return pretended;
        }
        wchar_t path[MAX_PATH]{};
        const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (!size || size >= MAX_PATH) return {};
        const wchar_t *tail = wcsrchr(path, L'\\');
        return tail ? std::wstring(tail + 1) : std::wstring(path);
    }

    std::string Name()
    {
        std::string out;
        for (const wchar_t c : File()) out.push_back(c > 0x20 && c < 0x7F ? static_cast<char>(c) : '?');
        return out;
    }

    bool Game(std::wstring_view file)
    {
        constexpr std::wstring_view head = L"Stalker2-";
        constexpr std::wstring_view tail = L"-Shipping.exe";
        if (file.size() <= head.size() + tail.size()) return false;
        if (!Same(file.substr(0, head.size()), head) || !Same(file.substr(file.size() - tail.size()), tail)) return false;
        for (const wchar_t c : file.substr(head.size(), file.size() - head.size() - tail.size()))
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'))) return false;
        return true;
    }

    bool Steam(std::wstring_view file)
    {
        return Same(file, kSteamFile);
    }

    bool Steam()
    {
        return Steam(File());
    }

    uintptr_t Base()
    {
        return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    }

    uint32_t ImageSize()
    {
        const uintptr_t base = Base();
        IMAGE_DOS_HEADER dos{};
        if (!ue::Copy(reinterpret_cast<const void *>(base), &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
        IMAGE_NT_HEADERS64 nt{};
        if (!ue::Copy(reinterpret_cast<const void *>(base + static_cast<uintptr_t>(dos.e_lfanew)), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE) return 0;
        return nt.OptionalHeader.SizeOfImage;
    }

    uint32_t Stamp()
    {
        const uintptr_t base = Base();
        IMAGE_DOS_HEADER dos{};
        if (!ue::Copy(reinterpret_cast<const void *>(base), &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
        IMAGE_NT_HEADERS64 nt{};
        if (!ue::Copy(reinterpret_cast<const void *>(base + static_cast<uintptr_t>(dos.e_lfanew)), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE) return 0;
        return nt.FileHeader.TimeDateStamp;
    }

    std::string Hash()
    {
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return {};
        const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE) return {};
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        std::string out;
        if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) && BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0)))
        {
            std::vector<uint8_t> buffer(1u << 20);
            DWORD got = 0;
            bool ok = true;
            while (!zsdk::thread::Stopping() && ::ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr) && got)
                if (!BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(), got, 0)))
                {
                    ok = false;
                    break;
                }
            uint8_t digest[32]{};
            if (ok && !zsdk::thread::Stopping() && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0)))
            {
                char hex[65]{};
                for (int i = 0; i < 32; ++i) _snprintf_s(hex + i * 2, 3, _TRUNCATE, "%02X", digest[i]);
                out = hex;
            }
        }
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        CloseHandle(file);
        std::lock_guard<std::mutex> hold(gate);
        sha = out;
        hashed = true;
        return out;
    }

    std::string Sha()
    {
        std::lock_guard<std::mutex> hold(gate);
        return sha;
    }

    bool Hashed()
    {
        std::lock_guard<std::mutex> hold(gate);
        return hashed && !sha.empty();
    }

    std::string Short()
    {
        std::lock_guard<std::mutex> hold(gate);
        if (sha.size() < 8) return "?";
        std::string out = sha.substr(0, 8);
        for (char &c : out)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        return out;
    }

    const guard::Build *Build()
    {
        return guard::Find(Sha());
    }

    bool Known()
    {
        return Build() != nullptr;
    }
}

#if defined(ZSDK_TESTS)
namespace zsdk::detail
{
    void Pretend(const wchar_t *file)
    {
        std::lock_guard<std::mutex> hold(zsdk::exe::gate);
        zsdk::exe::pretended = file ? file : L"";
    }
}
#endif
