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

struct Face { V3 c[8]; int count; V3 n; float shade; double depth; };
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

    // car: a white low-poly coupe lofted through cross-sections along its
    // length, flat shaded; wheels first (tucked under the body), then the body
    V3 fwd(std::sin(in.heading), 0, std::cos(in.heading));
    V3 right(fwd.z, 0, -fwd.x);
    V3 up(0, 1, 0);
    double rl = in.bodyRoll; // lean the body a little with cornering
    V3 upR = (up * std::cos(rl) - right * std::sin(rl)).norm();
    V3 rightR = upR.cross(fwd).norm(); // x right, y up, z forward
    V3 o = in.pos + V3(0, gy - in.pos.y, 0);
    auto P = [&](double x, double y, double z) { return o + rightR * x + upR * y + fwd * z; };
    const V3 light = V3(-0.35, 1.0, -0.25).norm();

    auto addFace = [&](std::vector<Face>& out, const V3* pts, int n, const V3& inside) {
        Face f;
        f.count = n;
        V3 ctr, nrm;
        for (int i = 0; i < n; i++) {
            f.c[i] = pts[i];
            ctr = ctr + pts[i] * (1.0 / n);
            const V3 &p0 = pts[i], &p1 = pts[(i + 1) % n]; // Newell normal
            nrm = nrm + V3((p0.y - p1.y) * (p0.z + p1.z), (p0.z - p1.z) * (p0.x + p1.x), (p0.x - p1.x) * (p0.y + p1.y));
        }
        if (nrm.len() < 1e-9) return;
        nrm = nrm.norm();
        if (nrm.dot(ctr - inside) < 0) nrm = nrm * -1.0; // outward
        if (nrm.dot(v.pos - ctr) <= 0) return;           // facing away
        f.n = nrm;
        f.shade = (float)std::clamp(0.30 + 0.70 * std::max(0.0, nrm.dot(light)), 0.0, 1.0);
        f.depth = v.toCam(ctr).z;
        out.push_back(f);
    };

    // cross-sections, rear to front: z, then heights (bottom, widest, belt, top)
    // and half widths at those heights. Belt to top is the glass over the cabin.
    struct Sec { double z, yb, ys, ybelt, ytop, wb, ws, wbelt, wtop; };
    static const Sec SECS[] = {
        { -2.25, 0.25, 0.48, 0.70, 0.74, 0.76, 0.85, 0.83, 0.70 }, // rear bumper
        { -2.07, 0.19, 0.50, 0.80, 0.85, 0.84, 0.92, 0.90, 0.78 }, // trunk lip
        { -1.45, 0.18, 0.50, 0.82, 0.88, 0.85, 0.93, 0.90, 0.74 }, // base of the rear window
        { -0.70, 0.18, 0.50, 0.84, 1.27, 0.85, 0.93, 0.88, 0.60 }, // roof, rear
        { 0.35, 0.18, 0.50, 0.84, 1.29, 0.85, 0.93, 0.88, 0.60 },  // roof, front
        { 1.05, 0.18, 0.50, 0.81, 0.87, 0.85, 0.93, 0.89, 0.74 },  // base of the windshield
        { 2.02, 0.20, 0.47, 0.69, 0.74, 0.82, 0.90, 0.86, 0.72 },  // nose
        { 2.25, 0.27, 0.45, 0.59, 0.62, 0.72, 0.81, 0.79, 0.65 },  // front bumper
    };
    const int NS = (int)(sizeof SECS / sizeof SECS[0]);
    auto ring = [&](const Sec& c, V3* r) {
        r[0] = P(c.wb, c.yb, c.z);       r[1] = P(c.ws, c.ys, c.z);
        r[2] = P(c.wbelt, c.ybelt, c.z); r[3] = P(c.wtop, c.ytop, c.z);
        r[4] = P(-c.wtop, c.ytop, c.z);  r[5] = P(-c.wbelt, c.ybelt, c.z);
        r[6] = P(-c.ws, c.ys, c.z);      r[7] = P(-c.wb, c.yb, c.z);
    };
    std::vector<Face> wheels, body;
    V3 rings[16][8];
    for (int i = 0; i < NS; i++) ring(SECS[i], rings[i]);
    for (int i = 0; i + 1 < NS; i++) {
        V3 inside = P(0, 0.55, (SECS[i].z + SECS[i + 1].z) / 2);
        for (int k = 0; k < 8; k++) {
            V3 q[4] = { rings[i][k], rings[i][(k + 1) % 8], rings[i + 1][(k + 1) % 8], rings[i + 1][k] };
            addFace(body, q, 4, inside);
        }
    }
    V3 center = P(0, 0.55, 0);
    addFace(body, rings[0], 8, center);
    addFace(body, rings[NS - 1], 8, center);
    // wheels: octagonal, the outer face just inside the body side
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            const double r = 0.33, cx = sx * 0.80, cz = sz * 1.38, hw = 0.12;
            V3 in8[8], out8[8];
            for (int k = 0; k < 8; k++) {
                double t = (k + 0.5) * 3.14159265 / 4;
                double yy = r + r * std::cos(t), zz = cz + r * std::sin(t);
                in8[k] = P(cx - hw, yy, zz);
                out8[k] = P(cx + hw, yy, zz);
            }
            V3 wc = P(cx, r, cz);
            for (int k = 0; k < 8; k++) {
                V3 q[4] = { in8[k], in8[(k + 1) % 8], out8[(k + 1) % 8], out8[k] };
                addFace(wheels, q, 4, wc);
            }
            addFace(wheels, in8, 8, wc);
            addFace(wheels, out8, 8, wc);
        }

    // soft contact shadow on the ground
    {
        auto ground = [&](double x, double z) { V3 g = P(x, 0, z); g.y = gy; return g; };
        for (int pass = 0; pass < 2; pass++) {
            double ex = pass ? 1.15 : 0.98, ez = pass ? 2.55 : 2.35;
            V3 c4[4] = { ground(-ex, -ez), ground(ex, -ez), ground(ex, ez), ground(-ex, ez) };
            ImVec2 q[4];
            bool ok = true;
            for (int i = 0; i < 4; i++) { V3 c = v.toCam(c4[i]); if (c.z < v.nearZ) ok = false; else q[i] = v.project(c); }
            if (ok) dl->AddConvexPolyFilled(q, 4, IM_COL32(0, 0, 0, pass ? 70 : 120));
        }
    }

    auto byDepth = [](const Face& x, const Face& y) { return x.depth > y.depth; };
    std::sort(wheels.begin(), wheels.end(), byDepth);
    std::sort(body.begin(), body.end(), byDepth);
    for (auto* list : { &wheels, &body })
        for (auto& fc : *list) {
            ImVec2 q[8];
            bool ok = true;
            for (int i = 0; i < fc.count; i++) {
                V3 c = v.toCam(fc.c[i]);
                if (c.z < v.nearZ) { ok = false; break; }
                q[i] = v.project(c);
            }
            if (!ok) continue;
            ImU32 fill = mix(0x5E646E, 0xF6F7F9, fc.shade); // white paint, shaded
            dl->AddConvexPolyFilled(q, fc.count, fill);
            dl->AddPolyline(q, fc.count, fill, 1.f * S, ImDrawFlags_Closed); // hides seams between faces
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
