#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include <cmath>
#include <limits>

namespace
{
    using check::That;
    namespace m = zsdk::math;

    bool Near(double a, double b, double e)
    {
        return std::fabs(a - b) <= e;
    }

    bool Near(const m::Vec3 &a, const m::Vec3 &b, double e)
    {
        return Near(a.x, b.x, e) && Near(a.y, b.y, e) && Near(a.z, b.z, e);
    }
}

int main()
{
    That(m::Wrap(190.0) == -170.0 && m::Wrap(-180.0) == 180.0 && m::Wrap(std::numeric_limits<double>::quiet_NaN()) == 0.0, "wrap");
    That(Near(m::Forward(90.0), m::Vec3{ 0, 1, 0 }, 1e-12) && Near(m::Left(90.0), m::Vec3{ 1, 0, 0 }, 1e-12) && Near(m::Right(0.0), m::Vec3{ 0, 1, 0 }, 1e-12), "headings");
    for (double h = -170; h <= 180; h += 37)
        for (double p = -60; p <= 60; p += 40)
            for (double r = -45; r <= 45; r += 30)
            {
                m::Vec3 x, y, z;
                m::Axes(m::Rot3{ p, h, r }, x, y, z);
                That(Near(m::Length(x), 1, 1e-9) && Near(m::Dot(x, y), 0, 1e-9) && Near(m::Cross(x, y), z, 1e-9), "axes are a right-handed orthonormal set");
                const m::Rot3 back = m::FromAxes(x, y, z);
                m::Vec3 bx, by, bz;
                m::Axes(back, bx, by, bz);
                That(Near(bx, x, 1e-9) && Near(by, y, 1e-9) && Near(bz, z, 1e-9), "from axes inverts axes");
            }
    const m::Rot3 bent = m::Attitude(33.0, 7.0, -4.0);
    const m::Vec3 origin{ 10, 20, 30 };
    That(Near(m::Local(origin, bent, m::Place(origin, bent, m::Vec3{ 120, -40, 60 })), m::Vec3{ 120, -40, 60 }, 1e-9), "local inverts place");
    That(Near(m::Unit(m::Vec3{ 0, 0, 0 }), m::Vec3{ 0, 0, 1 }, 0) && Near(m::Flat(m::Vec3{ 0, 0, 0 }, m::Vec3{ 3, 4, 9 }), 5, 1e-12), "unit and flat");
    That(Near(m::MeshHalf(1340.5, 504.5, 330.5, 90), m::Vec3{ 504.5, 1340.5, 330.5 }, 1e-12), "mesh half");
    static_assert(zsdk::Age(5000, 2000) == 3000 && zsdk::Age(2000, 2000) == 0, "Age is compile-time");
    That(zsdk::Age(5000, 2000) == 3000, "age of a past tick");
    That(zsdk::Age(2000, 2000) == 0, "age of the same tick");
    That(zsdk::Age(2000, 2001) == 0, "a stamp taken after now has age 0, not 2^64 - 1");
    That(zsdk::Age(0, std::numeric_limits<uint64_t>::max()) == 0, "the widest future stamp has age 0");
    That(zsdk::Age(std::numeric_limits<uint64_t>::max(), 0) == std::numeric_limits<uint64_t>::max(), "the oldest stamp keeps the full age");
    That(zsdk::Age(1000, 0) >= 1000 && !(zsdk::Age(100, 400) >= 3000), "a settle check over Age does not pass early on a later stamp");
    return check::Finish("math");
}
