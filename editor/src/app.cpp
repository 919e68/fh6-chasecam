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

void App::say(const std::string& title, const std::string& s, unsigned tone) {
    msgTitle = title;
    msg = s;
    msgColor = tone;
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
        say("Error", "Can't read the game's camera files. Choose the folder that has them.", DANGER);
        return;
    }
    fh6::Built b;
    try {
        b = fh6::build(ini, xml, p);
    } catch (const std::exception& e) {
        say("Error", std::string("These camera files don't look like FH6's: ") + e.what(), DANGER);
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
        say("Error", "Couldn't write to " + u8(out) + ". Check the folder isn't read-only.", DANGER);
        return;
    }
    dirty = false;
    say("Saved", "Restart Forza Horizon 6 to load the new camera.", COOL);
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
    say("Undone", "Camera files are back to how they were before your first save. Restart FH6 to load them.", COOL);
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
    if (!rx.start(port)) say("Warning", rx.error(), ORANGE);
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
// Layout: NavBar-style top bar, setup panel on the left, preview panel (the
// active one, with corner ticks) and three chart panels on the right, footer
// with the primary action bottom-right. Spacing follows the kit: space-6
// gutters, panel padding and gaps.
// ---------------------------------------------------------------------------
void App::frame(float W, float H) {
    update(std::min(ImGui::GetIO().DeltaTime, 0.1f));
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S) && stockOk()) saveToGame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    float g = SP6 * S;
    float topH = 56 * S, footH = 88 * S, leftW = 348 * S;
    ImGui::SetCursorPos(ImVec2(0, 0));
    topBar(W);
    float bodyY = topH + g, bodyH = H - topH - footH - 2 * g;
    ImGui::SetCursorPos(ImVec2(g, bodyY));
    controls(leftW, bodyH);
    float rx0 = g + leftW + g, rw = W - rx0 - g;
    float stripH = std::clamp(bodyH * 0.28f, 150 * S, 190 * S);
    ImGui::SetCursorPos(ImVec2(rx0, bodyY));
    previewCard(rw, bodyH - stripH - g);
    ImGui::SetCursorPos(ImVec2(rx0, bodyY + bodyH - stripH));
    inputsStrip(rw, stripH);
    ImGui::SetCursorPos(ImVec2(0, H - footH));
    footer(W, footH);
    ImGui::End();
}

// FH6 ChaseCam mark: a viewfinder with the kit's cut corner, and the car in it
static void drawMark(ImDrawList* dl, ImVec2 p, float sz, ImU32 col) {
    float k = sz / 24.f;
    cutLine(dl, p, ImVec2(p.x + 24 * k, p.y + 24 * k), 8 * k, col, 2.5f * k);
    dl->AddRectFilled(ImVec2(p.x + 6 * k, p.y + 12 * k), ImVec2(p.x + 13 * k, p.y + 18 * k), col);
}

void App::topBar(float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float h = 56 * S, g = SP6 * S, cy = p0.y + h / 2;
    dl->AddRectFilled(ImVec2(p0.x, p0.y + h - HAIR * S), ImVec2(p0.x + w, p0.y + h), rgb(LINE));
    // lockup: mark at 1.5x cap height, space-3, name
    float mk = 20 * S;
    drawMark(dl, ImVec2(p0.x + g, cy - mk / 2), mk, rgb(ORANGE));
    float nx = p0.x + g + mk + SP3 * S;
    drawText(dl, ImVec2(nx, cy - textHeight(Lockup) / 2 - 1 * S), Lockup, INK, "FH6 ChaseCam");
    drawText(dl, ImVec2(nx + textWidth(Lockup, "FH6 ChaseCam") + SP4 * S, cy - textHeight(Label) / 2), Label, INK_MUTED, "by SPIRE");

    // camera slot, right
    static const char* slots[] = { "Chase Near", "Chase Far" };
    float sw = segmentTabsWidth(slots, 2), sh = (SP2 + 13 + SP3) * S;
    ImGui::SetCursorScreenPos(ImVec2(p0.x + w - g - sw, p0.y + h - sh));
    if (segmentTabs("slot", &p.slot, slots, 2)) { cam.reset(); changed(); }

    // Data Out status: a tag and a short note
    bool err = !rx.error().empty();
    const char* tg = live ? "Live" : err ? "Port in use" : "Demo";
    Tone tone = live ? Tone::Cool : err ? Tone::Danger : Tone::Neutral;
    char note[96];
    if (live) std::snprintf(note, sizeof note, "%.0f Hz", rx.rate());
    else if (err) std::snprintf(note, sizeof note, "port %d", port);
    else std::snprintf(note, sizeof note, "Waiting for Data Out on port %d", port);
    Style ns = live || err ? Stat : Caption;
    float nw = textWidth(ns, note), tw = tagWidth(tg);
    float right = p0.x + w - g - sw - SP8 * S;
    drawText(dl, ImVec2(right - nw, cy - textHeight(ns) / 2 - 1 * S), ns, INK_MUTED, note);
    ImVec2 tp(right - nw - SP2 * S - tw, cy - 10 * S);
    tag(dl, tp, tone, tg);
    ImGui::SetCursorScreenPos(tp);
    ImGui::InvisibleButton("##status", ImVec2(tw + SP2 * S + nw, 20 * S));
    helpTooltip("In FH6: Settings > HUD and Gameplay > Data Out On, IP 127.0.0.1, and the port in Data Out on the left. "
                "Without it the preview runs a demo drive.");
}

