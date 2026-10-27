#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "fieldwatch.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using check::That;
    using zfieldwatch::Build;
    using zfieldwatch::Reading;
    using zfieldwatch::Verdict;
    namespace guard = zsdk::guard;
    namespace ledger = zsdk::ledger;
    namespace fields = zfieldwatch::fields;
    namespace watch = zfieldwatch::watch;

    constexpr guard::Guard kHeadList[] = { { .rva = 0, .bytes = "4D 5A" } };
    constexpr guard::Set kHead{ "fieldwatch.test.head", &guard::k61bc1e03, kHeadList, 1, guard::Need::Bytes };
    constexpr guard::Field kStub{ "Test.Stub", 0x5A, kHead, 0, 1 };
    constexpr const char *kStubKey = "FieldWatch.Test";
    constexpr const char *kCrashKey = "FieldWatch.TestCrash";

    size_t Count(const std::string &body, const std::string &what)
    {
        size_t n = 0;
        for (size_t at = body.find(what); at != std::string::npos; at = body.find(what, at + what.size())) ++n;
        return n;
    }

    std::string Log(const std::filesystem::path &folder)
    {
        zsdk::Flush();
        return scan::Body(folder / "fieldwatch.log");
    }

    void Proofs()
    {
        static_assert(fields::kDefaultFov.at == 0x2C8 && fields::kLockedFov61bc1e03.at == 0x2CC && fields::kLockedFov2bd3f115.at == 0x2CC && fields::kViewFov.at == 0x17F0);
        static_assert(fields::kLockedFov61bc1e03.guard == 0 && fields::kLockedFov61bc1e03.offset == 8 && fields::kLockedFov2bd3f115.guard == 0 && fields::kLockedFov2bd3f115.offset == 8);
        That(std::string(fields::kDefaultFov.reflectedClass) == "PlayerCameraManager" && std::string(fields::kDefaultFov.reflectedProperty) == "DefaultFOV", "the reflected field names its class and property");
        That(std::size(fields::kSets) == 2 && fields::kSets[0] == &fields::kCamera61bc1e03 && fields::kSets[1] == &fields::kCamera2bd3f115, "one camera set per game build, 61bc1e03 and 2bd3f115");
        for (size_t i = 0; i < std::size(fields::kSets); ++i)
        {
            const guard::Set &set = *fields::kSets[i];
            const std::string name = set.name;
            std::vector<int> bytes;
            That(set.count == 1 && guard::Hex(set.guards[0].bytes, bytes) && bytes.size() == 54, "the guard bytes of " + name + " parse, 54 bytes");
            That(bytes.size() == 54 && bytes[8] == 0xCC && bytes[9] == 0x02 && bytes[10] == 0 && bytes[11] == 0, "the guard of " + name + " holds the LockedFOV offset at byte 8, the movss displacement");
            That(bytes.size() == 54 && bytes[33] == -1 && bytes[36] == -1 && bytes[32] == 0x15 && bytes[37] == 0x48, "the guard of " + name + " masks the four bytes of the lea displacement");
            That(set.need == guard::Need::KnownBuild && name == std::string(fields::kCameraName) + "." + set.build->id, "the set " + name + " needs its exact build and is named after it");
            That(fields::kLockedFovs[i]->set == &set && std::string(fields::kLockedFovs[i]->name) == "Camera.LockedFOV", "the LockedFOV field of " + name + " is proved by that set");
        }
        That(fields::kCameraGuards61bc1e03[0].rva == 0x23CA0E4 && fields::kCameraGuards2bd3f115[0].rva == 0x22B5CDE, "GetFOVAngle is at 0x23CA0E4 in build 61bc1e03 and at 0x22B5CDE in build 2bd3f115");
        That(std::string(fields::kCameraGuards61bc1e03[0].bytes) == fields::kCameraGuards2bd3f115[0].bytes, "the guarded code is the same on both builds, only its address moved");
    }

    void Unknown(const std::filesystem::path &folder)
    {
        const uintptr_t base = zsdk::exe::Base();
        const void *object = reinterpret_cast<const void *>(base);
        That(!zsdk::exe::Hashed() && watch::Look().build == Build::Unchecked, "before the exe hash the build is not checked");
        That(guard::Of(fields::kCamera61bc1e03) == guard::State::Pending && guard::Of(fields::kCamera2bd3f115) == guard::State::Pending, "both known-build sets wait for the exe hash");
        That(&watch::Locked() == &fields::kLockedFov61bc1e03 && watch::Gate(watch::Locked()) == Verdict::Pending, "before the hash no set is on, the first LockedFOV field stands in and waits for its check");
        That(!watch::Look().camera && watch::Look().set == guard::State::Pending, "before the hash no set is picked and the board shows the sets as waiting");
        That(watch::Read(object, watch::Locked(), zfieldwatch::kLockedKey).verdict == Verdict::Pending && ledger::Watched().empty(), "a pending field opens no crash window");
        That(watch::Gate(fields::kDefaultFov) == Verdict::Pending && watch::Read(object, fields::kDefaultFov, zfieldwatch::kDefaultKey).verdict == Verdict::Pending, "a reflected field waits for its reflection check");
        That(watch::Gate(fields::kViewFov) == Verdict::Unproven && watch::Read(object, fields::kViewFov, nullptr).verdict == Verdict::Unproven, "an unproven field is never read");
        That(ledger::Watched().empty() && ledger::Proven(zfieldwatch::kDefaultKey).empty(), "no crash window was opened for a field that is not usable");
        const std::string sha = zsdk::exe::Hash();
        const zfieldwatch::Board b = watch::Look();
        That(sha.size() == 64 && b.build == Build::Other && b.id == zsdk::exe::Short(), "after the hash the test exe is a build this ZoneSDK does not know");
        That(!b.camera && b.set == guard::State::Off && b.locked == Verdict::Off, "on an unknown build no camera set is picked, the sets are off and so is the field");
        That(guard::Of(fields::kCamera61bc1e03) == guard::State::Off && guard::Of(fields::kCamera2bd3f115) == guard::State::Off && &watch::Locked() == &fields::kLockedFov61bc1e03,
             "both known-build sets are off on the unknown build");
        float value = 1.5f;
        That(!zsdk::mem::Read(base, fields::kLockedFov61bc1e03, value) && value == 1.5f && !zsdk::mem::Read(base, fields::kLockedFov2bd3f115, value) && value == 1.5f,
             "mem::Read refuses the byte-guarded fields of both builds and leaves the output alone");
        That(!zsdk::mem::Read(base, fields::kViewFov, value) && value == 1.5f, "mem::Read refuses the unproven field");
        That(!zsdk::mem::Read(base, fields::kDefaultFov, value) && value == 1.5f, "mem::Read refuses the reflected field without a bound engine");
        That(watch::Read(object, watch::Locked(), zfieldwatch::kLockedKey).verdict == Verdict::Off && ledger::Watched().empty(), "an off field opens no crash window");
        zfieldwatch::core::Judge(0);
        zfieldwatch::core::Judge(500);
        const std::string log = Log(folder);
        const std::string line = "fieldwatch: game build " + zsdk::exe::Short() + " is not known to this ZoneSDK; guard set fieldwatch.camera off; Camera.DefaultFOV waits for its check, Camera.LockedFOV off, Camera.ViewFOV unproven, never read";
        That(Count(log, line) == 1, "the verdict job logs the unknown build once, " + log.substr(log.size() > 600 ? log.size() - 600 : 0));
        That(log.find("guard: field Camera.LockedFOV is not usable on this build (guard set is not on)") != std::string::npos, "the SDK names the refused byte-guarded read");
        watch::Checked(true);
        That(watch::Gate(fields::kDefaultFov) == Verdict::Off, "a reflected field whose check ran and did not pass is off");
        zfieldwatch::core::Judge(1000);
        That(Count(Log(folder), "Camera.DefaultFOV off, Camera.LockedFOV off") == 1, "a changed verdict is logged again");
        watch::Checked(false);
    }

    void Windows(const std::filesystem::path &folder)
    {
        const uintptr_t base = zsdk::exe::Base();
        const void *object = reinterpret_cast<const void *>(base);
        float want = 0.0f;
        std::memcpy(&want, reinterpret_cast<const void *>(base + kStub.at), sizeof(want));
        That(std::isfinite(want) && watch::Gate(kStub) == Verdict::Read, "the test field over the image header is usable after the hash");
        const guard::Field *later[] = { &fields::kLockedFov61bc1e03, &fields::kLockedFov2bd3f115, &kStub };
        const guard::Field *none[] = { &fields::kLockedFov2bd3f115, &fields::kLockedFov61bc1e03 };
        That(&watch::On(later) == &kStub, "the field whose set is on is used, wherever it stands in the list");
        That(&watch::On(none) == &fields::kLockedFov2bd3f115 && &watch::Locked() == &fields::kLockedFov61bc1e03, "with no set on, the first field of the list stands in and stays off");
        const Reading first = watch::Read(object, kStub, kStubKey);
        That(first.verdict == Verdict::Read && std::memcmp(&first.value, &want, sizeof(want)) == 0, "the read inside the crash window returns the bytes of the field");
        That(ledger::Watched().empty() && ledger::Proven(kStubKey) == zsdk::exe::Short(), "the window closed and proved its key for this exe");
        That(watch::Live(kStub, kStubKey, first).proven, "the tab sees the key as proven on this build");
        That(watch::Read(object, kStub, kStubKey).verdict == Verdict::Read, "the next read works");
        const std::string log = Log(folder);
        That(Count(log, "safety: watching FieldWatch.Test\n") + Count(log, "safety: watching FieldWatch.Test\r\n") == 1, "the crash marker is written once, around the first read on this exe");
        That(scan::Body(folder / "fieldwatch.ini").find("FieldWatch.Test=" + zsdk::exe::Short()) != std::string::npos, "the proof is kept in the ini");
        That(watch::Read(nullptr, kStub, kCrashKey).verdict == Verdict::Waiting && ledger::Watched().empty(), "without an object there is no window and no read");
        That(watch::Read(object, kStub, nullptr).verdict == Verdict::Unmarked, "a read without a crash marker key is refused");
        That(ledger::Watch(kCrashKey), "a crash window is open, as when the game dies inside a read");
        That(zsdk::ini::Open(folder), "the next boot");
        const std::vector<std::string> booted = ledger::Booted();
        That(ledger::Blocked(kCrashKey) && std::find(booted.begin(), booted.end(), kCrashKey) != booted.end(), "the window that was open is disabled at boot and reported");
        That(watch::Read(object, kStub, kCrashKey).verdict == Verdict::Crashed && ledger::Watched().empty(), "a disabled key is not read and opens no window");
        That(watch::Live(kStub, kCrashKey, Reading{ .verdict = Verdict::Read, .value = 1.0f }).reading.verdict == Verdict::Crashed, "the tab shows the key as disabled after a crash");
        That(!ledger::Blocked(kStubKey) && watch::Read(object, kStub, kStubKey).verdict == Verdict::Read, "the crash switches off only its own key");
        ledger::Retry();
        That(!ledger::Blocked(kCrashKey) && watch::Read(object, kStub, kCrashKey).verdict == Verdict::Read, "Try again clears the key and the read comes back");
        That(ledger::Proven(kCrashKey) == zsdk::exe::Short(), "the read after Try again proves the key");
        for (int i = 0; i < 8; ++i) ledger::Watch("Fill." + std::to_string(i));
        That(watch::Read(object, kStub, "FieldWatch.NoRoom").verdict == Verdict::Unmarked && !ledger::Blocked("FieldWatch.NoRoom"), "when no crash marker can be written the read is skipped");
        That(ledger::Calm() && ledger::Watched().empty(), "calm");
    }
}

int main()
{
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path folder = home / "watch";
    std::error_code code;
    std::filesystem::remove_all(folder, code);
    That(zsdk::ini::Open(folder), "the data folder opens");
    Proofs();
    Unknown(folder);
    Windows(folder);
    return check::Finish("watch");
}
