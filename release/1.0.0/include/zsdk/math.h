#pragma once

namespace zsdk::math
{
    struct Vec3
    {
        double x = 0;
        double y = 0;
        double z = 0;
        friend bool operator==(const Vec3 &, const Vec3 &) = default;
    };

    struct Rot3
    {
        double pitch = 0;
        double yaw = 0;
        double roll = 0;
        friend bool operator==(const Rot3 &, const Rot3 &) = default;
    };

    double Wrap(double yaw);
    void Quarter(int meshYaw, double &c, double &s);
    void Axes(const Rot3 &r, Vec3 &x, Vec3 &y, Vec3 &z);
    Rot3 FromAxes(const Vec3 &x, const Vec3 &y, const Vec3 &z);
    Vec3 Forward(double heading);
    Vec3 Left(double heading);
    Vec3 Right(double heading);
    Vec3 Across(const Vec3 &pos, double heading, double f, double l);
    Vec3 MeshHalf(double f, double l, double u, int meshYaw);
    double Dot(const Vec3 &a, const Vec3 &b);
    Vec3 Cross(const Vec3 &a, const Vec3 &b);
    Vec3 Unit(const Vec3 &v);
    double Length(const Vec3 &v);
    double Flat(const Vec3 &a, const Vec3 &b);
    Rot3 Attitude(double heading, double pitch, double roll);
    Rot3 Compose(const Rot3 &outer, const Rot3 &inner);
    Vec3 Place(const Vec3 &origin, const Rot3 &rot, const Vec3 &flu);
    Vec3 Local(const Vec3 &origin, const Rot3 &rot, const Vec3 &world);
    Vec3 Add(const Vec3 &a, const Vec3 &b);
    Vec3 Sub(const Vec3 &a, const Vec3 &b);
    Vec3 Mul(const Vec3 &a, double k);
    Vec3 Ahead(const Rot3 &rot);
}
