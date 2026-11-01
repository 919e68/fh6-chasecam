#include "app.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "preview.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace ui;
static const char* VERSION = "0.1.0";
static const double PI = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
static bool readPath(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}
static bool writePath(const fs::path& p, const std::string& data) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size());
    return (bool)f;
}
static std::string u8(const fs::path& p) { return p.u8string(); }
static fs::path fromU8(const std::string& s) { return fs::u8path(s); }

static fs::path settingsFile() { return exeDir() / "fh6chasecam-editor.ini"; }

void App::loadSettings() {
    std::string t;
    if (!readPath(settingsFile(), t)) return;
    std::istringstream s(t);
    std::string line;
    while (std::getline(s, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        double d = std::atof(v.c_str());
        if (k == "gameDir") gameDir = fromU8(v);
        else if (k == "stockDir") stockDir = fromU8(v);
        else if (k == "port") port = std::clamp((int)d, 1024, 65535);
        else if (k == "followTravel") followTravel = d != 0;
        else if (k == "slot") p.slot = d != 0;
        else if (k == "heightOfs") p.heightOfs = d;
        else if (k == "targetOfs") p.targetOfs = d;
        else if (k == "radiusOfs") p.radiusOfs = d;
        else if (k == "fovAdd") p.fovAdd = d;
        else if (k == "maxFovAdd") p.maxFovAdd = d;
        else if (k == "follow") p.follow = d;
        else if (k == "cornerAim") p.cornerAim = d;
        else if (k == "driftTilt") p.driftTilt = d;
        else if (k == "speedFov") p.speedFov = d;
        else if (k == "driftSmooth") p.driftSmooth = d;
        else if (k == "flipAim") p.flipAim = d != 0;
        else if (k == "flipTilt") p.flipTilt = d != 0;
    }
}

void App::saveSettings() {
    std::ostringstream s;
    s << "gameDir=" << u8(gameDir) << "\nstockDir=" << u8(stockDir) << "\nport=" << port
      << "\nfollowTravel=" << followTravel << "\nslot=" << p.slot << "\nheightOfs=" << p.heightOfs
      << "\ntargetOfs=" << p.targetOfs << "\nradiusOfs=" << p.radiusOfs << "\nfovAdd=" << p.fovAdd
      << "\nmaxFovAdd=" << p.maxFovAdd << "\nfollow=" << p.follow << "\ncornerAim=" << p.cornerAim
      << "\ndriftTilt=" << p.driftTilt << "\nspeedFov=" << p.speedFov << "\ndriftSmooth=" << p.driftSmooth
      << "\nflipAim=" << p.flipAim << "\nflipTilt=" << p.flipTilt << "\n";
    writePath(settingsFile(), s.str());
}

void App::changed() {
    dirty = true;
    settingsSaveAt = fh6::nowSec() + 0.6;
}

void App::say(const std::string& s, unsigned color) {
    msg = s;
    msgColor = color;
    msgTime = fh6::nowSec();
}

// The game's own camera files: CameraPhysics.xml with CameraSettings.ini next
// to it, anywhere in the game folder except our output and backup folders.
void App::findStock() {
    stockNote.clear();
    if (stockOk()) return;
    stockDir.clear();
    if (gameDir.empty()) return;
    std::error_code ec;
    fs::path out = outDir();
    for (auto it = fs::recursive_directory_iterator(gameDir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it.depth() > 5) { it.disable_recursion_pending(); continue; }
        const fs::path& f = it->path();
        if (it->is_directory(ec)) {
            if (fs::equivalent(f, out, ec) || f.filename() == "fh6chasecam-backup") it.disable_recursion_pending();
            continue;
        }
        if (f.filename() == "CameraPhysics.xml" && fs::exists(f.parent_path() / "CameraSettings.ini", ec)) {
            stockDir = f.parent_path();
            break;
        }
    }
    if (stockDir.empty()) stockNote = "The game's own camera files weren't found in this folder. Choose the folder that has them.";
}

bool App::stockOk() const {
    std::error_code ec;
    if (stockDir.empty()) return false;
    if (!fs::exists(stockDir / "CameraPhysics.xml", ec) || !fs::exists(stockDir / "CameraSettings.ini", ec)) return false;
    return !(fs::exists(outDir(), ec) && fs::equivalent(stockDir, outDir(), ec)); // never build from our own output
}

bool App::hasBackup() const {
    std::error_code ec;
    return !gameDir.empty() && fs::exists(outDir() / "fh6chasecam-backup", ec);
}

void App::saveToGame() {
    std::string ini, xml;
    if (!stockOk() || !readPath(stockDir / "CameraSettings.ini", ini) || !readPath(stockDir / "CameraPhysics.xml", xml)) {
        say("Can't read the game's camera files. Choose the folder that has them.", RED);
        return;
    }
    fh6::Built b;
    try {
        b = fh6::build(ini, xml, p);
    } catch (const std::exception& e) {
        say(std::string("These camera files don't look like FH6's: ") + e.what(), RED);
        return;
    }
    std::error_code ec;
    fs::path out = outDir(), bak = out / "fh6chasecam-backup";
    fs::create_directories(out, ec);
    if (!fs::exists(bak, ec)) {
        // first save: keep whatever camera files were there, so Undo can put them back
        fs::create_directories(bak, ec);
        for (const char* n : { "CameraSettings.ini", "CameraPhysics.xml" })
            if (fs::exists(out / n, ec)) fs::copy_file(out / n, bak / n, fs::copy_options::overwrite_existing, ec);
        writePath(bak / "README.txt", "FH6 ChaseCam keeps the camera files that were here before its first save.\r\n"
                                      "Undo save in the editor puts them back. Empty means there were none.\r\n");
    }
    if (!writePath(out / "CameraSettings.ini", b.ini) || !writePath(out / "CameraPhysics.xml", b.xml)) {
        say("Couldn't write to " + u8(out) + ". Check the folder isn't read-only.", RED);
        return;
    }
    dirty = false;
    say("Saved. Restart Forza Horizon 6 to load the new camera.", SAKURA);
}

void App::undoSave() {
    std::error_code ec;
    fs::path out = outDir(), bak = out / "fh6chasecam-backup";
    for (const char* n : { "CameraSettings.ini", "CameraPhysics.xml" }) {
        fs::remove(out / n, ec);
        if (fs::exists(bak / n, ec)) fs::copy_file(bak / n, out / n, fs::copy_options::overwrite_existing, ec);
    }
    fs::remove(bak / "CameraSettings.ini", ec);
    fs::remove(bak / "CameraPhysics.xml", ec);
    fs::remove(bak / "README.txt", ec);
    fs::remove(bak, ec);
    dirty = true;
    say("Camera files are back to how they were before your first save. Restart FH6 to load them.", SMOKE);
}

void App::checkSaved() {
    dirty = true;
    std::string ini, xml, oi, ox;
    if (!stockOk() || !readPath(stockDir / "CameraSettings.ini", ini) || !readPath(stockDir / "CameraPhysics.xml", xml)) return;
    if (!readPath(outDir() / "CameraSettings.ini", oi) || !readPath(outDir() / "CameraPhysics.xml", ox)) return;
    try {
        fh6::Built b = fh6::build(ini, xml, p);
        dirty = b.ini != oi || b.xml != ox;
    } catch (const std::exception&) {}
}

// ---------------------------------------------------------------------------
void App::init() {
    loadSettings();
    findStock();
    checkSaved();
    if (!rx.start(port)) say(rx.error(), AMBER);
}

void App::shutdown() {
    saveSettings();
    rx.stop();
}

void App::update(float dt) {
    fh6::Telemetry t;
    double age;
    bool nowLive = rx.latest(t, age) && age < 0.5 && t.raceOn;
    if (nowLive != live) { cam.reset(); tin.reset(); scene = fh6::Scene(); gTrail.clear(); }
    live = nowLive;
    fh6::Inputs next = live ? tin.update(t, dt) : demo.step(dt);
    if ((next.pos - in.pos).len() > 30) { cam.reset(); scene = fh6::Scene(); }
    in = next;
    scene.step(in, dt);
    pose = cam.step(p, in, dt, followTravel);
    gTrail.push_back({ (float)in.latG, (float)in.lonG });
    while (gTrail.size() > 90) gTrail.pop_front();
    if (settingsSaveAt > 0 && fh6::nowSec() > settingsSaveAt) { saveSettings(); settingsSaveAt = -1; }
}

// ---------------------------------------------------------------------------
void App::frame(float W, float H) {
    update(std::min(ImGui::GetIO().DeltaTime, 0.1f));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    float pad = 16 * S, gap = 12 * S;
    float topH = 60 * S, footH = 64 * S, leftW = 330 * S;
    ImGui::SetCursorPos(ImVec2(pad, 0));
    topBar(W - 2 * pad);
    float bodyH = H - topH - footH;
    ImGui::SetCursorPos(ImVec2(pad, topH));
    controls(leftW, bodyH);
    float rx0 = pad + leftW + gap, rw = W - rx0 - pad;
    float stripH = std::clamp(bodyH * 0.3f, 150 * S, 190 * S);
    ImGui::SetCursorPos(ImVec2(rx0, topH));
    previewCard(rw, bodyH - stripH - gap);
    ImGui::SetCursorPos(ImVec2(rx0, topH + bodyH - stripH));
    inputsStrip(rw, stripH);
    ImGui::SetCursorPos(ImVec2(pad, H - footH));
    footer(W - 2 * pad);
    ImGui::End();
}

void App::topBar(float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float h = 60 * S;
    // wordmark: a small camera-swing glyph and the name
    ImVec2 g(p0.x + 12 * S, p0.y + h / 2);
    dl->PathArcTo(g, 11 * S, (float)(PI * 0.75), (float)(PI * 2.0), 24);
    dl->PathStroke(rgb(SAKURA), 2.5f * S);
    dl->AddCircleFilled(ImVec2(g.x + 11 * S, g.y), 3.5f * S, rgb(SAKURA));
    dl->AddText(fonts.medium, 21 * S, ImVec2(p0.x + 34 * S, p0.y + h / 2 - 13 * S), rgb(SMOKE), "FH6 ChaseCam");
    float nw = fonts.medium->CalcTextSizeA(21 * S, 1e9f, 0, "FH6 ChaseCam").x;
    dl->AddText(fonts.regular, 14 * S, ImVec2(p0.x + 34 * S + nw + 10 * S, p0.y + h / 2 - 8 * S), rgb(DIM), "by SPIRE");

    // camera slot
    float segW = 230 * S;
    ImGui::SetCursorScreenPos(ImVec2(p0.x + w - segW, p0.y + (h - 32 * S) / 2));
    static const char* slots[] = { "Chase Near", "Chase Far" };
    ImGui::BeginGroup();
    if (segmented("slot", &p.slot, slots, 2, segW)) { cam.reset(); changed(); }
    ImGui::EndGroup();
    helpTooltip("Which in-game camera this replaces. The other one stays stock, so you can switch back with the camera button.");

    // status: live or demo
    char st[96];
    bool err = !rx.error().empty();
    if (live) std::snprintf(st, sizeof st, "Live from FH6, %.0f Hz", rx.rate());
    else if (err) std::snprintf(st, sizeof st, "Data Out port %d unavailable", port);
    else std::snprintf(st, sizeof st, "Demo drive, waiting for Data Out on port %d", port);
    float sw = fonts.regular->CalcTextSizeA(14 * S, 1e9f, 0, st).x;
    float sx = p0.x + w - segW - 24 * S - sw;
    unsigned dot = live ? SAKURA : err ? AMBER : DIM;
    dl->AddCircleFilled(ImVec2(sx - 12 * S, p0.y + h / 2), 4 * S, rgb(dot));
    dl->AddText(fonts.regular, 14 * S, ImVec2(sx, p0.y + h / 2 - 9 * S), rgb(live ? SMOKE : MUTED), st);
    ImGui::SetCursorScreenPos(ImVec2(sx - 20 * S, p0.y + h / 2 - 10 * S));
    ImGui::InvisibleButton("##status", ImVec2(sw + 20 * S, 20 * S));
    helpTooltip("In FH6: Settings > HUD and Gameplay > Data Out: On, IP 127.0.0.1, and this port. "
                "Without it the preview runs a demo drive.");
}

void App::controls(float w, float h) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * S, 14 * S));
    ImGui::BeginChild("##controls", ImVec2(w, h), ImGuiChildFlags_AlwaysUseWindowPadding);
    bool c = false;
    sectionTitle("Position");
    c |= slider("h", "Height", &p.heightOfs, -2, 2, 0.05, "%+.2f m", -0.7,
                "Camera height compared to the stock camera. Lower: closer to the road, more dramatic.");
    c |= slider("d", "Distance", &p.radiusOfs, -3, 3, 0.05, "%+.2f m", -0.5,
                "Distance behind the car compared to stock. Negative: closer.");
    c |= slider("t", "Aim height", &p.targetOfs, -2, 2, 0.05, "%+.2f m", -0.55,
                "Height of the point the camera looks at. Lower than the camera change: flatter view, more horizon.");
    sectionTitle("View");
    c |= slider("fov", "Field of view", &p.fovAdd, -15, 25, 1, "%+.0f\xC2\xB0", 6,
                "Added to the camera's stock field of view. Higher: wider, feels faster.");
    c |= slider("mfov", "Field of view limit", &p.maxFovAdd, 0, 40, 1, "%+.0f\xC2\xB0", 10,
                "Raises the highest field of view the camera may reach. Raise it with the two settings above.");
    c |= slider("sfov", "Widen with speed", &p.speedFov, 0, 20, 1, "%.0f\xC2\xB0", 6,
                "Extra field of view from about 65 to 320 km/h.");
    sectionTitle("Follow");
    c |= slider("fol", "Rotation follow", &p.follow, 0.3, 1.5, 0.05, "\xC3\x97%.2f", 0.8,
                "How tightly the camera turns with the car. Lower: looser, shows more of the car's side in a drift.");
    sectionTitle("Drift");
    c |= slider("aim", "Look into the corner", &p.cornerAim, 0, 20, 0.5, "%.1f\xC2\xB0", 6,
                "While drifting, turns the view toward the inside of the corner. The car sits toward the outside of the frame.");
    c |= slider("tilt", "Horizon tilt", &p.driftTilt, -10, 10, 0.5, "%+.1f\xC2\xB0", 3,
                "While drifting, tips the horizon. Positive: toward the outside of the corner. 0: level.");
    c |= slider("sm", "Drift response", &p.driftSmooth, 2, 40, 1, "%.0f", 10,
                "How fast the drift effects come in and go. Higher: snappier. Lower: smoother.");
    if (c) changed();

    sectionTitle("Preview");
    toggle("ft", "Game's Drift camera is on", &followTravel,
           "Match FH6's own Drift camera setting (HUD and Gameplay). It makes the camera follow your direction of travel.");
    sectionTitle("If the game turns the wrong way");
    bool f = false;
    f |= toggle("fa", "Flip look into the corner", &p.flipAim,
                "Use if the game turns the view to the outside of the corner. The preview always shows the intended direction.");
    f |= toggle("ftl", "Flip horizon tilt", &p.flipTilt, "Use if the game tilts the horizon the other way than the preview.");
    if (f) changed();

    sectionTitle("Data Out");
    ImGui::PushFont(fonts.regular, 14);
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(MUTED));
    ImGui::TextWrapped("In FH6: Settings > HUD and Gameplay > Data Out On, IP 127.0.0.1, port:");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    static int portEdit = 0;
    if (portEdit == 0) portEdit = port;
    ImGui::SetNextItemWidth(110 * S);
    ImGui::InputInt("##port", &portEdit, 0, 0);
    portEdit = std::clamp(portEdit, 1024, 65535);
    ImGui::SameLine();
    if (button("Listen", false, 0, portEdit != port || !rx.error().empty())) {
        if (portEdit >= 5200 && portEdit <= 5300) say("Ports 5200 to 5300 are used by FH6 itself. Pick another.", AMBER);
        else {
            port = portEdit;
            if (rx.start(port)) say("Listening for Data Out on port " + std::to_string(port) + ".", SMOKE);
            else say(rx.error(), AMBER);
            changed();
        }
    }
    ImGui::Dummy(ImVec2(0, 8 * S));
    if (button("Reset to defaults", false)) { int slot = p.slot; p = fh6::CamParams(); p.slot = slot; changed(); }
    ImGui::Dummy(ImVec2(0, 6 * S));
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void App::previewCard(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    ImGui::InvisibleButton("##preview", ImVec2(w, h));
    dl->AddRectFilled(a, b, rgb(TARMAC), 10 * S);
    ImVec2 ia(a.x + 1, a.y + 1), ib(b.x - 1, b.y - 1);
    dl->PushClipRect(ia, ib, true);
    // the game's 16:9 frame, as large as fits; the rest of the card is dimmed
    float vw = ib.x - ia.x, vh = ib.y - ia.y;
    float fw = std::min(vw, vh * 16.f / 9.f), fh = fw * 9.f / 16.f;
    ImVec2 fa(ia.x + (vw - fw) / 2, ia.y + (vh - fh) / 2), fb(fa.x + fw, fa.y + fh);
    drawPreview(dl, ia, ib, fa, fb, pose, in, scene);
    dl->PopClipRect();
    // rounded mask corners
    dl->AddRect(a, b, rgb(ASPHALT), 10 * S, 3 * S);
    dl->AddRect(a, b, rgb(LINE), 10 * S, 1 * S);

    // readouts
    char buf[64];
    float x = fa.x + 18 * S, y = fb.y - 58 * S;
    std::snprintf(buf, sizeof buf, "%.0f", in.speed * 3.6);
    drawNum(dl, ImVec2(x, y), buf, 30, SMOKE, true);
    float sw = numWidth(buf, 30, true);
    dl->AddText(fonts.regular, 14 * S, ImVec2(x + sw + 6 * S, y + 14 * S), rgb(MUTED), "km/h");
    std::snprintf(buf, sizeof buf, "%.0f\xC2\xB0", std::fabs(in.drift) * 180 / PI);
    float dx = x + sw + 60 * S;
    drawNum(dl, ImVec2(dx, y), buf, 30, std::fabs(in.slip) > 1 ? SAKURA : SMOKE, true);
    dl->AddText(fonts.regular, 14 * S, ImVec2(dx + numWidth(buf, 30, true) + 6 * S, y + 14 * S), rgb(MUTED), "drift angle");

    std::snprintf(buf, sizeof buf, "FOV %.0f\xC2\xB0   aim %+.1f\xC2\xB0   tilt %+.1f\xC2\xB0", pose.fov, pose.aimDeg, pose.tiltDeg);
    float bw = numWidth(buf, 14);
    drawNum(dl, ImVec2(fb.x - bw - 18 * S, fb.y - 34 * S), buf, 14, MUTED);
    const char* note = live ? "Preview of your drive" : "Preview, demo drive";
    dl->AddText(fonts.medium, 15 * S, ImVec2(fa.x + 18 * S, fa.y + 14 * S), rgb(SMOKE), note);
    dl->AddText(fonts.regular, 13 * S, ImVec2(fa.x + 18 * S, fa.y + 34 * S), rgb(MUTED),
                "Approximate. FH6's own camera differs per car, so check in game.");
}

