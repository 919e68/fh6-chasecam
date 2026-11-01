#pragma once
#include "core.hpp"
#include "dataout.hpp"
#include "sim.hpp"
#include <deque>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Platform hooks (main.cpp)
fs::path pickFolder(const fs::path& start); // empty if cancelled
fs::path exeDir();
void openFolder(const fs::path& p);

class App {
public:
    void init();
    void frame(float width, float height);
    void shutdown();

private:
    // settings
    fh6::CamParams p;
    fs::path gameDir, stockDir;
    int port = 5607;
    bool followTravel = false;
    bool dirty = false;      // changed since the last save
    double settingsSaveAt = -1;

    // live
    fh6::Receiver rx;
    fh6::DemoDrive demo;
    fh6::TelemetryInputs tin;
    fh6::CameraSim cam;
    fh6::Scene scene;
    fh6::Inputs in;
    fh6::CamPose pose;
    bool live = false;
    std::deque<std::pair<float, float>> gTrail;

    // feedback
    std::string msg;
    unsigned msgColor = 0;
    double msgTime = -100;
    std::string stockNote;

    void loadSettings();
    void saveSettings();
    void changed();
    void say(const std::string& s, unsigned color);
    void findStock();
    fs::path outDir() const { return gameDir / "mediapc" / "camera"; }
    bool stockOk() const;
    void saveToGame();
    void undoSave();
    bool hasBackup() const;
    void checkSaved(); // dirty = game folder files differ from these settings

    void update(float dt);
    void topBar(float w);
    void controls(float w, float h);
    void previewCard(float w, float h);
    void inputsStrip(float w, float h);
    void footer(float w);
};
