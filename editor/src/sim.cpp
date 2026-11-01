#include "sim.hpp"
#include "schema.hpp"
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
    speed_ = 15 + 2 * std::sin(2 * PI * t_ / 9.0); // ~55 km/h: about 1 G at full lock, like a tight drift
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
    in.slipLon = 0.5 * (t.slipRatio[2] + t.slipRatio[3]);
    in.slipAll = 0.5 * (t.combinedSlip[2] + t.combinedSlip[3]);
    in.suspLon = 0.5 * (t.suspNorm[0] + t.suspNorm[1]) - 0.5 * (t.suspNorm[2] + t.suspNorm[3]);
    in.suspLat = 0.5 * (t.suspNorm[0] + t.suspNorm[2]) - 0.5 * (t.suspNorm[1] + t.suspNorm[3]);
    in.vertAcc = t.accel[1];
    bool air = t.suspNorm[0] < 0.01f && t.suspNorm[1] < 0.01f && t.suspNorm[2] < 0.01f && t.suspNorm[3] < 0.01f;
    airTime_ = air ? airTime_ + dt : 0;
    in.airTime = airTime_;
    in.telemetry = true;
    return in;
}

// ---------------------------------------------------------------------------
void fillInputs(const Inputs& in, InputFrame& f) {
    auto set = [&](const char* name, double v, bool sim = true) {
        int i = inputIndex(name);
        if (i >= 0) { f.v[i] = v; f.sim[i] = sim; }
    };
    double mph = in.speed * 2.23694;
    set("SpeedMPH", mph);
    set("Speed", std::min(mph / 240, 1.0));
    set("Gs_Longitudinal", in.lonG);
    set("Gs_Lateral", in.latG);
    set("Slip_Lateral", in.slip);
    set("Slip_Longitudinal", in.slipLon, in.telemetry);
    set("Slip", in.telemetry ? in.slipAll : std::fabs(in.slip));
    set("SuspensionBias_Longitudinal", in.suspLon, in.telemetry);
    set("SuspensionBias_Lateral", in.suspLat, in.telemetry);
    set("SuspensionAngleBias_Longitudinal", in.bodyPitch / DEG);
    set("SuspensionAngleBias_Lateral", in.bodyRoll / DEG);
    set("PitchAngleDegrees", in.bodyPitch / DEG);
    set("VerticalAcceleration", in.vertAcc, in.telemetry);
    set("OnRoadVerticalAcceleration", in.vertAcc, in.telemetry);
    set("OneWhenStationary", in.speed < 0.5 ? 1 : 0);
    set("TimeInAir", in.airTime, in.telemetry);
    set("ConstantOne", 1);
}

// Car box for the FH1 chase formula: roof height and half length (the S15).
static const double CAR_TOP = 1.30, CAR_HALF = 2.22;
static const double BASE_H[2] = { 0.35, 0.6 }, BASE_R[2] = { 2.95, 3.1 }; // FH1's base offsets, low / high

ChaseConfig chaseConfig(const std::map<std::string, double>& ini, int slot) {
    std::string pre = slot ? "FollowHighCam\\" : "FollowLowCam\\";
    auto get = [&](const char* k) {
        auto it = ini.find(pre + k);
        if (it != ini.end()) return it->second;
        const KeyInfo* ki = findKey(pre + k);
        return ki ? ki->def : 0.0;
    };
    ChaseConfig c;
    c.slot = slot;
    c.heightOfs = get("HeightOffsetGlobal");
    c.radiusOfs = get("RadiusOffsetGlobal");
    c.targetOfs = get("TargetYOffsetGlobal");
    c.hMin = get("HeightMinAboveGround");
    c.hMax = get("HeightMaxAboveGround");
    c.fov = get("FOV");
    c.minFov = get("MinFOV");
    c.maxFov = get("MaxFOV");
    c.yK = get("Spring\\YRotSpringK");
    c.yD = get("Spring\\YRotSpringD");
    c.yCar = get("Spring\\YRotCarDampScale");
    c.yMax = get("Spring\\YRotMaxSpeed");
    c.hK = get("Spring\\HeightSpringK");
    c.hD = get("Spring\\HeightSpringD");
    c.hKAir = get("Spring\\HeightSpringKInAir");
    c.hDAir = get("Spring\\HeightSpringDInAir");
    return c;
}

