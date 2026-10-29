#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include "support/probe.h"

#include <string>

namespace
{
    using check::That;
    using probe::Settings;
    namespace ini = zsdk::ini;

    void RoundTrip()
    {
        Settings s;
        s.language = "en";
        s.radiusCm = 222.5;
        s.look = probe::Look::Toxic;
        s.alphaOn = false;
        s.next = zsdk::keys::Parse("ctrl+]");
        s.back = zsdk::keys::Parse("ALT+F9");
        ini::Kept keep;
        keep.session = "00AA11BB22CC33DD";
        keep.pending = "A.B";
        keep.disabled = { "C.D", "E.F" };
        keep.watch = { "W.1" };
        keep.proven["G.H"] = "61bc1e03";
        keep.owned["probe"]["exe"] = "61bc1e03";
        keep.owned["memory"]["last"] = "x";
        keep.unknown.push_back(ini::Entry{ "alpha", "future", "7" });
        keep.unknown.push_back(ini::Entry{ "safety", "odd", "1" });
        keep.unknown.push_back(ini::Entry{ "extra", "note", "kept" });
        const std::string text = ini::Text(s, keep);
        Settings back;
        ini::Kept kept;
        ini::Parse(text, back, &kept);
        That(back == s, "the settings survive a round trip");
        That(kept == keep, "the ledger, owned sections and unknown keys survive a round trip");
        That(ini::Text(back, kept) == text, "a second round trip is byte-identical");
        That(text.find("[general]\r\nlanguage=en\r\nenabled=1\r\nlog_level=1\r\n") == 0, "the head section comes first in visitor order");
        That(text.find("[alpha]\r\nradius_cm=222.5\r\nlook=toxic\r\nenabled=0\r\nfuture=7\r\n") != std::string::npos, "an unknown key stays at the end of its section");
        That(text.find("[keys]\r\nnext=CTRL+]\r\nback=ALT+F9\r\n") != std::string::npos, "keys are written by name");
        That(text.find("[safety]\r\nsession=00AA11BB22CC33DD\r\npending=A.B\r\ndisabled=C.D,E.F\r\nwatch=W.1\r\nodd=1\r\n") != std::string::npos, "the safety section starts with the session and keeps its unknown keys");
        That(text.find("[proven]\r\nG.H=61bc1e03\r\n") != std::string::npos && text.find("[probe]\r\nexe=61bc1e03\r\n") != std::string::npos &&
                 text.find("[memory]\r\nlast=x\r\n") != std::string::npos && text.find("[extra]\r\nnote=kept\r\n") != std::string::npos,
             "proven, owned and unknown sections are written in order");
        That(text.find("[probe]") < text.find("[memory]") && text.find("[memory]") < text.find("[extra]"), "owned sections come before unknown sections");
        const std::string fresh = ini::Text(Settings{}, ini::Kept{});
        That(fresh.find("[probe]\r\n\r\n[memory]\r\n") != std::string::npos, "owned sections are written even when empty");
    }

    void Words()
    {
        Settings s;
        ini::Parse("[general]\nlanguage=RU\nlog_level=9\nenabled=maybe\n[alpha]\nradius_cm=abc\nlook=purple\n[keys]\nnext=TAB\nback=none\n", s);
        That(s.language == "ru" && s.logLevel == 9 && s.enabled, "raw parse lowers words, keeps numbers and ignores broken flags");
        That(s.radiusCm == 150 && s.look == probe::Look::Count, "a broken number keeps its value and an unknown word parses to Count");
        That(s.back.key == 0, "none unbinds a key");
        const Settings c = ini::Sanitize(s);
        That(c.language == "auto" && c.logLevel == 2 && c.look == probe::Look::Cold && c.next.key == 0xBE, "sanitize clamps and falls back to defaults, ru becomes auto");
        Settings same;
        ini::Parse("[keys]\nnext=,\nback=,\n", same);
        That(ini::Sanitize(same).back.key == 0, "the cross-field fix runs after the field rules");
        Settings flat;
        ini::Parse("enabled=0\n[alpha]\nenabled=0\n", flat);
        That(!flat.enabled && !flat.alphaOn, "keys before any section belong to the head and a key belongs to its section");
        Settings bom;
        ini::Parse("\xEF\xBB\xBF[general]\nlog_level=0\n", bom);
        That(bom.logLevel == 0, "a UTF-8 BOM is skipped");
        That(ini::Real(0.10000) == "0.1" && ini::Real(-0.0) == "0" && ini::Real(3.0) == "3" && ini::Real(1.23456) == "1.2346", "reals are written with at most four decimals");
    }