void App::controls(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    panel(dl, a, b, false);
    float pad = SP6 * S;
    drawText(dl, ImVec2(a.x + pad, a.y + pad), Label, INK_MUTED, fh6::SLOTS[p.slot ? 1 : 0].name);
    drawText(dl, ImVec2(a.x + pad, a.y + pad + 14 * S + SP1 * S), Headline, INK, "Setup");
    float top = pad + 14 * S + SP1 * S + 32 * S + SP4 * S;
    ImGui::SetCursorScreenPos(ImVec2(a.x + 1 * S, a.y + top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad - 1 * S, 0));
    ImGui::BeginChild("##controls", ImVec2(w - 2 * S, h - top - 2 * S), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SP3 * S, SP4 * S));
    bool c = false;
    groupTitle("Position", true);
    c |= slider("h", "Height", &p.heightOfs, -2, 2, 0.05, " m", 2, true, -0.7,
                "Camera height compared to the stock camera. Lower: closer to the road, more dramatic.");
    c |= slider("d", "Distance", &p.radiusOfs, -3, 3, 0.05, " m", 2, true, -0.5,
                "Distance behind the car compared to stock. Negative: closer.");
    c |= slider("t", "Aim height", &p.targetOfs, -2, 2, 0.05, " m", 2, true, -0.55,
                "Height of the point the camera looks at. Lower it less than the camera for a flatter view with more horizon.");
    groupTitle("View", false);
    c |= slider("fov", "Field of view", &p.fovAdd, -15, 25, 1, "\xC2\xB0", 0, true, 6,
                "Added to the camera's stock field of view. Higher: wider, feels faster.");
    c |= slider("mfov", "Field of view limit", &p.maxFovAdd, 0, 40, 1, "\xC2\xB0", 0, true, 10,
                "Raises the highest field of view the camera may reach. Raise it with the setting above.");
    c |= slider("sfov", "Widen with speed", &p.speedFov, 0, 20, 1, "\xC2\xB0", 0, false, 6,
                "Extra field of view from about 65 to 320 km/h.");
    groupTitle("Follow", false);
    double fp = p.follow * 100;
    if (slider("fol", "Rotation follow", &fp, 30, 150, 5, " %", 0, false, 80,
               "How tightly the camera turns with the car, compared to stock. Lower: looser, shows more of the car's side in a drift.")) {
        p.follow = fp / 100;
        c = true;
    }
    groupTitle("Drift", false);
    c |= slider("aim", "Look into the corner", &p.cornerAim, 0, 20, 0.5, "\xC2\xB0", 1, false, 6,
                "While you drift, turns the view toward the inside of the corner, so the car sits toward the outside of the frame.");
    c |= slider("tilt", "Horizon tilt", &p.driftTilt, -10, 10, 0.5, "\xC2\xB0", 1, true, 3,
                "While you drift, tips the horizon. Positive: toward the outside of the corner. 0: level.");
    c |= slider("sm", "Drift response", &p.driftSmooth, 2, 40, 1, "", 0, false, 10,
                "How fast the drift effects come in and go. Higher: snappier. Lower: smoother.");
    if (c) changed();

    groupTitle("Preview", false);
    toggle("ft", "Game's Drift camera is on", &followTravel,
           "Match FH6's own Drift camera setting (HUD and Gameplay). It makes the camera follow your direction of travel.");
    groupTitle("Direction fix", false);
    bool f = false;
    f |= toggle("fa", "Flip look into the corner", &p.flipAim,
                "Use if the game turns the view to the outside of the corner. The preview always shows the intended direction.");
    f |= toggle("ftl", "Flip horizon tilt", &p.flipTilt, "Use if the game tips the horizon the other way than the preview.");
    if (f) changed();

    groupTitle("Data Out", false);
    textWrapped(Body, INK_MUTED, "In FH6: Settings > HUD and Gameplay > Data Out On, IP 127.0.0.1, and this port.", ImGui::GetContentRegionAvail().x);
    static int portEdit = 0;
    if (portEdit == 0) portEdit = port;
    TypeStyle st = typeStyle(Stat);
    ImGui::PushFont(st.font, st.size);
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(LINE_STRONG));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SP3 * S, (40 * S - 13 * S) / 2));
    ImGui::SetNextItemWidth(110 * S);
    ImGui::InputInt("##port", &portEdit, 0, 0);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    portEdit = std::clamp(portEdit, 1024, 65535);
    ImGui::SameLine(0, SP3 * S);
    if (button("Listen", Btn::Secondary, false, 0, portEdit != port || !rx.error().empty())) {
        if (portEdit >= 5200 && portEdit <= 5300) say("Warning", "Ports 5200 to 5300 are used by FH6 itself. Pick another.", ORANGE);
        else {
            port = portEdit;
            if (rx.start(port)) say("Info", "Listening for Data Out on port " + std::to_string(port) + ".", COOL);
            else say("Warning", rx.error(), ORANGE);
            changed();
        }
    }
    groupTitle("Reset", false);
    if (button("Reset setup", Btn::Danger)) { int slot = p.slot; p = fh6::CamParams(); p.slot = slot; changed(); }
    helpTooltip("Puts every setting above back to the FH6 ChaseCam defaults. Saved camera files stay until you save again.");
    ImGui::Dummy(ImVec2(0, SP6 * S));
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Compact HUD card on black glass: columns of label over value, bottom-anchored.
struct HudCol { const char* label; std::string value; const char* unit; unsigned color; };
static void hudCard(ImDrawList* dl, ImVec2 anchor, bool alignRight, const std::vector<HudCol>& cols, Style valueStyle) {
    float S = ui::S, padX = SP4 * S, padY = SP3 * S, gap = SP6 * S;
    float vh = textHeight(valueStyle), w = 0;
    std::vector<float> cw;
    for (auto& c : cols) {
        float vw = textWidth(valueStyle, c.value.c_str()) + (c.unit ? SP1 * S + textWidth(Stat, c.unit) : 0);
        cw.push_back(std::max(textWidth(Label, c.label), vw));
        w += cw.back();
    }
    w += 2 * padX + gap * (cols.size() - 1);
    float h = padY + 14 * S + SP1 * S + vh + padY;
    ImVec2 a(alignRight ? anchor.x - w : anchor.x, anchor.y - h), b(a.x + w, anchor.y);
    panel(dl, a, b, false, 80);
    float x = a.x + padX;
    for (size_t i = 0; i < cols.size(); i++) {
        drawText(dl, ImVec2(x, a.y + padY), Label, ON_GLASS_MUTED, cols[i].label);
        float vy = a.y + padY + 14 * S + SP1 * S;
        drawText(dl, ImVec2(x, vy), valueStyle, cols[i].color, cols[i].value.c_str());
        if (cols[i].unit)
            drawText(dl, ImVec2(x + textWidth(valueStyle, cols[i].value.c_str()) + SP1 * S, vy + vh - 15 * S), Stat, ON_GLASS_MUTED, cols[i].unit);
        x += cw[i] + gap;
    }
}

