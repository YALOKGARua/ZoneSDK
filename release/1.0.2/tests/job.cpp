#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <windows.h>

#include "../src/detail/runtime.h"
#include "support/probe.h"

#include <atomic>
#include <stdexcept>
#include <string>

namespace
{
    using check::That;

    std::atomic<int> ticks{ 0 };
    std::atomic<uint64_t> tickedAt{ 0 };
    std::atomic<uint64_t> closest{ ~0ull };
    std::atomic<int> faults{ 0 };
    std::atomic<int> throws{ 0 };
    std::atomic<int> fires{ 0 };
    std::atomic<int> refusals{ 0 };
    std::atomic<int> slows{ 0 };
    std::atomic<int> laters{ 0 };
    std::atomic<bool> linger{ false };
    std::atomic<bool> lingering{ false };
    std::atomic<uint64_t> lingered{ 0 };
    int *volatile nowhere = nullptr;

    void Tick(uint64_t now)
    {
        const uint64_t before = tickedAt.exchange(now);
        if (before && zsdk::Age(now, before) < closest.load()) closest = zsdk::Age(now, before);
        ++ticks;
    }

    void Fault(uint64_t)
    {
        ++faults;
        *nowhere = 1;
    }

    void Throw(uint64_t)
    {
        ++throws;
        throw std::runtime_error("job suite throw");
    }

    void Fire(uint64_t)
    {
        ++fires;
        try
        {
            zsdk::Call c(probe::kSpecs, 0);
            zsdk::Fire(c);
        }
        catch (const zsdk::Refused &)
        {
            ++refusals;
        }
    }

    void Slow(uint64_t)
    {
        if (slows++ == 0) Sleep(80);
    }

    void Later(uint64_t)
    {
        ++laters;
    }

    void Linger(uint64_t)
    {
        if (!linger.load()) return;
        lingering = true;
        for (int waited = 0; waited < 5000 && !zsdk::job::Stopping(); waited += 5) Sleep(5);
        lingered = GetTickCount64();
    }

    void Never(uint64_t)
    {
    }