// rotate v around unit axis k by angle a (right hand)
static V3 rot(const V3& v, const V3& k, double a) {
    double c = std::cos(a), s = std::sin(a);
    return v * c + k.cross(v) * s + k * (k.dot(v) * (1 - c));
}

CamPose CameraSim::step(const ChaseConfig& c, const LayerOut& lo, const Inputs& in, double dt, bool followTravel) {
    double want = followTravel ? in.travel : in.heading;
    if (!init_) { yaw_ = want; yawV_ = 0; lastWant_ = want; hY_ = in.pos.y; hV_ = 0; init_ = true; }
    double carRate = wrapAngle(want - lastWant_) / std::max(dt, 1e-4);
    lastWant_ = want;
    yaw_ = want + wrapAngle(yaw_ - want);
    // FH1: yaw_vel += dt (K err - D (yaw_vel - CarDampScale car_yaw_rate)), capped at YRotMaxSpeed
    double rest = CAR_TOP + BASE_H[c.slot ? 1 : 0] + c.heightOfs;
    double h = c.hMin <= c.hMax ? std::clamp(rest, c.hMin, c.hMax) : rest;
    double tgt = CAR_TOP + c.targetOfs + (h - rest); // the aim moves with the clamp
    double ty = in.pos.y + std::max(0.15, h);
    bool air = in.airTime > 0.2;
    double hk = std::max(0.0, air ? c.hKAir : c.hK), hd = std::max(0.0, air ? c.hDAir : c.hD);
    int n = std::max(1, (int)std::ceil(dt / 0.001));
    double st = dt / n, ymax = std::max(0.01, c.yMax);
    for (int i = 0; i < n; i++) {
        double err = wrapAngle(want - yaw_);
        yawV_ += st * (c.yK * err - c.yD * (yawV_ - c.yCar * carRate));
        yawV_ = std::clamp(yawV_, -ymax, ymax);
        yaw_ += yawV_ * st;
        hV_ += st * ((ty - hY_) * hk - hd * hV_);
        hY_ += hV_ * st;
    }
    if (!std::isfinite(yaw_) || !std::isfinite(hY_)) { yaw_ = want; yawV_ = 0; hY_ = ty; hV_ = 0; }

    double dist = std::max(0.8, CAR_HALF + BASE_R[c.slot ? 1 : 0] + c.radiusOfs);
    V3 fwd(std::sin(yaw_), 0, std::cos(yaw_)), right(fwd.z, 0, -fwd.x), up0(0, 1, 0);
    CamPose p;
    p.pos = V3(in.pos.x, hY_, in.pos.z) - fwd * dist;
    // car-space offsets move the rig before it aims (rig axes: the camera's yaw)
    p.pos = p.pos + right * lo.carXYZ[0] + up0 * lo.carXYZ[1] + fwd * lo.carXYZ[2];
    V3 dir = (in.pos + V3(0, tgt, 0) - p.pos).norm();
    // rotations, in degrees: yaw + turns left, pitch + looks up, roll + leans the picture right
    double yawT = lo.carYPR[0] + lo.camYPR[0], pitchT = lo.carYPR[1] + lo.camYPR[1], rollT = lo.carYPR[2] + lo.camYPR[2];
    dir = rot(dir, up0, -yawT * DEG).norm();
    V3 r = up0.cross(dir).norm();
    dir = rot(dir, r, -pitchT * DEG).norm();
    r = up0.cross(dir).norm();
    V3 u = dir.cross(r).norm();
    u = rot(u, dir, -rollT * DEG).norm();
    r = u.cross(dir).norm();
    p.pos = p.pos + r * lo.camXYZ[0] + u * lo.camXYZ[1] + dir * lo.camXYZ[2];
    p.dir = dir;
    p.up = u;
    double lo_ = std::max(1.0, std::min(c.minFov, c.maxFov)), hi = std::max(lo_, c.maxFov);
    p.fov = std::clamp(c.fov + lo.fov, lo_, hi);
    p.aimDeg = yawT;
    p.tiltDeg = rollT;
    p.fovNow = p.fov;
    return p;
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