void App::previewCard(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    ImGui::InvisibleButton("##preview", ImVec2(w, h));
    // the game's 16:9 frame, as large as fits; the rest of the panel is dimmed
    ImVec2 ia(a.x + 1 * S, a.y + 1 * S), ib(b.x - 1 * S, b.y - 1 * S);
    float vw = ib.x - ia.x, vh = ib.y - ia.y;
    float fw = std::min(vw, vh * 16.f / 9.f), fh = fw * 9.f / 16.f;
    ImVec2 fa(ia.x + (vw - fw) / 2, ia.y + (vh - fh) / 2), fb(fa.x + fw, fa.y + fh);
    drawPreview(dl, ia, ib, fa, fb, pose, in, scene);
    // the panel's cut corner and hairline over the scene, then the active-panel ticks
    float cut = CH_LG * S;
    dl->AddTriangleFilled(ImVec2(b.x - cut, a.y), ImVec2(b.x + 1, a.y - 1), ImVec2(b.x + 1, a.y + cut), rgb(BG100));
    cutLine(dl, a, b, cut, rgb(LINE), HAIR * S);
    {
        float t = SP3 * S, rw = RULE * S;
        dl->AddRectFilled(a, ImVec2(a.x + t, a.y + rw), rgb(ORANGE));
        dl->AddRectFilled(a, ImVec2(a.x + rw, a.y + t), rgb(ORANGE));
        dl->AddRectFilled(ImVec2(b.x - t, b.y - rw), b, rgb(ORANGE));
        dl->AddRectFilled(ImVec2(b.x - rw, b.y - t), b, rgb(ORANGE));
    }

    float g = SP6 * S;
    drawText(dl, ImVec2(fa.x + g, fa.y + g), Label, INK_MUTED, live ? "Preview of your drive" : "Preview, demo drive");
    drawText(dl, ImVec2(fa.x + g, fa.y + g + 14 * S + SP1 * S), Caption, INK_MUTED,
             "Approximate. FH6's own camera differs per car, so check in game.");

    // HUD: speed and drift angle bottom left, the camera's live values bottom right
    char spd[32], ang[32], fov[32], aim[32], tlt[32];
    double effect = fh6::curveAt(fh6::DRIFT_CURVE, std::fabs(in.slip));
    std::snprintf(spd, sizeof spd, "%.0f", in.speed * 3.6);
    std::snprintf(ang, sizeof ang, "%.0f\xC2\xB0", std::fabs(in.drift) * 180 / PI);
    std::snprintf(fov, sizeof fov, "%.0f\xC2\xB0", pose.fov);
    fmtNum(aim, sizeof aim, pose.aimDeg, 1, true);
    fmtNum(tlt, sizeof tlt, pose.tiltDeg, 1, true);
    hudCard(dl, ImVec2(fa.x + g, fb.y - g), false,
            { { "Speed", spd, "km/h", ON_GLASS }, { "Drift angle", ang, nullptr, effect > 0.05 ? ORANGE_HOT : ON_GLASS } }, Readout);
    hudCard(dl, ImVec2(fb.x - g, fb.y - g), true,
            { { "Field of view", fov, nullptr, ON_GLASS }, { "Corner aim", std::string(aim) + "\xC2\xB0", nullptr, ON_GLASS },
              { "Horizon tilt", std::string(tlt) + "\xC2\xB0", nullptr, ON_GLASS } }, Stat);
}