    void Texts()
    {
        Settings s;
        ini::Parse("[names]\ncallsign=  Old Mixed Case  \nmark=MiXeD\n[general]\nlanguage=EN\n", s);
        That(s.callsign == "Old Mixed Case" && s.mark == "MiXeD", "a text field and a word without choices keep their case");
        That(s.language == "en", "a word with choices is still read in lower case");
        Settings longer;
        ini::Parse("[names]\ncallsign=" + std::string(40, 'x') + "\n", longer);
        That(longer.callsign == std::string(probe::kCallsignMost, 'x'), "a text field is cut to its most bytes on read");
        Settings cut;
        ini::Parse("[names]\ncallsign=" + std::string(23, 'x') + "\xD0\x96\xD0\x96\n", cut);
        That(cut.callsign == std::string(23, 'x'), "a cut never splits a UTF-8 letter");
        Settings odd;
        odd.callsign = "a\tb\x01" "c\r\nd";
        odd.mark = "Keep Me";
        const Settings clean = ini::Sanitize(odd);
        That(clean.callsign == "abcd" && clean.mark == "Keep Me", "sanitize drops control characters from a text field and keeps a free word as it is");
        const std::string text = ini::Text(odd);
        That(text.find("[names]\r\ncallsign=abcd\r\nmark=Keep Me\r\n") != std::string::npos, "the writer never puts a line break of a text field into the ini");
        Settings back;
        ini::Parse(text, back);
        That(back.callsign == "abcd" && back.mark == "Keep Me", "a text field survives a round trip with its case");
    }

    void Store()
    {
        const std::filesystem::path home = scan::EnvPath("ZSDK_TEST_HOME", std::filesystem::temp_directory_path());
        const std::filesystem::path folder = home / "store";
        std::error_code code;
        std::filesystem::remove_all(folder, code);
        That(ini::Open(folder), "the folder opens");
        That(std::filesystem::exists(folder / "probe.ini"), "the ini is created on open");
        Settings s = ini::Config<Settings>();
        s.radiusCm = 200;
        const uint64_t before = ini::Revision();
        ini::Apply(s);
        That(ini::Dirty() && ini::Revision() > before && ini::Config<Settings>().radiusCm == 200, "apply marks the settings dirty");
        ini::Apply(s);
        That(ini::Revision() == before + 1, "applying the same settings again changes nothing");
        That(ini::Save() && !ini::Dirty() && scan::Body(folder / "probe.ini").find("radius_cm=200") != std::string::npos, "save writes the ini");
        That(ini::Store("probe", { { "exe", "x" } }) && ini::Section("probe").at("exe") == "x", "a stored section is written at once");
        That(ini::Put("memory", "a", "1") && ini::Put("memory", "b", "2") && ini::Put("memory", "c", "3") && ini::Put("memory", "d", "4") && !ini::Put("memory", "e", "5"),
             "an owned section keeps its cap");
        That(ini::Put("memory", "a", "9") && ini::Section("memory").at("a") == "9", "an existing key can change at the cap");
        That(ini::Dirty() && scan::Body(folder / "probe.ini").find("a=9") == std::string::npos, "put waits for the data tick");
        ini::Close();
        That(scan::Body(folder / "probe.ini").find("[memory]\r\na=9\r\nb=2\r\nc=3\r\nd=4\r\n") != std::string::npos, "close saves the owned section");
        That(ini::Open(folder) && ini::Section("memory").size() == 4 && ini::Config<Settings>().radiusCm == 200, "the next open reads everything back");
        std::string raw;
        That(ini::ReadFile(folder / "probe.ini", raw) && raw == scan::Body(folder / "probe.ini"), "read file returns the bytes");
        That(ini::WriteAtomic(folder / "sub" / "x.txt", "abc") && scan::Body(folder / "sub" / "x.txt") == "abc", "atomic write creates folders");
        That(!std::filesystem::exists(folder / "sub" / "x.txt.tmp"), "no temp file stays behind");
        That(ini::Language() == "auto", "the language defaults to auto");
        Settings spoken = ini::Config<Settings>();
        spoken.language = "en";
        ini::Apply(spoken);
        That(ini::Language() == "en", "an explicit language is kept");
        spoken.language = "ru";
        ini::Apply(spoken);
        That(ini::Config<Settings>().language == "auto" && ini::Language() == "auto", "an applied ru becomes auto");
        That(ini::WriteAtomic(folder / "probe.ini", "[general]\r\nlanguage=ru\r\n") && ini::Open(folder) && ini::Language() == "auto", "a stored ru becomes auto at boot");
        That(ini::WriteAtomic(folder / "probe.ini", "[general]\r\nlanguage=xx\r\n") && ini::Open(folder) && ini::Language() == "auto", "a stored unknown code becomes auto at boot");
    }
}

int main()
{
    RoundTrip();
    Words();
    Texts();
    Store();
    return check::Finish("ini");
}