    template<class F> bool Within(int ms, F &&done)
    {
        for (int waited = 0; waited < ms; waited += 20)
        {
            if (done()) return true;
            Sleep(20);
        }
        return done();
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path folder = home / "jobs";
    std::error_code code;
    std::filesystem::remove_all(folder, code);
    std::filesystem::create_directories(folder, code);
    That(zsdk::ini::WriteAtomic(folder / "probe.ini", "[safety]\r\nsession=00AA11BB22CC33DD\r\ndisabled=job.later\r\n") && zsdk::ini::Open(folder), "an add-on data folder where job.later was disabled after a crash");
    That(!zsdk::job::Every("fast", 50, &Never), "a period below 100 ms is refused");
    That(!zsdk::job::Every("bad name", 200, &Never) && !zsdk::job::Every("a,b", 200, &Never) && !zsdk::job::Every("abcdefghijabcdefghijabcdefghijabc", 200, &Never), "a name with a space, a comma or over 32 signs is refused");
    That(!zsdk::job::Every("", 200, &Never) && !zsdk::job::Every(nullptr, 200, &Never), "an empty name is refused");
    That(!zsdk::job::Every("nofunction", 200, nullptr), "a job without a function is refused");
    That(zsdk::job::Every("tick", 100, &Tick), "a 100 ms job is registered");
    That(!zsdk::job::Every("tick", 300, &Never), "a taken name is refused");
    That(zsdk::job::Every("fault", 100, &Fault) && zsdk::job::Every("throw", 150, &Throw) && zsdk::job::Every("fire", 200, &Fire) && zsdk::job::Every("slow", 100, &Slow) &&
             zsdk::job::Every("later", 100, &Later) && zsdk::job::Every("linger", 100, &Linger) && zsdk::job::Every("spare.job-1", 1000, &Never),
         "eight jobs are registered");
    That(!zsdk::job::Every("ninth", 100, &Never), "an add-on has at most 8 jobs");
    That(!zsdk::job::Off("tick") && !zsdk::job::Off("ninth") && zsdk::job::Off("later"), "Off tells a registered job, a job disabled after a crash, and an unknown name apart");
    zsdk::detail::Pretend(zsdk::exe::kSteamFile);
    zsdk::detail::thread::Start();
    That(!zsdk::job::Every("late", 100, &Never), "once the SDK worker runs no job is registered");
    That(Within(5000, [] { return zsdk::thread::Rejected(); }), "the test exe has no engine, its native bindings are rejected");
    const int before = ticks.load();
    Sleep(1200);
    That(ticks.load() >= before + 6, "jobs keep running after native bindings were rejected, " + std::to_string(ticks.load() - before) + " ticks in 1.2 s");
    That(closest.load() >= 100 && closest.load() < 1000, "two runs of a 100 ms job are at least 100 ms apart, closest " + std::to_string(closest.load()) + " ms");
    That(faults.load() == 1 && zsdk::job::Off("fault") && zsdk::ledger::Blocked("job.fault"), "an access violation switches that job off and disables job.fault");
    That(!zsdk::job::Off("tick") && !zsdk::job::Off("throw") && !zsdk::job::Off("fire"), "an access violation in one job leaves the others on");
    That(throws.load() >= 4 && !zsdk::job::Off("throw"), "a C++ exception is logged and the job keeps its period");
    That(fires.load() >= 3 && refusals.load() == fires.load(), "a game call from a job is refused off the game thread pump: " + std::to_string(refusals.load()) + " of " + std::to_string(fires.load()) + " fires refused");
    That(slows.load() >= 5 && !zsdk::job::Off("slow"), "a slow run is logged and the job stays on");
    That(laters.load() == 0 && zsdk::job::Off("later"), "a job disabled after a crash in an earlier session does not run");
    zsdk::ledger::Retry();
    That(Within(1000, [] { return laters.load() > 0; }) && !zsdk::job::Off("later"), "Try again lets the disabled job run");
    That(faults.load() == 1 && zsdk::job::Off("fault"), "the job that faulted in this session stays off after Try again");
    zsdk::Flush();
    const std::string log = scan::Body(folder / "probe.log");
    That(log.find("job tick every 100 ms") != std::string::npos && log.find("job fast refused: the period is below 100 ms") != std::string::npos && log.find("job late refused: jobs are registered from hooks.boot") != std::string::npos,
         "registrations and refusals are logged");
    That(log.find("job fault: structured exception 0xC0000005") != std::string::npos && log.find("job.fault is disabled until Try again") != std::string::npos, "the access violation is logged once with its code");
    That(log.find("job throw error: job suite throw") != std::string::npos, "the C++ exception is logged with its text");
    That(log.find("job slow ran") != std::string::npos, "a run longer than 50 ms is logged");
    That(log.find("job later is disabled after a crash") != std::string::npos, "the skipped job says why");
    zsdk::detail::Exit(true);
    That(zsdk::job::Stopping(), "the process-exit flag is a stop for jobs");
    Sleep(150);
    const int frozen = ticks.load();
    Sleep(400);
    That(ticks.load() == frozen, "while the exit flag is set no job runs");
    zsdk::detail::Exit(false);
    That(!zsdk::job::Stopping() && Within(1000, [frozen] { return ticks.load() > frozen; }), "without the flag the jobs run again");
    linger = true;
    That(Within(2000, [] { return lingering.load(); }) && lingered.load() == 0, "a long job is running");
    zsdk::detail::job::Stuck(GetTickCount64());
    Sleep(1100);
    zsdk::detail::job::Stuck(GetTickCount64());
    zsdk::detail::job::Stuck(GetTickCount64());
    zsdk::Flush();
    const std::string stuck = scan::Body(folder / "probe.log");
    const size_t told = stuck.find("job linger has not returned for ");
    That(told != std::string::npos && stuck.find("job linger has not returned for ", told + 1) == std::string::npos && lingered.load() == 0,
         "a job that has not returned after 1 s is reported once from the present thread, while it still runs");
    const uint64_t stopAt = GetTickCount64();
    zsdk::detail::thread::Stop();
    That(zsdk::job::Stopping(), "Stop is a stop for jobs");
    That(Within(1000, [] { return lingered.load() != 0; }) && zsdk::Age(lingered.load(), stopAt) < 300, "a long job that polls job::Stopping returns soon after Stop");
    const int stopped = ticks.load();
    const uint64_t turns = zsdk::detail::thread::Turns();
    Sleep(400);
    That(ticks.load() == stopped && zsdk::detail::thread::Turns() == turns, "after Stop no job runs and the worker loop is gone");
    zsdk::ini::Close();
    return check::Finish("job");
}