// Chart panel: label and value, a hairline grid, the response curve in orange,
// and a marker at the current input.
void App::chartPanel(ImDrawList* dl, ImVec2 a, ImVec2 b, const char* title, const char* value, const std::vector<fh6::Pt>& curve,
                     double xMax, double xNow, double xScale, const char* x0Label, const char* x1Label) {
    panel(dl, a, b, false);
    float pad = SP6 * S;
    drawText(dl, ImVec2(a.x + pad, a.y + pad - 4 * S), Label, INK_MUTED, title);
    drawText(dl, ImVec2(b.x - pad - textWidth(Stat, value), a.y + pad - 5 * S), Stat, INK, value);
    ImVec2 ga(a.x + pad, a.y + pad + 22 * S), gb(b.x - pad, b.y - pad - 14 * S);
    for (int i = 0; i <= 3; i++) {
        float yy = std::floor(ga.y + (gb.y - ga.y) * i / 3.f);
        dl->AddRectFilled(ImVec2(ga.x, yy), ImVec2(gb.x, yy + HAIR * S), rgb(LINE));
    }
    auto X = [&](double x) { return ga.x + (gb.x - ga.x) * (float)(x / xMax); };
    auto Y = [&](double y) { return gb.y - (gb.y - ga.y) * (float)y; };
    std::vector<ImVec2> pts;
    for (int i = 0; i <= 64; i++) {
        double x = xMax * i / 64;
        pts.push_back(ImVec2(X(x), Y(fh6::curveAt(curve, x / xScale))));
    }
    dl->AddPolyline(pts.data(), (int)pts.size(), rgb(ORANGE), RULE * S);
    double xn = std::clamp(xNow, 0.0, xMax);
    float mx = std::floor(X(xn)), my = Y(fh6::curveAt(curve, xn / xScale));
    dl->AddRectFilled(ImVec2(mx, ga.y), ImVec2(mx + HAIR * S, gb.y), rgb(LINE_STRONG));
    dl->AddRectFilled(ImVec2(mx - 3 * S, my - 3 * S), ImVec2(mx + 3 * S, my + 3 * S), rgb(INK));
    drawText(dl, ImVec2(ga.x, gb.y + SP1 * S), Caption, INK_MUTED, x0Label);
    drawText(dl, ImVec2(gb.x - textWidth(Caption, x1Label), gb.y + SP1 * S), Caption, INK_MUTED, x1Label);
}

