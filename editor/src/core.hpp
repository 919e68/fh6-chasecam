// FH6 ChaseCam: edits Forza Horizon 6 camera files.
//
// FH6 reads mediapc/camera/CameraSettings.ini ("Section\Key value" lines) and
// mediapc/camera/CameraPhysics.xml (per camera, a list of effect layers). Edits
// are made in place on the original text, so every line we do not touch stays
// byte-for-byte the same. Same behaviour as fh6cam.py / build.py.
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace fh6 {

// Which ini section and xml section belong to each chase camera.
struct Slot { const char* name; const char* ini; const char* xml; };
extern const Slot SLOTS[2]; // 0 = Chase Near, 1 = Chase Far

// Everything the editor can change. Defaults = the "drift" preset.
struct CamParams {
    int slot = 1;               // 0 Chase Near, 1 Chase Far
    double heightOfs = -0.7;    // m, camera height (also its min/max above ground)
    double targetOfs = -0.55;   // m, aim point height
    double radiusOfs = -0.5;    // m, distance (negative = closer)
    double fovAdd = 6;          // deg added to FOV
    double maxFovAdd = 10;      // deg added to MaxFOV
    double follow = 0.8;        // multiplies the rotation follow spring (lower = looser)
    double cornerAim = 6;       // deg, look into the corner at full drift
    double driftTilt = 3;       // deg, horizon tilt at full drift (+ = outward)
    double speedFov = 6;        // deg, extra FOV at speed
    double driftSmooth = 10;    // spring stiffness for the drift effects (lower = smoother)
    bool flipAim = false, flipTilt = false;
};

// One ini value change: key, old, new.
struct IniChange { std::string key; double before, after; };

// Curve point (input, output) and an effect layer.
struct Pt { double x, y; };
std::string layer(const std::string& input, const std::string& output, double px, double py, double pz,
                  double clampMin, double clampMax, const std::vector<Pt>& curve, bool mirrored,
                  double springK, double springD); // springK <= 0: no spring

// Drift input curve shared by corner aim and tilt (lateral tyre slip, ~1 = grip limit).
extern const std::vector<Pt> DRIFT_CURVE;
extern const double DRIFT_CLAMP;
extern const std::vector<Pt> SPEED_CURVE; // speed FOV, input in mph

// Low-level edits. Throw std::runtime_error when a key or section is missing.
std::string fmtShort(double v);                        // "1.327", "-0.8", "66"
std::string editIni(const std::string& text, const std::string& section, const std::string& key,
                    double value, bool relative, bool scaled, IniChange* change);
int countLayers(const std::string& text, const std::string& section);
// A camera section of CameraPhysics.xml: '<' of the open tag, just past its '>', and the '<' of the close tag.
struct XmlSpan { size_t start, body, end; };
XmlSpan xmlSection(const std::string& text, const std::string& name);
std::vector<std::pair<size_t, size_t>> xmlLayers(const std::string& text, size_t a, size_t b); // <Layer ... /> spans in [a, b)
bool isWs(char c);
std::string appendLayers(const std::string& text, const std::string& section,
                         const std::vector<std::string>& layers);

// Drift setup that changes nothing (after loading a camera, or resetting one).
CamParams noChangeParams(int slot);
bool isNoChange(const CamParams& p);

// The effect layers the drift setup adds to its chase camera.
std::vector<std::string> quickLayers(const CamParams& p);

// Builds both files from the stock text and the parameters.
struct Built { std::string ini, xml; std::vector<IniChange> changes; int layersBefore = 0, layersAfter = 0; };
Built build(const std::string& stockIni, const std::string& stockXml, const CamParams& p);

// File helpers (binary, byte exact).
bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);

} // namespace fh6