// One small chart card: curve, live marker, caption.
static void chartCard(ImDrawList* dl, ImVec2 a, ImVec2 b, const char* title, const char* caption,
                      const std::vector<fh6::Pt>& curve, double xMax, double xNow, double xScale, const char* xLabel) {
    float S = ui::S;
    dl->AddRectFilled(a, b, rgb(TARMAC), 10 * S);
    dl->AddText(fonts.medium, 15 * S, ImVec2(a.x + 14 * S, a.y + 10 * S), rgb(SMOKE), title);
    float cw = fonts.regular->CalcTextSizeA(13 * S, 1e9f, 0, caption).x;
    dl->AddText(fonts.regular, 13 * S, ImVec2(b.x - 14 * S - cw, a.y + 12 * S), rgb(MUTED), caption);
    ImVec2 ga(a.x + 14 * S, a.y + 38 * S), gb(b.x - 14 * S, b.y - 26 * S);
    for (int i = 0; i <= 4; i++) {
        float yy = ga.y + (gb.y - ga.y) * i / 4.f;
        dl->AddLine(ImVec2(ga.x, yy), ImVec2(gb.x, yy), rgb(KERB), 1);
    }
    auto X = [&](double x) { return ga.x + (gb.x - ga.x) * (float)(x / xMax); };
    auto Y = [&](double y) { return gb.y - (gb.y - ga.y) * (float)y; };
    // filled area and line
    std::vector<ImVec2> pts;
    int n = 64;
    for (int i = 0; i <= n; i++) {
        double x = xMax * i / n;
        pts.push_back(ImVec2(X(x), Y(fh6::curveAt(curve, x / xScale))));
    }
    for (int i = 0; i < n; i++) {
        ImVec2 q[4] = { pts[i], pts[i + 1], ImVec2(pts[i + 1].x, gb.y), ImVec2(pts[i].x, gb.y) };
        dl->AddConvexPolyFilled(q, 4, rgb(SAKURA, 28));
    }
    dl->AddPolyline(pts.data(), (int)pts.size(), rgb(SAKURA), 2 * S);
    double xn = std::clamp(xNow, 0.0, xMax);
    float mx = X(xn), my = Y(fh6::curveAt(curve, xn / xScale));
    dl->AddLine(ImVec2(mx, ga.y), ImVec2(mx, gb.y), rgb(SMOKE, 70), 1 * S);
    dl->AddCircleFilled(ImVec2(mx, my), 5 * S, rgb(SMOKE));
    dl->AddText(fonts.regular, 12 * S, ImVec2(ga.x, gb.y + 6 * S), rgb(DIM), "0");
    float lw = fonts.regular->CalcTextSizeA(12 * S, 1e9f, 0, xLabel).x;
    dl->AddText(fonts.regular, 12 * S, ImVec2(gb.x - lw, gb.y + 6 * S), rgb(DIM), xLabel);
}