void App::inputsStrip(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos();
    float g = SP6 * S, gw = h + 20 * S;
    float cw = (w - gw - 2 * g) / 2;
    char v1[48], v2[48];
    double effect = fh6::curveAt(fh6::DRIFT_CURVE, std::fabs(in.slip));
    std::snprintf(v1, sizeof v1, "%.0f %%", effect * 100);
    chartPanel(dl, a, ImVec2(a.x + cw, a.y + h), "Drift effects", v1, fh6::DRIFT_CURVE, 8, std::fabs(in.slip), 1, "grip", "tyre slip 8");
    double kmh = in.speed * 3.6;
    char sv[32];
    fmtNum(sv, sizeof sv, p.speedFov * fh6::curveAt(fh6::SPEED_CURVE, kmh / 1.609344), 1, true);
    std::snprintf(v2, sizeof v2, "%s\xC2\xB0", sv);
    chartPanel(dl, ImVec2(a.x + cw + g, a.y), ImVec2(a.x + 2 * cw + g, a.y + h), "Widen with speed", v2, fh6::SPEED_CURVE, 320, kmh,
               1.609344, "0 km/h", "320 km/h");

    // G-force circle
    ImVec2 ga(a.x + 2 * cw + 2 * g, a.y), gb(ga.x + gw, a.y + h);
    panel(dl, ga, gb, false);
    float pad = SP6 * S;
    drawText(dl, ImVec2(ga.x + pad, ga.y + pad - 4 * S), Label, INK_MUTED, "G-force");
    char gv[32];
    std::snprintf(gv, sizeof gv, "%.1f G", std::hypot(in.latG, in.lonG));
    drawText(dl, ImVec2(gb.x - pad - textWidth(Stat, gv), ga.y + pad - 5 * S), Stat, INK, gv);
    ImVec2 c((ga.x + gb.x) / 2, (ga.y + pad + 22 * S + gb.y - pad) / 2);
    float r = std::min((gb.x - ga.x) / 2 - pad, (gb.y - pad - (ga.y + pad + 22 * S)) / 2);
    dl->AddCircle(c, r, rgb(LINE), 48, HAIR * S);
    dl->AddCircle(c, r * 0.5f, rgb(LINE), 48, HAIR * S);
    dl->AddRectFilled(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y + HAIR * S), rgb(LINE));
    dl->AddRectFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + HAIR * S, c.y + r), rgb(LINE));
    auto G = [&](float lat, float lon) { return ImVec2(c.x + std::clamp(lat / 1.5f, -1.f, 1.f) * r, c.y - std::clamp(lon / 1.5f, -1.f, 1.f) * r); };
    for (size_t i = 1; i < gTrail.size(); i++)
        dl->AddLine(G(gTrail[i - 1].first, gTrail[i - 1].second), G(gTrail[i].first, gTrail[i].second),
                    rgb(ORANGE, (int)(200 * i / gTrail.size())), RULE * S);
    ImVec2 d = G((float)in.latG, (float)in.lonG);
    dl->AddRectFilled(ImVec2(d.x - 3 * S, d.y - 3 * S), ImVec2(d.x + 3 * S, d.y + 3 * S), rgb(INK));
    ImGui::Dummy(ImVec2(w, h));
}

