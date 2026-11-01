#include "layereval.hpp"
#include "schema.hpp"
#include <algorithm>
#include <cmath>

namespace fh6 {

InputFrame::InputFrame() : v(inputs().size(), 0.0), sim(inputs().size(), 0) {}

void LayerStack::load(const std::vector<std::string>& layers) {
    std::vector<Def> defs;
    for (auto& text : layers) {
        Layer l = parseLayer(text);
        Def d;
        d.enabled = l.flag("Enabled");
        d.island = l.str("LocationType") == "IslandOnly";
        std::string in = l.str("InputType"), out = l.str("OutputType");
        d.input = inputIndex(in);
        d.output = outputIndex(out);
        if (d.input >= 0 && (inputs()[d.input].flags & E_MODONLY)) d.input = -1; // hand-made "Constant": semantics unknown
        d.impulse = d.output >= 0 && (outputs()[d.output].flags & E_IMPULSE);
        d.deriv = l.flag("InputTakeDerivative");
        d.clamp = l.has("InputClampMin") && l.has("InputClampMax");
        d.cmin = l.num("InputClampMin");
        d.cmax = l.num("InputClampMax");
        if (d.cmax < d.cmin) std::swap(d.cmin, d.cmax);
        d.env = l.flag("InputMagAttackDecay");
        d.attack = l.num("InputAttack", 1000);
        d.decay = l.num("InputDecay", 5);
        std::string m = l.str("InputToParamMapping", "Linear");
        d.mapping = m == "ConstantOne" ? 1 : (m == "ThreePoint" || m == "FivePoint") ? 2 : 0;
        d.curve = curvePts(l, "InputToParam");
        for (int i = 0; i < 4; i++) d.param[i] = l.num(std::string("Param.") + "xyzw"[i]);
        d.sprung = l.flag("Sprung") || d.impulse;
        d.k = l.num("SpringK", 100);
        d.d = l.num("SpringDPercent", 1);
        std::string n = l.str("NoiseType", "None");
        d.noise = n == "None" ? 0 : n == "Sin" ? 1 : n == "HeightGrid" ? 3 : 2;
        std::string nm = l.str("InputToNoiseFreqMapping", "ConstantOne");
        d.noiseMap = nm == "Linear" ? 1 : nm == "Abs" ? 2 : (nm == "ThreePoint" || nm == "FivePoint") ? 3 : 0;
        d.ncurve = curvePts(l, "InputToNoiseFreq");
        d.nfreq = l.num("NoiseFreq", 0);
        d.nseed = l.num("NoiseSeed", 0);
        d.nmin = l.num("NoiseOutputRangeMin", -1);
        d.nmax = l.num("NoiseOutputRangeMax", 1);
        d.sig = in + "/" + out;
        defs.push_back(d);
    }
    // keep the running state of layers that are still the same kind at the same place
    std::vector<State> st(defs.size());
    for (size_t i = 0; i < defs.size() && i < defs_.size(); i++)
        if (defs[i].sig == defs_[i].sig) st[i] = states_[i];
    defs_ = std::move(defs);
    states_ = std::move(st);
    probes_.assign(defs_.size(), LayerProbe());
}

static double hash1(double i, double seed) {
    double s = std::sin(i * 127.1 + seed * 311.7) * 43758.5453;
    return (s - std::floor(s)) * 2 - 1;
}
static double vnoise(double t, double seed) {
    double i = std::floor(t), f = t - i, u = f * f * (3 - 2 * f);
    double a = hash1(i, seed), b = hash1(i + 1, seed);
    return a + (b - a) * u;
}
static double noiseAt(int type, double t, double seed) {
    if (type == 1) return std::sin(2 * 3.14159265358979 * t + seed);
    if (type == 3) return vnoise(t, seed);
    return std::clamp(0.75 * vnoise(t, seed) + 0.35 * vnoise(2.03 * t + 7.7, seed + 19.1), -1.0, 1.0);
}

LayerOut LayerStack::step(const InputFrame& in, double dt) {
    LayerOut o;
    dt = std::clamp(dt, 1e-4, 0.1);
    for (size_t i = 0; i < defs_.size(); i++) {
        const Def& d = defs_[i];
        State& s = states_[i];
        LayerProbe& p = probes_[i];
        p = LayerProbe();
        if (!d.enabled || d.island || d.input < 0 || d.output < 0 || d.impulse) continue;
        p.active = true;
        p.simulated = in.sim[d.input] != 0;
        double x = in.v[d.input];
        p.input = x;
        if (d.deriv) {
            double r = s.hasPrev ? (x - s.prev) / dt : 0;
            s.prev = x;
            s.hasPrev = true;
            x = r;
        }
        if (d.clamp) x = std::clamp(x, d.cmin, d.cmax);
        if (d.env) {
            double t = std::fabs(x), rate = t > s.env ? d.attack : d.decay;
            s.env += (t - s.env) * std::min(1.0, rate * dt);
            x = s.env;
        }
        p.x = x;
        double f = d.mapping == 1 ? 1 : d.mapping == 2 ? curveEval(d.curve, x) : x;
        if (d.noise) {
            double fm = d.noiseMap == 1 ? x : d.noiseMap == 2 ? std::fabs(x) : d.noiseMap == 3 ? curveEval(d.ncurve, x) : 1;
            s.phase += d.nfreq * fm * dt;
            double n01 = (noiseAt(d.noise, s.phase, d.nseed) + 1) * 0.5;
            f *= d.nmin + (d.nmax - d.nmin) * n01;
        }
        p.factor = f;
        double k = std::max(d.k, 0.01), c = 2 * std::sqrt(k) * std::max(d.d, 0.0);
        int n = d.sprung ? std::max(1, (int)std::ceil(dt / 0.002)) : 0;
        double h = n ? dt / n : 0;
        for (int a = 0; a < 4; a++) {
            double target = f * d.param[a];
            if (!d.sprung) { s.x[a] = target; continue; }
            for (int j = 0; j < n; j++) {
                s.v[a] += (k * (target - s.x[a]) - c * s.v[a]) * h;
                s.x[a] += s.v[a] * h;
            }
            if (!std::isfinite(s.x[a])) { s.x[a] = target; s.v[a] = 0; }
        }
        for (int a = 0; a < 4; a++) p.out[a] = s.x[a];
        const std::string out = outputs()[d.output].name;
        double* dst = nullptr;
        if (out == "CarSpaceXYZOffset") dst = o.carXYZ;
        else if (out == "CarSpaceYPROffset") dst = o.carYPR;
        else if (out == "CameraSpaceXYZOffset") dst = o.camXYZ;
        else if (out == "CameraSpaceYPROffset") dst = o.camYPR;
        if (dst) for (int a = 0; a < 3; a++) dst[a] += s.x[a];
        else if (out == "FOV") o.fov += s.x[0];
        else if (out == "Vignette") o.vignette += s.x[0];
        else if (out == "SteeringWheelAngle") o.wheel += s.x[0];
    }
    return o;
}

} // namespace fh6
