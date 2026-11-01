// Preview model: what the camera reacts to (from Data Out or a demo drive),
// and an approximation of the FH6 chase camera with the editor's settings.
// The game's own base camera differs per car, so the preview is a guide for
// direction, strength and timing; the game itself is the final check.
#pragma once
#include "core.hpp"
#include "dataout.hpp"
#include <cmath>
#include <deque>
#include <vector>

namespace fh6 {

struct V3 {
    double x = 0, y = 0, z = 0;
    V3() = default;
    V3(double a, double b, double c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    V3 operator-(const V3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    V3 operator*(double s) const { return { x * s, y * s, z * s }; }
    double dot(const V3& o) const { return x * o.x + y * o.y + z * o.z; }
    V3 cross(const V3& o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
    double len() const { return std::sqrt(dot(*this)); }
    V3 norm() const { double l = len(); return l > 1e-9 ? *this * (1 / l) : V3(0, 0, 1); }
};

// What the camera layers read, in the editor's terms.
struct Inputs {
    V3 pos;            // car position, world metres
    double heading = 0;    // car nose direction, radians from +Z toward +X
    double travel = 0;     // direction of travel, same convention
    double bodyRoll = 0, bodyPitch = 0;
    double speed = 0;      // m/s
    double drift = 0;      // drift angle in radians, + = travelling to the right of the nose (left-hand drift)
    double slip = 0;       // lateral tyre slip like the camera's Slip_Lateral: >1 = sliding, signed like drift
    double latG = 0, lonG = 0; // G, car space (+lat = right, +lon = forward)
};

double wrapAngle(double a);
double curveAt(const std::vector<Pt>& c, double x); // piecewise linear, clamped at the ends

// Synthetic linked drifts, for when the game isn't sending Data Out.
class DemoDrive {
public:
    Inputs step(double dt);
private:
    double t_ = 0, travel_ = 0, drift_ = 0, speed_ = 21;
    V3 pos_;
};

// Turns Data Out packets into Inputs (heading worked out from travel and the
// car-space velocity, so it doesn't depend on the game's yaw convention).
class TelemetryInputs {
public:
    Inputs update(const Telemetry& t, double dt);
    void reset() { init_ = false; }
private:
    bool init_ = false;
    V3 lastPos_;
    double travel_ = 0;
};

// Approximate chase camera.
struct CamPose { V3 pos, dir, up; double fov = 60; double aimDeg = 0, tiltDeg = 0, fovNow = 0; };
class CameraSim {
public:
    CamPose step(const CamParams& p, const Inputs& in, double dt, bool followTravel);
    void reset() { init_ = false; }
private:
    bool init_ = false;
    double yaw_ = 0, yawV_ = 0, aim_ = 0, aimV_ = 0, roll_ = 0, rollV_ = 0, sfov_ = 0, sfovV_ = 0;
};

// Skid marks and tyre smoke behind the car.
struct Smoke { V3 p; double age, life, size; };
class Scene {
public:
    void step(const Inputs& in, double dt);
    std::deque<std::pair<V3, double>> skidL, skidR; // point, strength 0..1
    std::vector<Smoke> smoke;
private:
    double spawn_ = 0, seed_ = 1;
    double rnd();
};

} // namespace fh6