void App::footer(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float g = SP6 * S, cy = p0.y + h / 2;
    dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + HAIR * S), rgb(LINE));

    // right: undo, then the primary
    const char* slotName = fh6::SLOTS[p.slot ? 1 : 0].name;
    std::string sub = std::string("Replaces ") + slotName;
    float pw = 260 * S, uw = 150 * S;
    ImGui::SetCursorScreenPos(ImVec2(p0.x + w - g - pw, cy - 28 * S));
    if (button("Save to game", Btn::Primary, true, pw, stockOk(), "CTRL+S", sub.c_str())) saveToGame();
    helpTooltip("Writes the camera into mediapc\\camera in the game folder. Restart FH6 to load it.");
    ImGui::SetCursorScreenPos(ImVec2(p0.x + w - g - pw - SP4 * S - uw, cy - 20 * S));
    if (button("Undo save", Btn::Secondary, false, uw, hasBackup())) undoSave();
    helpTooltip("Puts back the camera files that were in the game before your first save.");

    // left: folder button, path, and a one-line alert
    ImGui::SetCursorScreenPos(ImVec2(p0.x + g, cy - 20 * S));
    if (button(gameDir.empty() ? "Choose folder" : "Change folder", Btn::Secondary)) {
        fs::path picked = pickFolder(gameDir);
        if (!picked.empty()) { gameDir = picked; stockDir.clear(); findStock(); changed(); checkSaved(); }
    }
    helpTooltip("The Forza Horizon 6 folder: the one with forzahorizon6.exe (Steam), or its Content folder (Xbox app).");
    ImGui::SameLine(0, SP4 * S);
    float tx = ImGui::GetCursorScreenPos().x;
    if (!stockNote.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(tx, cy - 20 * S));
        if (button("Find camera files", Btn::Ghost)) {
            fs::path picked = pickFolder(gameDir);
            if (!picked.empty()) {
                stockDir = picked;
                stockNote = stockOk() ? "" : "That folder doesn't have CameraPhysics.xml and CameraSettings.ini.";
                changed();
                checkSaved();
            }
        }
        helpTooltip("Pick the folder that has the game's own CameraPhysics.xml and CameraSettings.ini.");
        ImGui::SameLine(0, SP4 * S);
        tx = ImGui::GetCursorScreenPos().x;
    }
    float textMax = p0.x + w - g - pw - SP4 * S - uw - SP6 * S - tx;
    std::string folder = gameDir.empty() ? "No game folder chosen" : u8(gameDir);
    std::string title, line;
    unsigned tone = INK_MUTED;
    if (fh6::nowSec() - msgTime < 8) { title = msgTitle; line = msg; tone = msgColor; }
    else if (!stockNote.empty()) { title = "Warning"; line = stockNote; tone = ORANGE; }
    else if (stockOk()) { title = dirty ? "Not saved" : "Saved"; line = dirty ? "The game doesn't have these settings yet." : "The game has these settings."; tone = dirty ? INK_MUTED : COOL; }
    dl->PushClipRect(ImVec2(tx, p0.y), ImVec2(tx + std::max(0.f, textMax), p0.y + h), true);
    drawText(dl, ImVec2(tx, cy - 20 * S), Body, gameDir.empty() ? INK_MUTED : INK, folder.c_str());
    if (!line.empty()) {
        drawText(dl, ImVec2(tx, cy + 4 * S), Label, tone, title.c_str());
        drawText(dl, ImVec2(tx + textWidth(Label, title.c_str()) + SP2 * S, cy + 2 * S), Caption, INK, line.c_str());
    }
    dl->PopClipRect();
}
