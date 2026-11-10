#pragma once
#include "camfiles.hpp"
#include "core.hpp"
#include "dataout.hpp"
#include "layereval.hpp"
#include "sim.hpp"
#include <deque>
#include <map>
#include <vector>
#include "imgui.h"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Platform hooks (main.cpp)
fs::path pickFolder(const fs::path& start); // empty if cancelled
fs::path exeDir();
void openFolder(const fs::path& p);
void setFullscreen(bool on);                // borderless full screen on the window's monitor

class App {
public:
    void init();
    void frame(float width, float height);
    void shutdown();

private:
    // settings
    fh6::CamParams p;
    fh6::Edits edits;        // the user's key and layer edits on top of the game's files
    fs::path gameDir, stockDir;
    int port = 5607;
    bool followTravel = false;
    bool dirty = false;      // changed since the last save
    double settingsSaveAt = -1;
    int page = 0;            // 0 drift setup, 1 camera settings, 2 effect layers

    // the game's files (or the built-in stand-in for the preview) and what Save would write
    std::string baseIni, baseXml;
    bool baseReal = false;
    std::map<std::string, double> baseVals;      // game file values
    std::vector<fh6::XmlSection> baseSecs;
    std::map<std::string, double> finalVals;     // after edits and drift setup
    std::map<std::string, double> driftChange;   // key -> value after the drift setup, for keys it changes
    std::vector<fh6::XmlSection> finalSecs;
    std::string buildError;

    // live
    fh6::Receiver rx;
    fh6::DemoDrive demo;
    fh6::TelemetryInputs tin;
    fh6::CameraSim cam;
    fh6::Scene scene;
    fh6::Inputs in;
    fh6::CamPose pose;
    fh6::LayerStack stack;   // effect layers of the previewed chase camera
    fh6::InputFrame frameIn;
    fh6::ChaseConfig chase;
    int stackSlot = -1;
    bool live = false;
    std::deque<std::pair<float, float>> gTrail;

    // camera settings page
    int setCam = 0;
    std::string selKey;
    // effect layers page
    int laySec = 0, laySel = -1, layScrollTo = -1;
    char layFilter[64] = {};
    std::vector<fh6::Layer> work;     // the section being edited
    std::vector<std::string> workText;
    std::vector<char> workMine;       // 1 = changed here (written in the game's attribute order)
    std::string workFor;              // section the working copy belongs to

    // feedback
    std::string msg, msgTitle;
    unsigned msgColor = 0;
    double msgTime = -100;
    std::string stockNote;

    void loadSettings();
    void saveSettings();
    void changed();       // drift setup changed
    void editsChanged();  // key or layer edits changed
    void say(const std::string& title, const std::string& s, unsigned tone);
    void findStock();
    void loadBase();
    void rebuild();
    int previewSlot() const;
    void loadStack();
    fs::path outDir() const { return gameDir / "mediapc" / "camera"; }
    bool stockOk() const;
    void saveToGame();
    void undoSave();
    bool hasBackup() const;
    void checkSaved(); // dirty = game folder files differ from these settings

    void update(float dt);
    void topBar(float w);
    void controls(float w, float h);
    void previewCard(float w, float h, bool compact = false, bool full = false);
    // full screen preview
    bool fullPreview = false;
    double fullSince = 0;
    void setFull(bool on);
    // load an existing camera as the starting point
    void loadMenu(ImVec2 anchor);
    void loadCamera(const fs::path& dir, const std::string& label);
    std::string baseLabel() const;
    void inputsStrip(float w, float h);
    void footer(float w, float h);
    void chartPanel(ImDrawList* dl, ImVec2 a, ImVec2 b, const char* title, const char* value, const std::vector<fh6::Pt>& curve,
                    double xMax, double xNow, double xScale, const char* x0Label, const char* x1Label);

    // pages.cpp
    void settingsPage(float w, float h);
    void settingsNav(float w, float h);
    void settingsKeys(float w, float h);
    void settingsDetails(float w, float h);
    double keyValue(const std::string& key, bool* inBase = nullptr) const; // game value + edit
    void setKey(const std::string& key, double v);
    void layersPage(float w, float h);
    void layerList(float w, float h);
    void layerEditor(float w, float h);
    void syncWork();      // working copy <- edits or the game's section
    void commitWork();    // edits <- working copy
    void layerChanged(int i);
};
