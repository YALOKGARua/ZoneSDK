#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{
    using check::That;
    namespace guard = zsdk::guard;

    constexpr guard::Guard kList[] = {
        { 0x68F67DB, "48 89 9F 40 03 00 00 48 8B 43 08 48 89 87 48 03" },
        { 0x68F6D5C, "48 8B 86 40 03 00 00 48 85 C0 74 54 8B 50 10 4C" },
        { 0x68643A1, "48 8B 90 50 06 00 00 4C 8D 44 24 5C 48 89 F9 0F" },
        { 0x0C77CD1, "8B 05 61 A0 66 09" },
    };
    constexpr guard::Set kSet{ "probe", &guard::k61bc1e03, kList, 4, guard::Need::ImageSize };
    constexpr guard::Set kKnown{ "probe.known", &guard::k61bc1e03, kList, 4, guard::Need::KnownBuild };
    constexpr guard::Field kThrower{ "Grenade.Thrower", 0x340, kSet, 0, 3 };
    constexpr guard::Field kUid{ "Record.Uid", 0x10, kSet, 1, 14 };
    constexpr guard::Field kRecord{ "Pawn.Record", 0x650, kSet, 2, 3 };
    constexpr guard::Field kItems = guard::Field::Rip("Items.Root", 0xA2E1D38u, kSet, 3, 2, 6);
    constexpr guard::Field kLoose = guard::Field::Unproven("Loose.Offset", 0x148);
    constexpr guard::Field kReflected{ "Pawn.Root", 0x1A8, "Actor", "RootComponent" };
    constexpr guard::Field kWaiting{ "Probe.Waiting", 0x340, kKnown, 0, 3 };
    constexpr guard::Guard kHeadList[] = { { 0x0, "4D 5A" } };
    constexpr guard::Set kHead{ "probe.head", &guard::k61bc1e03, kHeadList, 1, guard::Need::Bytes };
    constexpr guard::Set kLate{ "probe.late", &guard::k61bc1e03, kHeadList, 1, guard::Need::Bytes };
    constexpr guard::Field kMagic{ "Head.Magic", 0x5A, kHead, 0, 1 };

    static_assert(kThrower.at == 0x340 && kThrower.proof == guard::Proof::Guard && kThrower.guard == 0 && kThrower.offset == 3);
    static_assert(kUid.at == 0x10 && kRecord.at == 0x650);
    constexpr guard::Field kProtos = guard::Field::Rip("Protos.Root", 0xA2E1D30u, kSet, 3, 2, 6, 8);
    static_assert(kItems.proof == guard::Proof::Rip && kItems.at == 0xA2E1D38u && kProtos.at == 0xA2E1D30u);
    static_assert(kLoose.proof == guard::Proof::Unproven && kReflected.proof == guard::Proof::Reflected);
    static_assert(kThrower.Raw() + 8 == 0x348);
    static_assert(guard::detail::ByteAt("48 ?? 9F", 1) == -1 && guard::detail::ByteAt("48 ?? 9F", 2) == 0x9F && guard::detail::ByteAt("48", 3) == -2);
}