void App::inputsStrip(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos();
    float gap = 12 * S, gw = h; // G circle card is square
    float cw = (w - gw - 2 * gap) / 2;
    char cap[96];
    double drift = fh6::curveAt(fh6::DRIFT_CURVE, std::fabs(in.slip));
    std::snprintf(cap, sizeof cap, "slip %.1f, effect %.0f%%", std::fabs(in.slip), drift * 100);
    chartCard(dl, a, ImVec2(a.x + cw, a.y + h), "Drift effects", cap, fh6::DRIFT_CURVE, 8, std::fabs(in.slip), 1, "tyre slip 8");
    double kmh = in.speed * 3.6;
    std::snprintf(cap, sizeof cap, "+%.1f\xC2\xB0 at %.0f km/h", p.speedFov * fh6::curveAt(fh6::SPEED_CURVE, kmh / 1.609344), kmh);
    chartCard(dl, ImVec2(a.x + cw + gap, a.y), ImVec2(a.x + 2 * cw + gap, a.y + h), "Widen with speed", cap, fh6::SPEED_CURVE,
              320, kmh, 1.609344, "320 km/h");

    // G-G circle
    ImVec2 ga(a.x + 2 * cw + 2 * gap, a.y), gb(ga.x + gw, a.y + h);
    dl->AddRectFilled(ga, gb, rgb(TARMAC), 10 * S);
    dl->AddText(fonts.medium, 15 * S, ImVec2(ga.x + 14 * S, ga.y + 10 * S), rgb(SMOKE), "G-forces");
    ImVec2 c((ga.x + gb.x) / 2, (ga.y + gb.y) / 2 + 12 * S);
    float r = (gb.y - ga.y) / 2 - 30 * S;
    dl->AddCircle(c, r, rgb(KERB), 48, 1.5f * S);
    dl->AddCircle(c, r * 0.5f, rgb(KERB), 48, 1 * S);
    dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), rgb(KERB), 1);
    dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), rgb(KERB), 1);
    auto G = [&](float lat, float lon) { return ImVec2(c.x + std::clamp(lat / 1.5f, -1.f, 1.f) * r, c.y - std::clamp(lon / 1.5f, -1.f, 1.f) * r); };
    for (size_t i = 1; i < gTrail.size(); i++)
        dl->AddLine(G(gTrail[i - 1].first, gTrail[i - 1].second), G(gTrail[i].first, gTrail[i].second),
                    rgb(SAKURA, (int)(140 * i / gTrail.size())), 2 * S);
    dl->AddCircleFilled(G((float)in.latG, (float)in.lonG), 5 * S, rgb(SMOKE));
    char gb2[32];
    std::snprintf(gb2, sizeof gb2, "%.1f G", std::hypot(in.latG, in.lonG));
    float tw = numWidth(gb2, 13);
    drawNum(dl, ImVec2(gb.x - 14 * S - tw, ga.y + 12 * S), gb2, 13, MUTED);
    ImGui::Dummy(ImVec2(w, h));
}

