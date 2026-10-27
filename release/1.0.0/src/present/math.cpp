#include <zsdk/present.h>

#include <cmath>

namespace zsdk::math
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kRad = kPi / 180.0;

        Vec3 Blend(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &k)
        {
            return Vec3{ a.x * k.x + b.x * k.y + c.x * k.z, a.y * k.x + b.y * k.y + c.y * k.z, a.z * k.x + b.z * k.y + c.z * k.z };
        }
    }

    double Wrap(double yaw)
    {
        if (!std::isfinite(yaw)) return 0.0;
        double out = std::fmod(yaw, 360.0);
        if (out <= -180.0) out += 360.0;
        else if (out > 180.0) out -= 360.0;
        return out;
    }

    void Quarter(int meshYaw, double &c, double &s)
    {
        const int turn = ((meshYaw % 360) + 360) % 360;
        switch (turn)
        {
        case 0: c = 1.0; s = 0.0; return;
        case 90: c = 0.0; s = 1.0; return;
        case 180: c = -1.0; s = 0.0; return;
        case 270: c = 0.0; s = -1.0; return;
        default:
            c = std::cos(turn * kRad);
            s = std::sin(turn * kRad);
            return;
        }
    }

    void Axes(const Rot3 &r, Vec3 &x, Vec3 &y, Vec3 &z)
    {
        const double sp = std::sin(r.pitch * kRad), cp = std::cos(r.pitch * kRad);
        const double sy = std::sin(r.yaw * kRad), cy = std::cos(r.yaw * kRad);
        const double sr = std::sin(r.roll * kRad), cr = std::cos(r.roll * kRad);
        x = Vec3{ cp * cy, cp * sy, sp };
        y = Vec3{ sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp };
        z = Vec3{ -(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp };
    }

    Rot3 FromAxes(const Vec3 &x, const Vec3 &y, const Vec3 &z)
    {
        Rot3 r;
        r.pitch = std::atan2(x.z, std::sqrt(x.x * x.x + x.y * x.y)) / kRad;
        r.yaw = std::atan2(x.y, x.x) / kRad;
        const Vec3 side{ -std::sin(r.yaw * kRad), std::cos(r.yaw * kRad), 0.0 };
        r.roll = std::atan2(Dot(z, side), Dot(y, side)) / kRad;
        return r;
    }

    Vec3 Forward(double heading)
    {
        return Vec3{ std::cos(heading * kRad), std::sin(heading * kRad), 0.0 };
    }

    Vec3 Left(double heading)
    {
        return Vec3{ std::sin(heading * kRad), -std::cos(heading * kRad), 0.0 };
    }

    Vec3 Right(double heading)
    {
        return Vec3{ -std::sin(heading * kRad), std::cos(heading * kRad), 0.0 };
    }

    Vec3 Across(const Vec3 &pos, double heading, double f, double l)
    {
        const Vec3 ahead = Forward(heading);
        const Vec3 left = Left(heading);
        return Vec3{ pos.x + f * ahead.x + l * left.x, pos.y + f * ahead.y + l * left.y, pos.z };
    }

    Vec3 MeshHalf(double f, double l, double u, int meshYaw)
    {
        double c = 0, s = 0;
        Quarter(meshYaw, c, s);
        c = std::fabs(c);
        s = std::fabs(s);
        return Vec3{ c * f + s * l, s * f + c * l, u };
    }

    double Dot(const Vec3 &a, const Vec3 &b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    Vec3 Cross(const Vec3 &a, const Vec3 &b)
    {
        return Vec3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }

    double Length(const Vec3 &v)
    {
        return std::sqrt(Dot(v, v));
    }

    Vec3 Unit(const Vec3 &v)
    {
        const double n = Length(v);
        if (!(n > 1e-12) || !std::isfinite(n)) return Vec3{ 0, 0, 1 };
        return Vec3{ v.x / n, v.y / n, v.z / n };
    }

    double Flat(const Vec3 &a, const Vec3 &b)
    {
        const double dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    Rot3 Attitude(double heading, double pitch, double roll)
    {
        Vec3 x, y, z;
        Axes(Rot3{ std::isfinite(pitch) ? pitch : 0.0, std::isfinite(heading) ? heading : 0.0, std::isfinite(roll) ? roll : 0.0 }, x, y, z);
        return FromAxes(Vec3{ -y.x, -y.y, -y.z }, x, z);
    }

    Rot3 Compose(const Rot3 &outer, const Rot3 &inner)
    {
        Vec3 xo, yo, zo, xi, yi, zi;
        Axes(outer, xo, yo, zo);
        Axes(inner, xi, yi, zi);
        return FromAxes(Blend(xo, yo, zo, xi), Blend(xo, yo, zo, yi), Blend(xo, yo, zo, zi));
    }

    Vec3 Place(const Vec3 &origin, const Rot3 &rot, const Vec3 &flu)
    {
        Vec3 x, y, z;
        Axes(rot, x, y, z);
        return Vec3{ origin.x + flu.x * y.x + flu.y * x.x + flu.z * z.x, origin.y + flu.x * y.y + flu.y * x.y + flu.z * z.y,
                     origin.z + flu.x * y.z + flu.y * x.z + flu.z * z.z };
    }

    Vec3 Local(const Vec3 &origin, const Rot3 &rot, const Vec3 &world)
    {
        Vec3 x, y, z;
        Axes(rot, x, y, z);
        const Vec3 d{ world.x - origin.x, world.y - origin.y, world.z - origin.z };
        return Vec3{ Dot(d, y), Dot(d, x), Dot(d, z) };
    }

    Vec3 Add(const Vec3 &a, const Vec3 &b)
    {
        return Vec3{ a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vec3 Sub(const Vec3 &a, const Vec3 &b)
    {
        return Vec3{ a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vec3 Mul(const Vec3 &a, double k)
    {
        return Vec3{ a.x * k, a.y * k, a.z * k };
    }

    Vec3 Ahead(const Rot3 &rot)
    {
        const double p = rot.pitch * kRad, y = rot.yaw * kRad;
        return Vec3{ std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), std::sin(p) };
    }
}