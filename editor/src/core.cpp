#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace fh6 {

const Slot SLOTS[2] = {
    { "Chase Near", "FollowLowCam", "FollowCam" },
    { "Chase Far", "FollowHighCam", "FollowCam2" },
};
const std::vector<Pt> DRIFT_CURVE = { { 1.0, 0.0 }, { 2.5, 0.6 }, { 6.0, 1.0 } };
const double DRIFT_CLAMP = 8;
const std::vector<Pt> SPEED_CURVE = { { 40, 0 }, { 70, 0.35 }, { 110, 0.7 }, { 150, 0.9 }, { 200, 1 } };

static std::string f6(double v) { char b[64]; std::snprintf(b, sizeof b, "%.6f", v); return b; }

std::string fmtShort(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.4f", v);
    std::string s = b;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0" || s.empty()) s = "0";
    return s;
}

std::string layer(const std::string& in, const std::string& out, double px, double py, double pz,
                  double cmin, double cmax, const std::vector<Pt>& curve, bool mirrored, double k, double d) {
    std::string s = "<Layer Enabled=\"1\" LocationType=\"All\" InputType=\"" + in + "\" NoiseType=\"None\" OutputType=\"" + out +
                    "\" InputTakeDerivative=\"0\" InputClampMin=\"" + f6(cmin) + "\" InputClampMax=\"" + f6(cmax) +
                    "\" InputMagAttackDecay=\"0\"";
    if (curve.empty()) s += " InputToParamMapping=\"Linear\"";
    else {
        if (curve.size() != 3 && curve.size() != 5) throw std::runtime_error("curve needs 3 or 5 points");
        s += curve.size() == 3 ? " InputToParamMapping=\"ThreePoint\"" : " InputToParamMapping=\"FivePoint\"";
        for (size_t i = 0; i < curve.size(); i++) {
            s += " InputToParamCurveInput" + std::to_string(i) + "=\"" + f6(curve[i].x) + "\"";
            s += " InputToParamCurveOutput" + std::to_string(i) + "=\"" + f6(curve[i].y) + "\"";
        }
        s += std::string(" InputToParamCurveMirrored=\"") + (mirrored ? "1" : "0") + "\"";
    }
    s += " Param.x=\"" + f6(px) + "\" Param.y=\"" + f6(py) + "\" Param.z=\"" + f6(pz) + "\" Param.w=\"" + f6(0) + "\"";
    if (k > 0) s += " Sprung=\"1\" SpringK=\"" + f6(k) + "\" SpringDPercent=\"" + f6(d) + "\"";
    else s += " Sprung=\"0\"";
    return s + " />";
}

// ---------------------------------------------------------------------------
// CameraSettings.ini: first line "<ws>Section\Key<ws>value", value replaced
// ---------------------------------------------------------------------------
bool isWs(char c) { return c == ' ' || c == '\t'; }
static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

std::string editIni(const std::string& t, const std::string& section, const std::string& key,
                    double value, bool relative, bool scaled, IniChange* ch) {
    std::string full = section + "\\" + key;
    size_t ls = 0;
    while (ls <= t.size()) {
        size_t p = ls;
        while (p < t.size() && isWs(t[p])) p++;
        if (t.compare(p, full.size(), full) == 0 && p + full.size() < t.size() && isWs(t[p + full.size()])) {
            size_t v = p + full.size();
            while (v < t.size() && isWs(t[v])) v++;
            size_t e = v;
            while (e < t.size() && !isSpace(t[e])) e++;
            if (e > v) {
                double old = std::strtod(t.substr(v, e - v).c_str(), nullptr);
                double nv = relative ? old + value : scaled ? old * value : value;
                if (ch) *ch = { full, old, nv };
                return t.substr(0, v) + fmtShort(nv) + t.substr(e);
            }
        }
        size_t nl = t.find('\n', ls);
        if (nl == std::string::npos) break;
        ls = nl + 1;
    }
    throw std::runtime_error(full + " not found in CameraSettings.ini");
}

// ---------------------------------------------------------------------------
// CameraPhysics.xml
// ---------------------------------------------------------------------------
XmlSpan xmlSection(const std::string& t, const std::string& name) {
    std::string open = "<" + name;
    size_t p = 0;
    while ((p = t.find(open, p)) != std::string::npos) {
        size_t q = p + open.size();
        if (q < t.size() && isSpace(t[q])) {
            size_t gt = t.find('>', q);
            std::string tag = t.substr(q, gt == std::string::npos ? 0 : gt - q);
            if (gt != std::string::npos && tag.find("NumLayers=\"") != std::string::npos) {
                size_t end = t.find("</" + name + ">", gt + 1);
                if (end == std::string::npos) throw std::runtime_error("</" + name + "> not found in CameraPhysics.xml");
                return { p, gt + 1, end };
            }
        }
        p = q;
    }
    throw std::runtime_error("<" + name + "> not found in CameraPhysics.xml");
}