void App::footer(float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float h = 64 * S;
    float bw = 150 * S;
    // game folder
    std::string folder = gameDir.empty() ? "No game folder chosen" : u8(gameDir);
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + (h - 34 * S) / 2));
    if (button(gameDir.empty() ? "Choose game folder" : "Change", false)) {
        fs::path picked = pickFolder(gameDir);
        if (!picked.empty()) {
            gameDir = picked;
            stockDir.clear();
            findStock();
            changed();
            checkSaved();
        }
    }
    helpTooltip("The Forza Horizon 6 folder: the one with forzahorizon6.exe (Steam), or its Content folder (Xbox app).");
    ImGui::SameLine(0, 12 * S);
    float tx = ImGui::GetCursorScreenPos().x;
    float textMax = p0.x + w - 2 * bw - 40 * S - tx;
    // path, then the stock files line or the last message
    std::string line2;
    unsigned c2 = MUTED;
    if (fh6::nowSec() - msgTime < 8) { line2 = msg; c2 = msgColor; }
    else if (!stockNote.empty()) { line2 = stockNote; c2 = AMBER; }
    else if (stockOk()) line2 = dirty ? "Not saved to the game yet" : "The game has these settings";
    dl->PushClipRect(ImVec2(tx, p0.y), ImVec2(tx + textMax, p0.y + h), true);
    dl->AddText(fonts.regular, 14 * S, ImVec2(tx, p0.y + 13 * S), rgb(gameDir.empty() ? DIM : SMOKE), folder.c_str());
    dl->AddText(fonts.regular, 13 * S, ImVec2(tx, p0.y + 33 * S), rgb(c2), line2.c_str());
    dl->PopClipRect();
    if (!stockNote.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(tx, p0.y + 30 * S));
        ImGui::InvisibleButton("##stockpick", ImVec2(std::min(textMax, fonts.regular->CalcTextSizeA(13 * S, 1e9f, 0, stockNote.c_str()).x), 18 * S));
        if (ImGui::IsItemClicked()) {
            fs::path picked = pickFolder(gameDir);
            if (!picked.empty()) { stockDir = picked; stockNote = stockOk() ? "" : "That folder doesn't have CameraPhysics.xml and CameraSettings.ini."; changed(); checkSaved(); }
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x + w - 2 * bw - 12 * S, p0.y + (h - 34 * S) / 2));
    if (button("Undo save", false, bw, hasBackup())) undoSave();
    helpTooltip("Puts back the camera files that were in the game before your first save.");
    ImGui::SameLine(0, 12 * S);
    if (button("Save to game", true, bw, stockOk())) saveToGame();
    helpTooltip("Writes the camera into mediapc\\camera in the game folder. Restart FH6 to load it.");
    (void)VERSION;
}
