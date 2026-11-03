// FH6 FOV Live Test: writes a FOV change into the game's camera files every
// time the slider moves, to find out whether FH6 picks up camera file changes
// without a restart (and if not, which in-game action makes it reload them).
// Its window and UI come from the editor (main.cpp, theme).
#pragma once
#include "app.hpp" // platform hooks: exeDir, pickFolder
#include <string>

class FovTest {
public:
    void init();
    void frame(float width, float height);
    void shutdown();

private:
    fs::path gameDir;
    std::string baseIni, baseXml, baseFrom;
    bool baseOk = false;
    bool hadFiles = false;     // the game folder had camera files before the test
    bool written = false;      // the test has written files that aren't put back yet
    double fov = 0;            // degrees added
    int mode = 2;              // 0 layer, 1 settings file, 2 both
    bool restoreOnClose = true;
    int writes = 0;
    double lastWrite = -100, pendingAt = -1;
    std::string lastTime, msg;
    unsigned msgColor = 0;

    fs::path outDir() const { return gameDir / "mediapc" / "camera"; }
    fs::path backupDir() const { return outDir() / "fovtest-backup"; }
    void loadSettings();
    void saveSettings();
    void loadBase();
    void write();
    void restore();
};
