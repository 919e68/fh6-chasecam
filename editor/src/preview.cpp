#include "preview.hpp"
#include "carmesh.hpp"
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

struct Face { V3 c[8]; int count; V3 n; float shade; double depth; bool front; };
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

    // car: the generated high-poly coupe (carmesh.cpp), see-through white with
    // yellow edge lines. All faces back to front, so the far side shows faintly
    // through the near side.
    V3 fwd(std::sin(in.heading), 0, std::cos(in.heading));
    V3 right(fwd.z, 0, -fwd.x);
    V3 up(0, 1, 0);
    double rl = in.bodyRoll; // lean the body a little with cornering
    V3 upR = (up * std::cos(rl) - right * std::sin(rl)).norm();
    V3 rightR = upR.cross(fwd).norm(); // x right, y up, z forward
    V3 o = in.pos + V3(0, gy - in.pos.y, 0);
    auto P = [&](const V3& p) { return o + rightR * p.x + upR * p.y + fwd * p.z; };
    const V3 light = V3(-0.35, 1.0, -0.25).norm();

    // soft contact shadow on the ground
    {
        auto ground = [&](double x, double z) { V3 g = P(V3(x, 0, z)); g.y = gy; return g; };
        for (int pass = 0; pass < 2; pass++) {
            double ex = pass ? 1.15 : 0.98, ez = pass ? 2.55 : 2.35;
            V3 c4[4] = { ground(-ex, -ez), ground(ex, -ez), ground(ex, ez), ground(-ex, ez) };
            ImVec2 q[4];
            bool ok = true;
            for (int i = 0; i < 4; i++) { V3 c = v.toCam(c4[i]); if (c.z < v.nearZ) ok = false; else q[i] = v.project(c); }
            if (ok) dl->AddConvexPolyFilled(q, 4, IM_COL32(0, 0, 0, pass ? 70 : 120));
        }
    }

    const fh6::CarMesh& mesh = fh6::carMesh();
    static std::vector<V3> world, camv;
    static std::vector<Face> faces;
    world.resize(mesh.verts.size());
    camv.resize(mesh.verts.size());
    for (size_t i = 0; i < mesh.verts.size(); i++) { world[i] = P(mesh.verts[i]); camv[i] = v.toCam(world[i]); }
    faces.clear();
    for (size_t fi = 0; fi < mesh.faces.size(); fi++) {
        const auto& mf = mesh.faces[fi];
        Face f;
        f.count = mf.n;
        V3 ctr, nrm;
        for (int i = 0; i < mf.n; i++) {
            const V3& p0 = world[mf.idx[i]];
            const V3& p1 = world[mf.idx[(i + 1) % mf.n]];
            f.c[i] = camv[mf.idx[i]]; // camera space
            ctr = ctr + p0 * (1.0 / mf.n);
            nrm = nrm + V3((p0.y - p1.y) * (p0.z + p1.z), (p0.z - p1.z) * (p0.x + p1.x), (p0.x - p1.x) * (p0.y + p1.y)); // Newell
        }
        if (nrm.len() < 1e-12) continue;
        nrm = nrm.norm();
        if (nrm.dot(ctr - P(mesh.inside[fi])) < 0) nrm = nrm * -1.0; // outward
        f.front = nrm.dot(v.pos - ctr) > 0;
        f.n = nrm;
        f.shade = (float)std::clamp(0.30 + 0.70 * std::max(0.0, nrm.dot(light)), 0.0, 1.0);
        f.depth = v.toCam(ctr).z;
        faces.push_back(f);
    }
    std::sort(faces.begin(), faces.end(), [](const Face& x, const Face& y) { return x.depth > y.depth; });
    for (auto& fc : faces) {
        ImVec2 q[8];
        bool ok = true;
        for (int i = 0; i < fc.count; i++) {
            if (fc.c[i].z < v.nearZ) { ok = false; break; }
            q[i] = v.project(fc.c[i]);
        }
        if (!ok) continue;
        // semi-transparent white; yellow edges kept light because every edge is
        // shared by two faces and the mesh is dense
        ImU32 fill = mix(0x5E646E, 0xF6F7F9, fc.front ? fc.shade : 0.35f, fc.front ? 120 : 30);
        dl->AddConvexPolyFilled(q, fc.count, fill);
        dl->AddPolyline(q, fc.count, rgb(0xFFD43B, fc.front ? 85 : 22), 1.f * S, ImDrawFlags_Closed);
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
