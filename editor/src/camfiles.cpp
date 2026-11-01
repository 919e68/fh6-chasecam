#include "camfiles.hpp"
#include "schema.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <stdexcept>

namespace fh6 {

std::string f6(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.6f", v);
    return b;
}

static bool isSp(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static std::string newline(const std::string& t) { return t.find("\r\n") != std::string::npos ? "\r\n" : "\n"; }

// ---------------------------------------------------------------------------
// ini
std::vector<IniEntry> iniEntries(const std::string& t) {
    std::vector<IniEntry> out;
    size_t ls = 0;
    while (ls < t.size()) {
        size_t le = t.find('\n', ls);
        if (le == std::string::npos) le = t.size();
        size_t p = ls;
        while (p < le && isSp(t[p])) p++;
        size_t ke = p;
        while (ke < le && !isSp(t[ke])) ke++;
        size_t v = ke;
        while (v < le && isSp(t[v])) v++;
        size_t ve = v;
        while (ve < le && !isSp(t[ve])) ve++;
        if (ke > p && ve > v) out.push_back({ t.substr(p, ke - p), t.substr(v, ve - v) });
        ls = le + 1;
    }
    return out;
}

std::map<std::string, double> iniValues(const std::string& t) {
    std::map<std::string, double> m;
    for (auto& e : iniEntries(t)) m[e.key] = std::strtod(e.value.c_str(), nullptr);
    return m;
}

std::string iniSet(const std::string& t, const std::string& key, const std::string& value) {
    // existing line: replace the value token
    size_t ls = 0, bestEnd = std::string::npos, bestLen = 0;
    while (ls < t.size()) {
        size_t le = t.find('\n', ls);
        if (le == std::string::npos) le = t.size();
        size_t p = ls;
        while (p < le && isWs(t[p])) p++;
        size_t ke = p;
        while (ke < le && !isSp(t[ke])) ke++;
        std::string k = t.substr(p, ke - p);
        if (k == key) {
            size_t v = ke;
            while (v < le && isWs(t[v])) v++;
            size_t ve = v;
            while (ve < le && !isSp(t[ve])) ve++;
            if (v == ke) return t.substr(0, ke) + " " + value + t.substr(ve);
            return t.substr(0, v) + value + t.substr(ve);
        }
        // remember the last line sharing the longest block prefix
        size_t common = 0;
        for (size_t i = 0; i < k.size() && i < key.size() && k[i] == key[i]; i++)
            if (k[i] == '\\') common = i + 1;
        if (common > 0 && common >= bestLen) { bestLen = common; bestEnd = le; }
        ls = le + 1;
    }
    std::string nl = newline(t), line = key + " " + value;
    if (bestEnd != std::string::npos) {
        if (bestEnd >= t.size()) return t + nl + line + nl;
        size_t at = t[bestEnd] == '\n' ? bestEnd + 1 : bestEnd; // after the line's newline (CRLF: '\n' is last)
        return t.substr(0, at) + line + nl + t.substr(at);
    }
    std::string r = t;
    if (!r.empty() && r.back() != '\n') r += nl;
    if (!r.empty()) r += nl;
    return r + line + nl;
}

// ---------------------------------------------------------------------------
// Layers
const std::string* Layer::find(const std::string& n) const {
    for (auto& x : a) if (x.name == n) return &x.value;
    return nullptr;
}
std::string Layer::str(const std::string& n, const std::string& def) const {
    const std::string* v = find(n);
    return v ? *v : def;
}
double Layer::num(const std::string& n, double def) const {
    const std::string* v = find(n);
    if (!v) return def;
    const char* s = v->c_str();
    char* end = nullptr;
    double d = std::strtod(s, &end);
    return end == s ? def : d;
}
void Layer::set(const std::string& n, const std::string& v) {
    for (auto& x : a) if (x.name == n) { x.value = v; return; }
    a.push_back({ n, v });
}
void Layer::setNum(const std::string& n, double v) { set(n, f6(v)); }
void Layer::erase(const std::string& n) {
    a.erase(std::remove_if(a.begin(), a.end(), [&](const Attr& x) { return x.name == n; }), a.end());
}

Layer parseLayer(const std::string& t) {
    Layer l;
    size_t p = t.find("<Layer");
    if (p == std::string::npos) return l;
    p += 6;
    while (p < t.size()) {
        while (p < t.size() && isSp(t[p])) p++;
        if (p >= t.size() || t[p] == '/' || t[p] == '>') break;
        size_t ne = p;
        while (ne < t.size() && !isSp(t[ne]) && t[ne] != '=' && t[ne] != '/' && t[ne] != '>') ne++;
        std::string name = t.substr(p, ne - p);
        p = ne;
        while (p < t.size() && isSp(t[p])) p++;
        if (p >= t.size() || t[p] != '=') { if (!name.empty()) l.a.push_back({ name, "" }); continue; }
        p++;
        while (p < t.size() && isSp(t[p])) p++;
        if (p >= t.size()) break;
        char q = t[p];
        if (q != '"' && q != '\'') break;
        size_t ve = t.find(q, p + 1);
        if (ve == std::string::npos) break;
        l.a.push_back({ name, t.substr(p + 1, ve - p - 1) });
        p = ve + 1;
    }
    return l;
}

std::string writeLayer(const Layer& l) {
    std::string s = "<Layer";
    for (auto& x : l.a) s += " " + x.name + "=\"" + x.value + "\"";
    return s + " />";
}

static int pointsFor(const std::string& mapping) { return mapping == "ThreePoint" ? 3 : mapping == "FivePoint" ? 5 : 0; }

CurvePts curvePts(const Layer& l, const std::string& pre) {
    CurvePts c;
    c.n = pointsFor(l.str(pre + "Mapping"));
    for (int i = 0; i < c.n; i++) {
        c.x[i] = l.num(pre + "CurveInput" + std::to_string(i));
        c.y[i] = l.num(pre + "CurveOutput" + std::to_string(i));
    }
    c.mirrored = l.flag(pre + "CurveMirrored");
    return c;
}

static double interp(const CurvePts& c, double x) {
    if (c.n == 0) return x;
    if (x <= c.x[0]) return c.y[0];
    for (int i = 1; i < c.n; i++)
        if (x <= c.x[i]) {
            double d = c.x[i] - c.x[i - 1];
            return d <= 1e-12 ? c.y[i] : c.y[i - 1] + (c.y[i] - c.y[i - 1]) * (x - c.x[i - 1]) / d;
        }
    return c.y[c.n - 1];
}

double curveEval(const CurvePts& c, double x) {
    if (c.mirrored) return x < 0 ? -interp(c, -x) : interp(c, x);
    return interp(c, x);
}

void normalizeLayer(Layer& l) {
    std::string in = l.str("InputType"), out = l.str("OutputType"), noise = l.str("NoiseType", "None");
    if (in == "Constant") return; // hand-made mod layer: keep its own shape
    int oi = outputIndex(out), ii = inputIndex(in);
    bool impulse = oi >= 0 ? (outputs()[oi].flags & E_IMPULSE) != 0 : out.find("Impulse") != std::string::npos;
    bool collision = ii >= 0 ? (inputs()[ii].flags & E_COLLISION) != 0 : false;
    bool noClamp = ii >= 0 ? (inputs()[ii].flags & E_NOCLAMP) != 0 : false;
    double cmin = ii >= 0 ? inputs()[ii].clampMin : 0, cmax = ii >= 0 ? inputs()[ii].clampMax : 1;

    std::vector<Attr> o;
    std::set<std::string> known;
    auto push = [&](const std::string& n, const std::string& def) {
        known.insert(n);
        const std::string* v = l.find(n);
        o.push_back({ n, v ? *v : def });
    };
    auto curve = [&](const std::string& pre, double lo, double hi, double y0, double y1) {
        int n = pointsFor(l.str(pre + "Mapping"));
        for (int i = 0; i < n; i++) {
            double t = n > 1 ? (double)i / (n - 1) : 0;
            push(pre + "CurveInput" + std::to_string(i), f6(lo + (hi - lo) * t));
            push(pre + "CurveOutput" + std::to_string(i), f6(y0 + (y1 - y0) * t));
        }
        if (n) push(pre + "CurveMirrored", "0");
    };
    push("Enabled", "1");
    push("LocationType", "All");
    push("InputType", in);
    push("NoiseType", "None");
    push("OutputType", out);
    if (!collision) push("InputTakeDerivative", "0");
    if (!noClamp) { push("InputClampMin", f6(cmin)); push("InputClampMax", f6(cmax)); }
    push("InputMagAttackDecay", "0");
    if (l.flag("InputMagAttackDecay")) { push("InputAttack", f6(1000)); push("InputDecay", f6(5)); }
    if (noise != "None") {
        push("InputToNoiseFreqMapping", "ConstantOne");
        curve("InputToNoiseFreq", l.num("InputClampMin", cmin), l.num("InputClampMax", cmax), 1, 1);
        push("NoiseFreq", f6(10));
        push("NoiseSeed", f6(0));
        push("NoiseOutputRangeMin", f6(-1));
        push("NoiseOutputRangeMax", f6(1));
    }
    push("InputToParamMapping", "Linear");
    curve("InputToParam", l.num("InputClampMin", cmin), l.num("InputClampMax", cmax), 0, 1);
    for (const char* c : { "Param.x", "Param.y", "Param.z", "Param.w" }) push(c, f6(0));
    if (impulse) {
        push("ImpulseInputSizeCutoff", f6(0));
        push("ImpulseDelay", f6(0));
        push("SpringK", f6(150));
        push("SpringDPercent", f6(0.5));
    } else {
        push("Sprung", "0");
        if (l.flag("Sprung")) { push("SpringK", f6(100)); push("SpringDPercent", f6(1)); }
    }
    // everything the editor doesn't know about stays, in its order (FH4 rebound springs and the like)
    static const std::set<std::string> managed = [] {
        std::set<std::string> s = { "Enabled", "LocationType", "InputType", "NoiseType", "OutputType", "InputTakeDerivative", "InputClampMin",
                                    "InputClampMax", "InputMagAttackDecay", "InputAttack", "InputDecay", "InputToNoiseFreqMapping",
                                    "InputToNoiseFreqCurveMirrored", "NoiseFreq", "NoiseSeed", "NoiseOutputRangeMin", "NoiseOutputRangeMax",
                                    "InputToParamMapping", "InputToParamCurveMirrored", "Param.x", "Param.y", "Param.z", "Param.w", "Sprung",
                                    "SpringK", "SpringDPercent", "ImpulseInputSizeCutoff", "ImpulseDelay" };
        for (int i = 0; i < 5; i++)
            for (const char* p : { "InputToParamCurveInput", "InputToParamCurveOutput", "InputToNoiseFreqCurveInput", "InputToNoiseFreqCurveOutput" })
                s.insert(p + std::to_string(i));
        return s;
    }();
    for (auto& x : l.a)
        if (!known.count(x.name) && !managed.count(x.name)) o.push_back(x);
    l.a = o;
}

void setMapping(Layer& l, const std::string& pre, const std::string& mapping) {
    std::string old = l.str(pre + "Mapping");
    CurvePts c = curvePts(l, pre);
    int n = pointsFor(mapping);
    double lo = l.num("InputClampMin", 0), hi = l.num("InputClampMax", 1);
    if (c.n) { lo = c.x[0]; hi = c.x[c.n - 1]; }
    if (hi - lo < 1e-9) hi = lo + 1;
    double nx[5], ny[5];
    for (int i = 0; i < n; i++) {
        double t = n > 1 ? (double)i / (n - 1) : 0;
        nx[i] = lo + (hi - lo) * t;
        if (c.n) ny[i] = curveEval(c, nx[i]);
        else if (old == "ConstantOne") ny[i] = 1;
        else if (pre == "InputToNoiseFreq" && old != "Linear" && old != "Abs") ny[i] = 1;
        else ny[i] = old == "Abs" ? std::fabs(nx[i]) : nx[i];
    }
    for (int i = 0; i < 5; i++) { l.erase(pre + "CurveInput" + std::to_string(i)); l.erase(pre + "CurveOutput" + std::to_string(i)); }
    bool mir = c.mirrored;
    l.erase(pre + "CurveMirrored");
    l.set(pre + "Mapping", mapping);
    // insert the points right after the mapping attribute
    std::vector<Attr> pts;
    for (int i = 0; i < n; i++) {
        pts.push_back({ pre + "CurveInput" + std::to_string(i), f6(nx[i]) });
        pts.push_back({ pre + "CurveOutput" + std::to_string(i), f6(ny[i]) });
    }
    if (n) pts.push_back({ pre + "CurveMirrored", mir ? "1" : "0" });
    for (size_t i = 0; i < l.a.size(); i++)
        if (l.a[i].name == pre + "Mapping") { l.a.insert(l.a.begin() + i + 1, pts.begin(), pts.end()); break; }
    normalizeLayer(l);
}

// ---------------------------------------------------------------------------
// Sections
std::vector<XmlSection> xmlSections(const std::string& t) {
    std::vector<XmlSection> out;
    size_t p = 0;
    while ((p = t.find('<', p)) != std::string::npos) {
        size_t q = p + 1, ne = q;
        while (ne < t.size() && (std::isalnum((unsigned char)t[ne]) || t[ne] == '_')) ne++;
        std::string name = t.substr(q, ne - q);
        size_t gt = t.find('>', ne);
        if (name.empty() || name == "Layer" || gt == std::string::npos) { p = q; continue; }
        std::string tag = t.substr(ne, gt - ne);
        size_t nl = tag.find("NumLayers=");
        if (nl == std::string::npos) { p = q; continue; }
        XmlSection s;
        s.name = name;
        size_t vq = nl + 10;
        if (vq < tag.size() && (tag[vq] == '"' || tag[vq] == '\'')) s.declared = std::atoi(tag.c_str() + vq + 1);
        size_t end = t.find("</" + name + ">", gt + 1);
        if (end == std::string::npos) break;
        for (auto& sp : xmlLayers(t, gt + 1, end)) s.layers.push_back(t.substr(sp.first, sp.second - sp.first));
        out.push_back(std::move(s));
        p = end + name.size() + 3;
    }
    return out;
}

const XmlSection* findSection(const std::vector<XmlSection>& v, const std::string& name) {
    for (auto& s : v) if (s.name == name) return &s;
    return nullptr;
}

std::string replaceSectionLayers(const std::string& t, const std::string& section, const std::vector<std::string>& layers) {
    XmlSpan s = xmlSection(t, section);
    std::string nl = newline(t);
    size_t ls = t.rfind('\n', s.start);
    ls = ls == std::string::npos ? 0 : ls + 1;
    std::string ind;
    for (size_t i = ls; i < s.start && isWs(t[i]); i++) ind += t[i];
    // layers keep the indentation of the section's first layer that starts its own line
    std::string lind = ind.empty() ? "  " : ind;
    for (auto& l : xmlLayers(t, s.body, s.end)) {
        size_t ln = t.rfind('\n', l.first);
        if (ln != std::string::npos && ln >= s.start) {
            bool ok = true;
            for (size_t i = ln + 1; i < l.first; i++) if (!isWs(t[i])) ok = false;
            if (ok) { lind = t.substr(ln + 1, l.first - ln - 1); break; }
        }
    }
    std::string body = nl;
    for (auto& l : layers) body += lind + l + nl;
    body += ind;
    std::string head = t.substr(s.start, s.body - s.start);
    size_t a = head.find("NumLayers=\"");
    if (a != std::string::npos) {
        a += 11;
        size_t b = head.find('"', a);
        head = head.substr(0, a) + std::to_string(layers.size()) + head.substr(b);
    }
    return t.substr(0, s.start) + head + body + t.substr(s.end);
}

std::string emptyPhysics() {
    std::string s = "<CameraPhysics>\n";
    for (auto& sec : sections()) s += std::string("  <") + sec.name + " NumLayers=\"0\">\n  </" + sec.name + ">\n";
    return s + "</CameraPhysics>\n";
}

// ---------------------------------------------------------------------------
// Edits
std::string writeEdits(const Edits& e) {
    std::string s = "FHCE-EDITS 1\n";
    for (auto& kv : e.ini) s += "ini " + kv.first + " " + kv.second + "\n";
    for (auto& kv : e.sections) {
        s += "section " + kv.first + "\n";
        for (auto& l : kv.second) s += l + "\n";
        s += "end\n";
    }
    return s;
}

bool readEdits(const std::string& text, Edits& e) {
    e = Edits();
    std::istringstream in(text);
    std::string line, cur;
    bool inSec = false, ok = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!ok) { if (line.rfind("FHCE-EDITS", 0) == 0) ok = true; continue; }
        if (inSec) {
            if (line == "end") inSec = false;
            else if (line.find("<Layer") != std::string::npos) e.sections[cur].push_back(line);
            continue;
        }
        if (line.rfind("ini ", 0) == 0) {
            size_t sp = line.find(' ', 4);
            if (sp != std::string::npos && sp + 1 < line.size()) e.ini[line.substr(4, sp - 4)] = line.substr(sp + 1);
        } else if (line.rfind("section ", 0) == 0) {
            cur = line.substr(8);
            inSec = true;
            e.sections[cur]; // an empty section is a valid edit (all layers deleted)
        }
    }
    return ok;
}

void applyEdits(const std::string& ini, const std::string& xml, const Edits& e, std::string& oi, std::string& ox) {
    oi = ini;
    for (auto& kv : e.ini) oi = iniSet(oi, kv.first, kv.second);
    ox = xml;
    if (e.sections.empty()) return;
    auto secs = xmlSections(ox);
    for (auto& kv : e.sections)
        if (findSection(secs, kv.first)) ox = replaceSectionLayers(ox, kv.first, kv.second);
}

Built buildAll(const std::string& ini, const std::string& xml, const Edits& e, const CamParams& p) {
    std::string i2, x2;
    applyEdits(ini, xml, e, i2, x2);
    return build(i2, x2, p);
}

} // namespace fh6
