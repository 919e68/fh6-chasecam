#include "sim.hpp"
#include <algorithm>

namespace fh6 {

static const double PI = 3.14159265358979323846, DEG = PI / 180;

double wrapAngle(double a) {
    a = std::fmod(a + PI, 2 * PI);
    if (a < 0) a += 2 * PI;
    return a - PI;
}

double curveAt(const std::vector<Pt>& c, double x) {
    if (c.empty()) return x;
    if (x <= c.front().x) return c.front().y;
    for (size_t i = 1; i < c.size(); i++)
        if (x <= c[i].x) {
            double f = (x - c[i - 1].x) / (c[i].x - c[i - 1].x);
            return c[i - 1].y + (c[i].y - c[i - 1].y) * f;
        }
    return c.back().y;
}

// Spring toward a target with stiffness k and damping ratio z, in 1 ms steps
// so stiff springs stay stable at any frame rate.
static void spring(double& x, double& v, double target, double k, double z, double dt, double maxV = 1e9) {
    double c = 2 * std::sqrt(std::max(k, 1e-6)) * z;
    int n = std::max(1, (int)std::ceil(dt / 0.001));
    double h = dt / n;
    for (int i = 0; i < n; i++) {
        v += (k * (target - x) - c * v) * h;
        v = std::clamp(v, -maxV, maxV);
        x += v * h;
    }
}

// ---------------------------------------------------------------------------
Inputs DemoDrive::step(double dt) {
    t_ += dt;
    // alternate left and right corners every 3.2 s, with a short straight
    double phase = std::sin(2 * PI * t_ / 6.4);
    double k = 0.05 * std::copysign(std::pow(std::fabs(phase), 0.6), phase); // path curvature, 1/m (+ = right)
    speed_ = 21 + 2.5 * std::sin(2 * PI * t_ / 9.0);
    travel_ += speed_ * k * dt;
    // drift angle: nose points into the corner; builds and unwinds smoothly
    double target = -k / 0.05 * 38 * DEG;
    drift_ += (target - drift_) * (1 - std::exp(-2.4 * dt));
    pos_ = pos_ + V3(std::sin(travel_), 0, std::cos(travel_)) * (speed_ * dt);
    Inputs in;
    in.pos = pos_;
    in.travel = travel_;
    in.drift = drift_;
    in.heading = travel_ - drift_;
    in.speed = speed_;
    in.slip = drift_ / (8 * DEG);
    in.latG = speed_ * speed_ * k / 9.81;
    in.lonG = 0.15 * std::cos(2 * PI * t_ / 9.0);
    in.bodyRoll = in.latG * 2.5 * DEG;
    return in;
}

// ---------------------------------------------------------------------------
Inputs TelemetryInputs::update(const Telemetry& t, double dt) {
    Inputs in;
    V3 p(t.pos[0], t.pos[1], t.pos[2]);
    in.pos = p;
    in.speed = t.speed;
    double vx = t.vel[0], vz = t.vel[2];
    double fwd = std::hypot(vx, vz);
    in.drift = fwd > 2 ? std::atan2(vx, vz) : 0;
    if (std::fabs(in.drift) > 120 * DEG) in.drift = 0; // reversing
    // direction of travel from movement; nose = travel minus drift angle
    if (!init_) { lastPos_ = p; travel_ = t.yaw; init_ = true; }
    V3 d = p - lastPos_;
    double moved = std::hypot(d.x, d.z);
    if (moved > 0.05 && moved < 30) {
        double tr = std::atan2(d.x, d.z);
        travel_ += wrapAngle(tr - travel_) * std::min(1.0, 20 * dt);
    }
    if (moved > 0.05 || moved >= 30) lastPos_ = p;
    in.travel = travel_;
    in.heading = travel_ - in.drift;
    // rear tyres' lateral slip; its sign follows the drift angle
    double s = 0.5 * (std::fabs(t.slipAngle[2]) + std::fabs(t.slipAngle[3]));
    in.slip = in.drift >= 0 ? s : -s;
    in.latG = t.accel[0] / 9.81;
    in.lonG = t.accel[2] / 9.81;
    in.bodyRoll = t.roll;
    in.bodyPitch = t.pitch;
    return in;
}

// ---------------------------------------------------------------------------
// Rough stock chase cameras: distance, height, aim height, FOV, max FOV.
struct Base { double dist, height, target, fov, maxFov; };
static const Base BASES[2] = { { 4.6, 1.55, 0.75, 55, 70 }, { 6.2, 2.10, 0.90, 60, 70 } };

CamPose CameraSim::step(const CamParams& p, const Inputs& in, double dt, bool followTravel) {
    const Base& b = BASES[p.slot ? 1 : 0];
    double want = followTravel ? in.travel : in.heading;
    double drift = std::copysign(curveAt(DRIFT_CURVE, std::fabs(in.slip)), in.slip);
    // into the corner: the nose side, so a left-hand drift (drift > 0) turns the view left
    double aimT = -drift * p.cornerAim * DEG;
    double rollT = drift * p.driftTilt * DEG;  // + leans the picture toward the outside
    double sfovT = p.speedFov * curveAt(SPEED_CURVE, in.speed * 2.23694);
    if (!init_) {
        yaw_ = want; yawV_ = 0; aim_ = aimT; aimV_ = 0; roll_ = rollT; rollV_ = 0; sfov_ = sfovT; sfovV_ = 0;
        init_ = true;
    }
    yaw_ = want + wrapAngle(yaw_ - want);
    spring(yaw_, yawV_, want, 8.5 * std::max(p.follow, 0.05), 0.9, dt, 4.05);
    spring(aim_, aimV_, aimT, std::max(p.driftSmooth, 0.5), 1, dt);
    spring(roll_, rollV_, rollT, std::max(p.driftSmooth * 0.8, 0.5), 1, dt);
    spring(sfov_, sfovV_, sfovT, 20, 1, dt);

    double dist = std::max(1.0, b.dist + p.radiusOfs);
    double h = std::max(0.25, b.height + p.heightOfs);
    double tgt = b.target + p.targetOfs;
    V3 back(std::sin(yaw_), 0, std::cos(yaw_));
    CamPose c;
    c.pos = in.pos - back * dist + V3(0, h, 0);
    V3 dir = (in.pos + V3(0, tgt, 0) - c.pos).norm();
    // corner aim: turn the view around world up (+ = right)
    double ca = std::cos(aim_), sa = std::sin(aim_);
    dir = V3(dir.x * ca + dir.z * sa, dir.y, dir.z * ca - dir.x * sa).norm();
    V3 right = V3(0, 1, 0).cross(dir).norm(); // with x right, y up, z forward
    V3 up = dir.cross(right).norm();
    // tilt: lean the camera's up toward the outside of the corner
    double cr = std::cos(roll_), sr = std::sin(roll_);
    c.up = (up * cr + right * sr).norm();
    c.dir = dir;
    c.fov = std::clamp(b.fov + p.fovAdd + sfov_, 20.0, b.maxFov + p.maxFovAdd);
    c.aimDeg = aim_ / DEG;
    c.tiltDeg = roll_ / DEG;
    c.fovNow = c.fov;
    return c;
}

// ---------------------------------------------------------------------------
double Scene::rnd() { seed_ = std::fmod(seed_ * 16807.0, 2147483647.0); return seed_ / 2147483647.0; }

void Scene::step(const Inputs& in, double dt) {
    V3 fwd(std::sin(in.heading), 0, std::cos(in.heading));
    V3 right(fwd.z, 0, -fwd.x);
    V3 rl = in.pos - fwd * 1.35 - right * 0.8, rr = in.pos - fwd * 1.35 + right * 0.8;
    double sliding = std::clamp((std::fabs(in.slip) - 1.0) / 2.0, 0.0, 1.0);
    auto push = [&](std::deque<std::pair<V3, double>>& q, const V3& p) {
        if (q.empty() || (q.back().first - p).len() > 0.4) q.push_back({ p, sliding });
        while (q.size() > 260) q.pop_front();
    };
    push(skidL, V3(rl.x, in.pos.y, rl.z));
    push(skidR, V3(rr.x, in.pos.y, rr.z));
    for (auto& s : smoke) {
        s.age += dt;
        s.size += dt * 0.75;
        s.p.y += dt * 0.35;
    }
    smoke.erase(std::remove_if(smoke.begin(), smoke.end(), [](const Smoke& s) { return s.age > s.life; }), smoke.end());
    spawn_ += dt * 55 * sliding;
    while (spawn_ >= 1) {
        spawn_ -= 1;
        const V3& w = rnd() < 0.5 ? rl : rr;
        smoke.push_back({ V3(w.x + 0.6 * (rnd() - 0.5), in.pos.y + 0.2, w.z + 0.6 * (rnd() - 0.5)), 0, 1.2 + 0.8 * rnd(), 0.3 + 0.2 * rnd() });
    }
    if (smoke.size() > 400) smoke.erase(smoke.begin(), smoke.begin() + (smoke.size() - 400));
}

} // namespace fh6
