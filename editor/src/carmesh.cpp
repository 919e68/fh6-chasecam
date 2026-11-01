#include "carmesh.hpp"
#include <algorithm>
#include <cmath>

namespace fh6 {

namespace {
const double PI = 3.14159265358979323846;
const double L = 2.25;                          // half length
const double WHEEL_Z[2] = { -1.36, 1.36 }, WHEEL_R = 0.33, ARCH_R = 0.42;

double clamp01(double x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
double smooth(double a, double b, double x) { double t = clamp01((x - a) / (b - a)); return t * t * (3 - 2 * t); }

// Plan view: half width, with rounded ends, a slightly narrower nose and fender flares
double halfWidth(double z) {
    double w = 0.92 * (1 - 0.17 * smooth(1.55, L, std::fabs(z))) * (1 - 0.04 * smooth(0.4, L, z));
    for (double wz : WHEEL_Z) w += 0.035 * std::exp(-std::pow((z - wz) / 0.5, 2));
    return w;
}
// Belt (shoulder) line: drops toward the nose, kicks up a little at the rear, drops at the tail
double belt(double z) {
    return 0.80 - 0.15 * std::pow(smooth(0.9, L, z), 1.4) + 0.035 * smooth(-0.3, -1.9, z) - 0.07 * smooth(-1.95, -L, z);
}
// Floor: rises at the overhangs
double floorY(double z) { return 0.17 + 0.11 * smooth(1.65, L, z) + 0.09 * smooth(-1.7, -L, z); }
// Wheel arch: height of the arch edge above the ground at z (0 outside the arches)
double archY(double z) {
    double y = 0;
    for (double wz : WHEEL_Z) {
        double d = std::fabs(z - wz);
        if (d < ARCH_R) y = std::max(y, WHEEL_R + std::sqrt(ARCH_R * ARCH_R - d * d) * 0.92);
    }
    return y;
}
// Cabin height above the deck: raked windshield, slightly crowned roof, fastback
double cabin(double z) {
    const double wsBase = 0.98, roofFront = 0.12, roofRear = -0.72, rearBase = -1.62, roofH = 0.47;
    if (z >= wsBase || z <= rearBase) return 0;
    if (z > roofFront) return roofH * (1 - std::pow((z - roofFront) / (wsBase - roofFront), 1.25));
    if (z < roofRear) return roofH * (1 - std::pow((roofRear - z) / (roofRear - rearBase), 1.15));
    double m = (z - (roofFront + roofRear) / 2) / ((roofFront - roofRear) / 2);
    return roofH * (1 - 0.035 * m * m);
}

struct Builder {
    CarMesh m;
    int v(const V3& p) { m.verts.push_back(p); return (int)m.verts.size() - 1; }
    void quad(int a, int b, int c, int d, int part, const V3& in) { m.faces.push_back({ { a, b, c, d }, 4, part }); m.inside.push_back(in); }
    void tri(int a, int b, int c, int part, const V3& in) { m.faces.push_back({ { a, b, c, 0 }, 3, part }); m.inside.push_back(in); }
    void box(V3 c, V3 h, int part) { // axis-aligned box: centre, half sizes
        int i[8];
        for (int k = 0; k < 8; k++) i[k] = v(V3(c.x + (k & 1 ? h.x : -h.x), c.y + (k & 2 ? h.y : -h.y), c.z + (k & 4 ? h.z : -h.z)));
        int f[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
        for (auto& q : f) quad(i[q[0]], i[q[1]], i[q[2]], i[q[3]], part, c);
    }
};

// One half section from the bottom centre to the top centre (11 points).
void halfSection(double z, V3* p) {
    double w = halfWidth(z), b = belt(z), fl = floorY(z), h = cabin(z), arch = archY(z);
    double deck = b + 0.04;
    double low = std::max(fl + 0.10, arch), bot = std::max(fl, arch); // the arch lifts the outer lower edge
    double wg = w * 0.80, wr = w * 0.63;
    double crown = h > 0.01 ? 0.02 : 0.03;
    p[0] = V3(0, fl, z);
    p[1] = V3(0.80 * w, bot, z);
    p[2] = V3(0.95 * w, low, z);
    p[3] = V3(w, std::max(0.47, low + 0.02), z);
    p[4] = V3(0.985 * w, std::max(b - 0.08, low + 0.04), z);
    p[5] = V3(0.95 * w, b, z);
    p[6] = V3(0.86 * w, b + 0.035, z);
    p[7] = V3(wg, deck, z);
    p[8] = V3(wr + (wg - wr) * 0.3, deck + h * 0.78, z);
    p[9] = V3(wr, deck + h, z);
    p[10] = V3(0, deck + h + crown, z);
}

CarMesh build() {
    Builder B;
    // body: rings of 20 points (centre bottom, right side up, centre top, left side down)
    const int NS = 40, NR = 20;
    std::vector<int> rings(NS * NR);
    std::vector<double> zs(NS);
    for (int s = 0; s < NS; s++) {
        double t = (double)s / (NS - 1);
        double z = -L + 2 * L * (0.5 - 0.5 * std::cos(PI * t)); // denser near the ends, where the shape turns
        zs[s] = z;
        V3 hp[11];
        halfSection(z, hp);
        int k = 0;
        for (int i = 0; i <= 10; i++) rings[s * NR + k++] = B.v(hp[i]);
        for (int i = 9; i >= 1; i--) rings[s * NR + k++] = B.v(V3(-hp[i].x, hp[i].y, hp[i].z));
    }
    for (int s = 0; s + 1 < NS; s++) {
        V3 in(0, 0.5, (zs[s] + zs[s + 1]) / 2);
        for (int k = 0; k < NR; k++) {
            if (k == 0 || k == NR - 1) continue; // no floor: only seen through the shell, and it clutters the arches
            B.quad(rings[s * NR + k], rings[s * NR + (k + 1) % NR], rings[(s + 1) * NR + (k + 1) % NR], rings[(s + 1) * NR + k], 0, in);
        }
    }
    // rounded bumpers: a few rings shrinking toward the centre of each end, then a small cap
    for (int e = 0; e < 2; e++) {
        int s = e ? NS - 1 : 0;
        double dir = e ? 1 : -1;
        V3 c(0, 0, 0);
        for (int k = 0; k < NR; k++) c = c + B.m.verts[rings[s * NR + k]] * (1.0 / NR);
        const double scale[3] = { 0.82, 0.58, 0.30 }, push[3] = { 0.035, 0.06, 0.075 };
        std::vector<int> prev(rings.begin() + s * NR, rings.begin() + s * NR + NR);
        V3 in(0, 0.5, zs[s] - dir * 0.3);
        for (int r = 0; r < 3; r++) {
            std::vector<int> cur(NR);
            for (int k = 0; k < NR; k++) {
                V3 p = B.m.verts[prev[k] == rings[s * NR + k] ? prev[k] : rings[s * NR + k]];
                V3 q = c + (p - c) * scale[r];
                q.z = zs[s] + dir * push[r];
                cur[k] = B.v(q);
            }
            for (int k = 0; k < NR; k++) B.quad(prev[k], prev[(k + 1) % NR], cur[(k + 1) % NR], cur[k], 0, in);
            prev = cur;
        }
        int ci = B.v(V3(c.x, c.y, zs[s] + dir * 0.08));
        for (int k = 0; k < NR; k++) B.tri(ci, prev[k], prev[(k + 1) % NR], 0, in);
    }
    // wheels: 24-sided tyre with sidewalls and a spoked rim on the outer face
    const int WS = 24;
    for (int sx = -1; sx <= 1; sx += 2)
        for (double wz : WHEEL_Z) {
            double xo = sx * (halfWidth(wz) - 0.06), xi = xo - sx * 0.24, rr = 0.22;
            V3 hub(xo - sx * 0.04, WHEEL_R, wz), wc((xo + xi) / 2, WHEEL_R, wz);
            int to[WS], ti[WS], ro[WS], ri[WS];
            for (int k = 0; k < WS; k++) {
                double a = 2 * PI * k / WS, cy = std::cos(a), sz = std::sin(a);
                to[k] = B.v(V3(xo, WHEEL_R + WHEEL_R * cy, wz + WHEEL_R * sz));
                ti[k] = B.v(V3(xi, WHEEL_R + WHEEL_R * cy, wz + WHEEL_R * sz));
                ro[k] = B.v(V3(xo - sx * 0.03, WHEEL_R + rr * cy, wz + rr * sz));
                ri[k] = B.v(V3(xi, WHEEL_R + rr * cy, wz + rr * sz));
            }
            int h = B.v(hub);
            for (int k = 0; k < WS; k++) {
                int n = (k + 1) % WS;
                B.quad(to[k], to[n], ti[n], ti[k], 1, wc);   // tread
                B.quad(to[k], ro[k], ro[n], to[n], 1, wc);   // outer sidewall
                B.quad(ti[k], ti[n], ri[n], ri[k], 1, wc);   // inner sidewall
                B.tri(h, ro[k], ro[n], 1, V3(wc.x, WHEEL_R, wz)); // rim
            }
        }
    // mirrors and a rear wing on two stands
    for (int sx = -1; sx <= 1; sx += 2) {
        double z = 0.82, w = halfWidth(z);
        B.box(V3(sx * (w + 0.06), belt(z) + 0.12, z), V3(0.07, 0.045, 0.05), 2);
        B.box(V3(sx * 0.55, belt(-1.95) + 0.19, -1.98), V3(0.015, 0.15, 0.05), 2);
    }
    B.box(V3(0, belt(-1.95) + 0.36, -2.0), V3(0.84, 0.018, 0.15), 2);
    return B.m;
}
} // namespace

const CarMesh& carMesh() {
    static const CarMesh m = build();
    return m;
}

} // namespace fh6
