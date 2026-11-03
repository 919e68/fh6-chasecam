#include "fovtest.hpp"
#include "camfiles.hpp"
#include "dataout.hpp"
#include "imgui.h"
#include "theme.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

using namespace ui;

static bool readPath(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}
// write next to the target, then swap it in, so the game never reads a half-written file
static bool writeAtomic(const fs::path& p, const std::string& data) {
    fs::path tmp = p;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), (std::streamsize)data.size());
        if (!f) return false;
    }
    return MoveFileExW(tmp.wstring().c_str(), p.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
static bool hasPair(const fs::path& d) {
    std::error_code ec;
    return fs::exists(d / "CameraSettings.ini", ec) && fs::exists(d / "CameraPhysics.xml", ec);
}
// Windows' own copy: std::filesystem::copy_file can't overwrite an existing file with MinGW
static bool copyOver(const fs::path& from, const fs::path& to) {
    return CopyFileW(from.wstring().c_str(), to.wstring().c_str(), FALSE) != 0;
}
static bool sameFile(const fs::path& a, const fs::path& b) {
    std::string x, y;
    return readPath(a, x) && readPath(b, y) && x == y;
}
static fs::path settingsFile() { return exeDir() / "fh6-fovtest.ini"; }

void FovTest::loadSettings() {
    // own settings first, then the editor's game folder
    for (const fs::path& f : { settingsFile(), exeDir() / "fh6chasecam-editor.ini" }) {
        std::string t;
        if (!readPath(f, t)) continue;
        std::istringstream s(t);
        std::string line;
        while (std::getline(s, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("gameDir=", 0) == 0 && gameDir.empty()) gameDir = fs::u8path(line.substr(8));
            if (line.rfind("mode=", 0) == 0 && f == settingsFile()) mode = std::clamp(std::atoi(line.c_str() + 5), 0, 2);
        }
    }
}

void FovTest::saveSettings() {
    std::ofstream f(settingsFile(), std::ios::binary | std::ios::trunc);
    f << "gameDir=" << gameDir.u8string() << "\nmode=" << mode << "\n";
}

// The files the test builds on: what the game folder has now (kept in a backup
// so they can be put back), or the editor's camera-base folder.
void FovTest::loadBase() {
    baseOk = false;
    hadFiles = false;
    baseFrom.clear();
    if (gameDir.empty()) return;
    fs::path from;
    if (hasPair(backupDir())) { from = backupDir(); hadFiles = true; written = true; } // left from a test that wasn't put back
    else if (hasPair(outDir())) { from = outDir(); hadFiles = true; }
    else if (hasPair(exeDir() / "camera-base")) from = exeDir() / "camera-base";
    if (from.empty()) { msg = "No camera files to start from. Put this exe next to the editor's camera-base folder."; msgColor = DANGER; return; }
    if (!readPath(from / "CameraSettings.ini", baseIni) || !readPath(from / "CameraPhysics.xml", baseXml)) return;
    baseOk = true;
    baseFrom = hadFiles ? "the camera files in the game now" : "camera-base next to this exe";
}

void FovTest::write() {
    if (!baseOk) return;
    std::error_code ec;
    fs::create_directories(outDir(), ec);
    if (hadFiles && !hasPair(backupDir())) {
        // keep the game's current camera files before touching them; never write without a good backup
        fs::create_directories(backupDir(), ec);
        bool ok = true;
        for (const char* n : { "CameraSettings.ini", "CameraPhysics.xml" })
            ok = ok && copyOver(outDir() / n, backupDir() / n) && sameFile(outDir() / n, backupDir() / n);
        if (!ok) {
            msg = "Couldn't back up your camera files, so nothing was written.";
            msgColor = DANGER;
            return;
        }
    }
    std::string ini = baseIni, xml = baseXml;
    try {
        if (fov != 0) {
            for (const char* sec : { "FollowLowCam", "FollowHighCam" }) {
                if (mode != 0) ini = fh6::editIni(ini, sec, "FOV", fov, true, false, nullptr);
                // keep the game's limits from clamping the change away
                if (fov > 0) ini = fh6::editIni(ini, sec, "MaxFOV", fov, true, false, nullptr);
                else ini = fh6::editIni(ini, sec, "MinFOV", fov, true, false, nullptr);
            }
            if (mode != 1) {
                // an always-on FOV layer, like the game's own +10 on the bumper camera
                fh6::Layer l;
                l.set("Enabled", "1");
                l.set("LocationType", "All");
                l.set("InputType", "ConstantOne");
                l.set("NoiseType", "None");
                l.set("OutputType", "FOV");
                l.set("InputToParamMapping", "ConstantOne");
                l.setNum("Param.x", fov);
                fh6::normalizeLayer(l);
                std::string lt = fh6::writeLayer(l);
                xml = fh6::appendLayers(xml, "FollowCam", { lt });
                xml = fh6::appendLayers(xml, "FollowCam2", { lt });
            }
        }
    } catch (const std::exception& e) {
        msg = std::string("These camera files can't be changed: ") + e.what();
        msgColor = DANGER;
        return;
    }
    if (!writeAtomic(outDir() / "CameraSettings.ini", ini) || !writeAtomic(outDir() / "CameraPhysics.xml", xml)) {
        msg = "Couldn't write to the game folder. Is it read-only?";
        msgColor = DANGER;
        return;
    }
    written = true;
    writes++;
    lastWrite = fh6::nowSec();
    std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%H:%M:%S", std::localtime(&t));
    lastTime = b;
    msg.clear();
}

void FovTest::restore() {
    std::error_code ec;
    if (hasPair(backupDir())) {
        bool ok = true;
        for (const char* n : { "CameraSettings.ini", "CameraPhysics.xml" })
            ok = ok && copyOver(backupDir() / n, outDir() / n) && sameFile(backupDir() / n, outDir() / n);
        if (!ok) {
            msg = "Couldn't put the files back. Your originals are safe in mediapc\\camera\\fovtest-backup.";
            msgColor = DANGER;
            return;
        }
        fs::remove_all(backupDir(), ec);
    } else if (!hadFiles) {
        fs::remove(outDir() / "CameraSettings.ini", ec);
        fs::remove(outDir() / "CameraPhysics.xml", ec);
    }
    written = false;
    fov = 0;
    msg = "Your camera files are back. Restart FH6 if it picked up the test.";
    msgColor = COOL;
    loadBase();
}

void FovTest::init() {
    loadSettings();
    loadBase();
}

void FovTest::shutdown() {
    saveSettings();
    if (restoreOnClose && written) restore();
}

void FovTest::frame(float W, float H) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus);
    float g = SP6 * S, colW = std::floor((W - 3 * g) / 2), top = g;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SP2 * S, SP3 * S));
    ImGui::SetCursorPos(ImVec2(g, top));
    text(Lockup, INK, "FOV live test");
    ImGui::SetCursorPos(ImVec2(g, ImGui::GetCursorPosY() - SP1 * S));
    textWrapped(Caption, INK_MUTED,
                "Moves the chase cameras' field of view by writing the camera files every time you change it. "
                "If FH6 shows the change without a restart, live editing works.",
                W - 2 * g);
    float colTop = ImGui::GetCursorPosY() + SP1 * S, colH = H - colTop - g;

    // ---- left: folder, target file, the FOV change
    ImGui::SetCursorPos(ImVec2(g, colTop));
    ImGui::BeginChild("##left", ImVec2(colW, colH));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SP2 * S, SP2 * S));
    groupTitle("Game folder", true);
    std::string folder = gameDir.empty() ? "No game folder chosen" : gameDir.u8string();
    textWrapped(Body, gameDir.empty() ? INK_MUTED : INK, folder.c_str(), colW);
    if (button(gameDir.empty() ? "Choose folder" : "Change folder", Btn::Secondary)) {
        fs::path picked = pickFolder(gameDir);
        if (!picked.empty()) {
            if (written) restore();
            gameDir = picked;
            loadBase();
            saveSettings();
        }
    }
    helpTooltip("The folder with forzahorizon6.exe (Steam), or the game's Content folder (Xbox app).");
    if (baseOk) textWrapped(Caption, INK_MUTED, ("Starting from " + baseFrom + ".").c_str(), colW);

    groupTitle("Write to", false);
    static const char* modes[] = { "Layer", "Settings", "Both" };
    if (segmentTabs("mode", &mode, modes, 3)) { pendingAt = fh6::nowSec(); saveSettings(); }
    textWrapped(Caption, INK_MUTED,
                mode == 0   ? "An always-on FOV layer on Chase Near and Chase Far (CameraPhysics.xml)."
                : mode == 1 ? "FOV in CameraSettings.ini. FH6's own FOV slider may override it."
                            : "Both files. If it works, try each alone to see which file the game reloads.",
                colW);

    groupTitle("Field of view change", false);
    if (slider("fov", "Degrees added", &fov, -30, 40, 1, "\xC2\xB0", 0, true, 0, "Right-click to go back to 0.")) pendingAt = fh6::nowSec();
    static const double presets[] = { -15, 0, 15, 30 };
    static const char* plab[] = { "\xE2\x88\x92 15", "0", "+ 15", "+ 30" };
    float bw = std::floor((colW - 3 * SP2 * S) / 4);
    for (int i = 0; i < 4; i++) {
        if (i) ImGui::SameLine(0, SP2 * S);
        ImGui::PushID(i);
        if (button(plab[i], Btn::Secondary, false, bw, baseOk)) { fov = presets[i]; pendingAt = fh6::nowSec(); }
        ImGui::PopID();
    }
    // write right away, at most 10 times a second
    if (pendingAt > 0 && baseOk && fh6::nowSec() - lastWrite > 0.1) { write(); pendingAt = -1; }
    std::string st = !baseOk ? "Nothing to write yet."
                     : writes ? "Written " + lastTime + "  \xC2\xB7  " + std::to_string(writes) + (writes == 1 ? " write" : " writes")
                              : "Not written yet. Press + 30.";
    text(Stat, writes ? COOL : INK_MUTED, st.c_str());
    if (!msg.empty()) textWrapped(Caption, msgColor, msg.c_str(), colW);
    ImGui::PopStyleVar();
    ImGui::EndChild();

    // ---- right: how to test, putting things back
    ImGui::SetCursorPos(ImVec2(g + colW + g, colTop));
    ImGui::BeginChild("##right", ImVec2(colW, colH));
    groupTitle("How to test", true);
    static const char* steps[] = {
        "Drive in FH6 on Chase Near or Chase Far. Windowed or borderless makes it easier to watch.",
        "Press + 30. If the view widens right away, the game reloads the files live.",
        "If not, try these one at a time and note which one shows it: switch camera, pause menu, photo mode, change car, start or leave an event.",
        "Press 0 and check it goes back the same way.",
    };
    for (int i = 0; i < 4; i++) {
        char n[8];
        std::snprintf(n, sizeof n, "%d", i + 1);
        ImVec2 p = ImGui::GetCursorScreenPos();
        drawText(ImGui::GetWindowDrawList(), ImVec2(p.x, p.y + 1 * S), Stat, ORANGE, n);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 18 * S, p.y));
        textWrapped(Body, INK, steps[i], colW - 18 * S);
    }
    groupTitle("When done", false);
    if (button("Put files back", Btn::Secondary, false, 0, written)) restore();
    helpTooltip("Puts back the camera files that were in the game before the test.");
    toggle("roc", "Put back when this closes", &restoreOnClose, "Recommended. The camera you saved with the editor comes back.");
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}
