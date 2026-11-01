#include "preview.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

using fh6::V3;

namespace ui {

namespace {
struct View {
    V3 pos, right, up, fwd;
    float cx, cy, f;
    double nearZ = 0.15;
    V3 toCam(const V3& p) const { V3 d = p - pos; return { d.dot(right), d.dot(up), d.dot(fwd) }; }
    ImVec2 project(const V3& c) const { return ImVec2(cx + (float)(c.x / c.z) * f, cy - (float)(c.y / c.z) * f); }
};

// world line, clipped against the near plane
void line3(ImDrawList* dl, const View& v, V3 a, V3 b, ImU32 col, float th) {
    V3 ca = v.toCam(a), cb = v.toCam(b);
    if (ca.z < v.nearZ && cb.z < v.nearZ) return;
    if (ca.z < v.nearZ) ca = ca + (cb - ca) * ((v.nearZ - ca.z) / (cb.z - ca.z));
    if (cb.z < v.nearZ) cb = cb + (ca - cb) * ((v.nearZ - cb.z) / (ca.z - cb.z));
    dl->AddLine(v.project(ca), v.project(cb), col, th);
}

ImU32 mix(unsigned a, unsigned b, float t, int alpha = 255) {
    auto ch = [&](int s) { return (int)(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t); };
    return IM_COL32(ch(16), ch(8), ch(0), alpha);
}

struct Face { V3 c[4]; V3 n; unsigned color; float shade; double depth; };
} // namespace

void drawPreview(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec2 fa, ImVec2 fb, const fh6::CamPose& cam, const fh6::Inputs& in, const fh6::Scene& scene) {
    const float S = ui::S;
    View v;
    v.pos = cam.pos;
    v.fwd = cam.dir;
    v.right = cam.up.cross(cam.dir).norm();
    v.up = cam.dir.cross(v.right).norm();
    v.cx = (fa.x + fb.x) * 0.5f;
    v.cy = (fa.y + fb.y) * 0.5f;
    v.f = (fb.y - fa.y) * 0.5f / (float)std::tan(cam.fov * 3.14159265 / 360);

    dl->PushClipRect(a, b, true);
    // sky to ground: dusk over the hills, asphalt below
    dl->AddRectFilled(a, b, rgb(BG100)); // flat grounds, no gradients
    // horizon (follows camera pitch and roll)
    {
        V3 h0 = v.pos + V3(v.fwd.x, 0, v.fwd.z).norm() * 1000.0 - v.right * 3000.0;
        V3 h1 = v.pos + V3(v.fwd.x, 0, v.fwd.z).norm() * 1000.0 + v.right * 3000.0;
        h0.y = h1.y = v.pos.y;
        V3 c0 = v.toCam(h0), c1 = v.toCam(h1);
        if (c0.z > 1 && c1.z > 1) {
            ImVec2 p0 = v.project(c0), p1 = v.project(c1);
            // fill below the horizon with the road colour
            ImVec2 dn(-(p1.y - p0.y), p1.x - p0.x);
            float l = std::sqrt(dn.x * dn.x + dn.y * dn.y);
            if (l > 0) { dn.x /= l; dn.y /= l; if (dn.y < 0) { dn.x = -dn.x; dn.y = -dn.y; } }
            float big = 4000;
            ImVec2 q[4] = { p0, p1, ImVec2(p1.x + dn.x * big, p1.y + dn.y * big), ImVec2(p0.x + dn.x * big, p0.y + dn.y * big) };
            dl->AddConvexPolyFilled(q, 4, rgb(BG000));
            dl->AddLine(p0, p1, rgb(LINE), 1.f * S);
        }
    }

    double gy = in.pos.y - 0.02; // ground under the car
    // ground grid, fading into the dark
    const double step = 4, range = 64;
    double ox = std::floor(in.pos.x / step) * step, oz = std::floor(in.pos.z / step) * step;
    for (double k = -range; k <= range; k += step) {
        for (int axis = 0; axis < 2; axis++) {
            for (double s0 = -range; s0 < range; s0 += step) {
                V3 p0 = axis == 0 ? V3(ox + k, gy, oz + s0) : V3(ox + s0, gy, oz + k);
                V3 p1 = axis == 0 ? V3(ox + k, gy, oz + s0 + step) : V3(ox + s0 + step, gy, oz + k);
                V3 mid = (p0 + p1) * 0.5;
                double d = std::hypot(mid.x - in.pos.x, mid.z - in.pos.z);
                float fade = (float)std::clamp(1 - d / range, 0.0, 1.0);
                if (fade <= 0) continue;
                line3(dl, v, p0, p1, rgb(LINE_STRONG, (int)(110 * fade * fade)), 1.f * S);
            }
        }
    }

    // skid marks
    for (auto* q : { &scene.skidL, &scene.skidR }) {
        for (size_t i = 1; i < q->size(); i++) {
            double s = std::min((*q)[i].second, (*q)[i - 1].second);
            if (s <= 0.02) continue;
            float age = (float)(q->size() - i) / (float)q->size();
            V3 p0 = (*q)[i - 1].first, p1 = (*q)[i].first;
            p0.y = p1.y = gy;
            line3(dl, v, p0, p1, IM_COL32(0, 0, 0, (int)(220 * s * (1 - age))), 5.f * S);
        }
    }

    // smoke behind the car (drawn before the car when it is further away)
    auto drawSmoke = [&](bool behind) {
        V3 cc = v.toCam(in.pos);
        for (auto& sm : scene.smoke) {
            V3 c = v.toCam(sm.p);
            if (c.z < 0.5 || (c.z > cc.z) != behind) continue;
            float life = (float)(sm.age / sm.life);
            float r = (float)(sm.size / c.z) * v.f;
            // fade puffs the camera is about to pass through
            float nearFade = (float)std::clamp((c.z - 1.5) / 3.0, 0.0, 1.0);
            int alpha = (int)(46 * (1 - life) * std::min(1.f, life * 6) * nearFade);
            if (alpha <= 0) continue;
            // soft puff: wide faint halo, denser core
            ImVec2 sc = v.project(c);
            dl->AddCircleFilled(sc, r, rgb(INK_MUTED, alpha / 2), 24);
            dl->AddCircleFilled(sc, r * 0.62f, rgb(INK_MUTED, alpha), 20);
        }
    };
    drawSmoke(true);

    // car: body and cabin as shaded boxes, painter's order
    V3 fwd(std::sin(in.heading), 0, std::cos(in.heading));
    V3 right(fwd.z, 0, -fwd.x);
    V3 up(0, 1, 0);
    double rl = in.bodyRoll; // lean the body a little with cornering
    V3 upR = (up * std::cos(rl) - right * std::sin(rl)).norm();
    V3 rightR = upR.cross(fwd).norm(); // x right, y up, z forward
    std::vector<Face> faces;
    auto box = [&](double cx, double cz, double y0, double y1, double hw, double hl, double topShrinkW, double topShrinkL, unsigned col) {
        V3 o = in.pos + V3(0, gy - in.pos.y, 0);
        auto P = [&](double sx, double sz, double y) {
            double w = hw - (y == y1 ? topShrinkW : 0), l = hl - (y == y1 ? topShrinkL : 0);
            return o + rightR * (cx + sx * w) + fwd * (cz + sz * l) + upR * y;
        };
        V3 c[8] = { P(-1, -1, y0), P(1, -1, y0), P(1, 1, y0), P(-1, 1, y0), P(-1, -1, y1), P(1, -1, y1), P(1, 1, y1), P(-1, 1, y1) };
        int idx[6][4] = { { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 1, 2, 6, 5 }, { 2, 3, 7, 6 }, { 3, 0, 4, 7 }, { 0, 3, 2, 1 } };
        V3 bc = o + rightR * cx + fwd * cz + upR * ((y0 + y1) / 2);
        for (auto& f : idx) {
            Face fc;
            for (int i = 0; i < 4; i++) fc.c[i] = c[f[i]];
            fc.n = (fc.c[1] - fc.c[0]).cross(fc.c[2] - fc.c[1]).norm();
            V3 ctr = (fc.c[0] + fc.c[1] + fc.c[2] + fc.c[3]) * 0.25;
            if (fc.n.dot(ctr - bc) < 0) fc.n = fc.n * -1.0; // outward
            fc.color = col;
            V3 light = V3(-0.4, 1, -0.3).norm();
            fc.shade = (float)std::clamp(0.35 + 0.65 * std::max(0.0, fc.n.dot(light)), 0.0, 1.0);
            fc.depth = v.toCam(ctr).z;
            if (fc.n.dot(v.pos - ctr) > 0) faces.push_back(fc); // facing the camera
        }
    };
    box(0, 0, 0.22, 0.72, 0.93, 2.25, 0.06, 0.18, INK);            // body
    box(0, -0.2, 0.72, 1.20, 0.80, 1.10, 0.15, 0.48, BG300);      // cabin and glass, raked
    // wheels first (tucked under the body, only their lower part shows), then the body
    std::vector<Face> body;
    body.swap(faces);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) box(sx * 0.80, sz * 1.38, 0.0, 0.62, 0.12, 0.32, 0, 0, BG000);
    auto byDepth = [](const Face& x, const Face& y) { return x.depth > y.depth; };
    std::sort(faces.begin(), faces.end(), byDepth);
    std::sort(body.begin(), body.end(), byDepth);
    faces.insert(faces.end(), body.begin(), body.end());
    for (auto& fc : faces) {
        ImVec2 q[4];
        bool ok = true;
        for (int i = 0; i < 4; i++) {
            V3 c = v.toCam(fc.c[i]);
            if (c.z < v.nearZ) { ok = false; break; }
            q[i] = v.project(c);
        }
        if (!ok) continue;
        unsigned col = fc.color;
        ImU32 fill = mix(BG000, col, fc.shade);
        dl->AddConvexPolyFilled(q, 4, fill);
        dl->AddPolyline(q, 4, rgb(BG000, 160), 1.f * S, ImDrawFlags_Closed);
    }
    // tail lights and a sakura stripe on the rear face
    {
        V3 o = in.pos + V3(0, gy - in.pos.y, 0);
        auto R = [&](double x, double y) { return o + rightR * x + fwd * -2.255 + upR * y; };
        if (fwd.dot(v.pos - in.pos) < 0) { // rear visible
            for (int s = -1; s <= 1; s += 2) {
                V3 c[4] = { R(s * 0.82, 0.62), R(s * 0.45, 0.62), R(s * 0.45, 0.72), R(s * 0.82, 0.72) };
                ImVec2 q[4];
                bool ok = true;
                for (int i = 0; i < 4; i++) { V3 cc = v.toCam(c[i]); if (cc.z < v.nearZ) ok = false; else q[i] = v.project(cc); }
                if (ok) {
                    dl->AddConvexPolyFilled(q, 4, rgb(DANGER));
                    ImVec2 m((q[0].x + q[2].x) / 2, (q[0].y + q[2].y) / 2);
                    dl->AddCircleFilled(m, 10 * S, rgb(DANGER, 40), 20);
                }
            }
        }
    }
    drawSmoke(false);
    // outside the game's 16:9 frame: dimmed, with a thin frame edge
    ImU32 dim = glass(60);
    if (fa.x > a.x) { dl->AddRectFilled(a, ImVec2(fa.x, b.y), dim); dl->AddRectFilled(ImVec2(fb.x, a.y), b, dim); }
    if (fa.y > a.y) { dl->AddRectFilled(a, ImVec2(b.x, fa.y), dim); dl->AddRectFilled(ImVec2(a.x, fb.y), b, dim); }
    dl->AddRect(ImVec2(std::max(fa.x, a.x), std::max(fa.y, a.y)), ImVec2(std::min(fb.x, b.x), std::min(fb.y, b.y)), GLASS_LINE, 0, 1.f * S);
    dl->PopClipRect();
}

} // namespace ui