// <Layer ... /> occurrences in [a, b): (start, one past end)
std::vector<std::pair<size_t, size_t>> xmlLayers(const std::string& t, size_t a, size_t b) {
    std::vector<std::pair<size_t, size_t>> out;
    size_t p = a;
    while ((p = t.find("<Layer", p)) != std::string::npos && p < b) {
        size_t q = p + 6;
        char c = q < t.size() ? t[q] : '\0';
        bool word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        size_t gt = t.find('>', q);
        if (!word && gt != std::string::npos && gt < b && t[gt - 1] == '/') { out.push_back({ p, gt + 1 }); p = gt + 1; }
        else p = q;
    }
    return out;
}

int countLayers(const std::string& t, const std::string& section) {
    XmlSpan s = xmlSection(t, section);
    return (int)xmlLayers(t, s.body, s.end).size();
}

std::string appendLayers(const std::string& t, const std::string& section, const std::vector<std::string>& add) {
    XmlSpan s = xmlSection(t, section);
    std::string nl = t.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string seg = t.substr(s.body, s.end - s.body);
    if (!add.empty()) {
        // indentation of the section's first layer that starts its own line
        std::string ind = "  ";
        for (auto& l : xmlLayers(t, s.start, s.end)) {
            size_t ln = t.rfind('\n', l.first);
            if (ln != std::string::npos && ln >= s.start) {
                bool ok = true;
                for (size_t i = ln + 1; i < l.first; i++) if (!isWs(t[i])) ok = false;
                if (ok) { ind = t.substr(ln + 1, l.first - ln - 1); break; }
            }
        }
        size_t keep = seg.size();
        while (keep > 0 && isWs(seg[keep - 1])) keep--;
        std::string tail = seg.substr(keep), core = seg.substr(0, keep);
        if (core.size() < nl.size() || core.compare(core.size() - nl.size(), nl.size(), nl) != 0) core += nl;
        for (auto& l : add) core += ind + l + nl;
        seg = core + tail;
    }
    size_t count = xmlLayers(seg, 0, seg.size()).size();
    std::string head = t.substr(s.start, s.body - s.start);
    size_t a = head.find("NumLayers=\"") + 11, b = head.find('"', a);
    head = head.substr(0, a) + std::to_string(count) + head.substr(b);
    return t.substr(0, s.start) + head + seg + t.substr(s.end);
}

// ---------------------------------------------------------------------------
CamParams noChangeParams(int slot) {
    CamParams q;
    q.slot = slot;
    q.heightOfs = q.targetOfs = q.radiusOfs = q.fovAdd = q.maxFovAdd = 0;
    q.follow = 1;
    q.cornerAim = q.driftTilt = q.speedFov = 0;
    return q;
}

bool isNoChange(const CamParams& p) {
    return p.heightOfs == 0 && p.targetOfs == 0 && p.radiusOfs == 0 && p.fovAdd == 0 && p.maxFovAdd == 0 && p.follow == 1 &&
           p.cornerAim == 0 && p.driftTilt == 0 && p.speedFov == 0;
}

std::vector<std::string> quickLayers(const CamParams& p) {
    double aim = (p.flipAim ? -1 : 1) * p.cornerAim, tilt = (p.flipTilt ? -1 : 1) * p.driftTilt;
    std::vector<std::string> add;
    if (p.cornerAim != 0)
        add.push_back(layer("Slip_Lateral", "CameraSpaceYPROffset", aim, 0, 0, -DRIFT_CLAMP, DRIFT_CLAMP, DRIFT_CURVE, true, p.driftSmooth, 1));
    if (p.driftTilt != 0)
        add.push_back(layer("Slip_Lateral", "CameraSpaceYPROffset", 0, 0, tilt, -DRIFT_CLAMP, DRIFT_CLAMP, DRIFT_CURVE, true, p.driftSmooth * 0.8, 1));
    if (p.speedFov != 0)
        add.push_back(layer("SpeedMPH", "FOV", p.speedFov, 0, 0, 0, 200, SPEED_CURVE, false, 20, 1));
    return add;
}

Built build(const std::string& stockIni, const std::string& stockXml, const CamParams& p) {
    const Slot& sl = SLOTS[p.slot ? 1 : 0];
    Built r;
    std::string ini = stockIni;
    struct E { const char* key; double v; bool rel, scale; } edits[] = {
        { "HeightOffsetGlobal", p.heightOfs, true, false },
        { "HeightMinAboveGround", p.heightOfs, true, false },
        { "HeightMaxAboveGround", p.heightOfs, true, false },
        { "TargetYOffsetGlobal", p.targetOfs, true, false },
        { "RadiusOffsetGlobal", p.radiusOfs, true, false },
        { "FOV", p.fovAdd, true, false },
        { "MaxFOV", p.maxFovAdd, true, false },
        { "Spring\\YRotSpringK", p.follow, false, true },
    };
    for (auto& e : edits) {
        if ((e.rel && e.v == 0) || (e.scale && e.v == 1)) continue; // no change: keep the stock text
        IniChange c;
        ini = editIni(ini, sl.ini, e.key, e.v, e.rel, e.scale, &c);
        r.changes.push_back(c);
    }
    std::vector<std::string> add = quickLayers(p);
    r.layersBefore = countLayers(stockXml, sl.xml);
    r.xml = appendLayers(stockXml, sl.xml, add);
    r.layersAfter = countLayers(r.xml, sl.xml);
    r.ini = ini;
    return r;
}

bool readFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

} // namespace fh6
