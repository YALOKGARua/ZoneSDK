#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include "support/probe.h"

#include <string>

namespace
{
    using check::That;
    namespace verbs = zsdk::verbs;

    zsdk::ue::Sign Sign(std::initializer_list<zsdk::ue::Param> params, int32_t bytes)
    {
        zsdk::ue::Sign s;
        s.owner = "GameplayStatics";
        s.bytes = bytes;
        s.params = params;
        return s;
    }

    zsdk::ue::Param P(const char *name, const char *type, int32_t at, int32_t size)
    {
        zsdk::ue::Param p;
        p.name = name;
        p.type = type;
        p.at = at;
        p.size = size;
        return p;
    }
}

int main()
{
    const verbs::Spec &controller = probe::kSpecs[0];
    const auto good = Sign({ P("WorldContextObject", "ObjectProperty", 0, 8), P("PlayerIndex", "IntProperty", 8, 4), P("ReturnValue", "ObjectProperty", 0x10, 8) }, 24);
    std::string note, spelled;
    That(verbs::Verify(good, false, true, controller, false, note, spelled) == verbs::Check::Ok, "a matching signature passes");
    That(note == " WorldContextObject:ObjectProperty@0/8 PlayerIndex:IntProperty@8/4 ReturnValue:ObjectProperty@16/8", "the note lists the parameters");
    That(verbs::Verify(good, true, false, controller, false, note, spelled) == verbs::Check::Bad && note == "missing", "a missing function");
    That(verbs::Verify(good, true, true, controller, false, note, spelled) == verbs::Check::Bad && note == "rejected", "a rejected function");
    const auto moved = Sign({ P("WorldContextObject", "ObjectProperty", 0, 8), P("PlayerIndex", "IntProperty", 12, 4), P("ReturnValue", "ObjectProperty", 0x10, 8) }, 24);
    That(verbs::Verify(moved, false, true, controller, false, note, spelled) == verbs::Check::Bad && note.find("parameter PlayerIndex wants IntProperty@8/4") == 0, "a moved parameter fails");
    const auto spelledSign = Sign({ P("WorldContextObject", "ObjectProperty", 0, 8), P("playerIndex", "IntProperty", 8, 4), P("ReturnValue", "ObjectProperty", 0x10, 8) }, 24);
    That(verbs::Verify(spelledSign, false, true, controller, false, note, spelled) == verbs::Check::Bad && note.find("no parameter PlayerIndex") == 0, "strict spelling refuses a case difference");
    That(verbs::Verify(spelledSign, false, true, controller, true, note, spelled) == verbs::Check::Ok && spelled == "parameter PlayerIndex is spelled playerIndex here",
         "tolerant spelling accepts it and says so");
    const verbs::Spec &location = probe::kSpecs[5];
    const auto one = Sign({ P("ReturnValue", "StructProperty", 0, 24) }, 24);
    That(verbs::Verify(one, false, true, location, false, note, spelled) == verbs::Check::Ok, "exact count and block size pass");
    const auto two = Sign({ P("ReturnValue", "StructProperty", 0, 24), P("Extra", "IntProperty", 24, 4) }, 32);
    That(verbs::Verify(two, false, true, location, false, note, spelled) == verbs::Check::Bad && note.find("2 parameters, wants 1") == 0, "an exact count refuses extra parameters");
    const auto wide = Sign({ P("ReturnValue", "StructProperty", 0, 24) }, 32);
    That(verbs::Verify(wide, false, true, location, false, note, spelled) == verbs::Check::Bad && note == "parameter block 32 bytes, wants 24", "the block size is checked");
    That(verbs::TypeFits("FloatProperty", "DoubleProperty|FloatProperty") && !verbs::TypeFits("IntProperty", "DoubleProperty|FloatProperty") && verbs::TypeFits("X", nullptr),
         "type lists");
    That(verbs::KeyOf(controller) == "GameplayStatics.GetPlayerController", "keys are Owner.Name");
    That(!verbs::Ready(0) && !verbs::Ready(-1) && !verbs::Ready(99) && verbs::Note(99).empty(), "nothing is ready before prime");
    static_assert(verbs::Same("abc", "abc") && !verbs::Same("abc", "abd"));
    return check::Finish("verbs");
}