int main()
{
    std::vector<int> bytes;
    That(guard::Hex("48 8B ?? 0F", bytes) && bytes.size() == 4 && bytes[2] == -1 && bytes[3] == 0x0F, "hex with wildcards");
    That(!guard::Hex("48 8", bytes) && !guard::Hex("ZZ", bytes) && !guard::Hex(nullptr, bytes), "broken hex is refused");
    const uint8_t code[] = { 0x48, 0x8B, 0x90, 0x50, 0x06, 0x00, 0x00, 0x4C };
    That(guard::Matches(reinterpret_cast<uintptr_t>(code), "48 8B 90 50 06 00 00"), "bytes match in memory");
    That(guard::Matches(reinterpret_cast<uintptr_t>(code), "48 ?? 90"), "wildcards match anything");
    That(!guard::Matches(reinterpret_cast<uintptr_t>(code), "48 8B 91"), "a different byte fails");
    That(!guard::Matches(0x10, "48"), "an unreadable address fails without a crash");
    const uint8_t rip[] = { 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00 };
    const uintptr_t at = reinterpret_cast<uintptr_t>(rip);
    That(guard::Target(at, 2, 6) == at + 6 + 0x10, "RIP-relative targets");
    const guard::Report r = guard::Check(kSet);
    That(r.total == 4 && r.ok == 0 && r.state == guard::State::Off && guard::Of(kSet) == guard::State::Off, "a set that does not match this image is off");
    That(r.bad == "0x68F67DB 0x68F6D5C 0x68643A1 0xC77CD1", "the failed RVAs are listed");
    That(!guard::Usable(kThrower) && !guard::Usable(kLoose), "fields of an off set and unproven fields are unusable");
    That(!guard::Usable(kReflected), "a reflected field is not usable before its reflection check passed");
    uintptr_t slot = 1;
    That(!guard::Use(kThrower, slot) && slot == 0, "using a field of an off set is refused");
    That(!guard::Use(kLoose, slot) && slot == 0, "using an unproven field is refused");
    That(!guard::Use(kReflected, slot) && slot == 0 && !guard::Confirm(kReflected) && !guard::Usable(kReflected), "a reflected field without a bound engine stays unusable");
    uint64_t value = 0;
    That(!zsdk::mem::Read(reinterpret_cast<uintptr_t>(code), kThrower, value), "a read through a refused field does not happen");
    That(!zsdk::mem::Read(0x10, kRecord, value), "a read below the first page is refused");
    That(guard::Recheck() == 0, "no set waits for the exe hash in this host");
    That(guard::Find(guard::k61bc1e03.sha256) == &guard::k61bc1e03 && guard::Find("00") == nullptr, "known builds are found by sha");
    That(std::strlen(guard::k61bc1e03.sha256) == 64 && guard::k61bc1e03.imageSize == 0xB963000u && guard::k61bc1e03.stamp == 0xCD15CBEEu, "the 61bc1e03 facts");
    That(guard::Find(guard::k2bd3f115.sha256) == &guard::k2bd3f115 && std::strlen(guard::k2bd3f115.sha256) == 64 && guard::k2bd3f115.imageSize == 0xB954000u && guard::k2bd3f115.stamp == 0x2B1AEEBFu &&
             guard::k2bd3f115.objectsRoot == 0xA0D4990u && guard::k2bd3f115.objectsCount == guard::k2bd3f115.objectsRoot + 0x14,
         "the 2bd3f115 facts");
    That(std::size(guard::kBuilds) == 2 && guard::kBuilds[0] == &guard::k61bc1e03 && guard::kBuilds[1] == &guard::k2bd3f115, "the known builds are 61bc1e03 and 2bd3f115, the old build stays known");
    That(zsdk::exe::ImageSize() > 0 && zsdk::exe::Base() != 0 && zsdk::exe::Stamp() != 0, "the host image is readable");
    That(zsdk::exe::Short() == "?" && !zsdk::exe::Known(), "the build is unknown until the hash is computed");
    That(guard::Check(kKnown).state == guard::State::Pending && guard::Of(kKnown) == guard::State::Pending, "a known-build set waits for the hash");
    That(!guard::Ready(kKnown), "a waiting set is not ready");
    That(guard::Of(kHead) == guard::State::Pending && !guard::Ready(kHead), "an unseen set before the hash is pending");
    const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
    const std::filesystem::path logFile = home / "guard.log";
    zsdk::log::Open(logFile, home / "guard.1.log");
    zsdk::log::SetLevel(2);
    That(!guard::Use(kWaiting, slot) && slot == 0, "a field of a pending set is refused");
    zsdk::Flush();
    const std::string waiting = scan::Body(logFile);
    That(waiting.find("guard: field Probe.Waiting waits, guard verdict pending for set probe.known") != std::string::npos, "a pending verdict is a note, " + waiting);
    That(waiting.find("Probe.Waiting is not usable on this build") == std::string::npos, "a pending verdict does not look like a patch-day failure");
    That(waiting.find("guard: set probe.head is not in App::sets") != std::string::npos, "a set that is not listed in App::sets is named");
    zsdk::log::SetLevel(1);
    const std::string sha = zsdk::exe::Hash();
    That(guard::Of(kKnown) == guard::State::Off && guard::Ready(kKnown), "the waiting set is checked again once the hash is known");
    That(guard::Of(kLate) == guard::State::Ok && guard::Ready(kLate), "an unseen set with the hash known is checked at once, without a pump");
    uintptr_t magic = 0;
    That(guard::Use(kMagic, magic) && magic == 0x5A, "a field of an unseen set is usable at once after the hash");
    That(!guard::Use(kWaiting, slot), "the field of the set that is now off is refused");
    zsdk::Flush();
    That(scan::Body(logFile).find("guard: field Probe.Waiting is not usable on this build (guard set is not on)") != std::string::npos, "a set that is really off keeps the patch-day text");
    That(sha.size() == 64 && zsdk::exe::Hashed() && zsdk::exe::Short().size() == 8, "the host exe hashes");
    That(!zsdk::exe::Known(), "the test host is not a known game build");
    return check::Finish("guard");
}
