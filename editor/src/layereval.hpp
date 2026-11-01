// Runs a camera section's effect layers on preview inputs, the way the game
// most likely does: input -> [rate of change] -> clamp -> [smoothed magnitude]
// -> curve -> [x noise] -> x Param -> [spring] -> summed per output.
// The order and units are inferred, not documented; impulse outputs and inputs
// the preview can't produce (collisions, wind, head tracking) contribute nothing.
#pragma once
#include "camfiles.hpp"
#include <string>
#include <vector>

namespace fh6 {

struct InputFrame {
    std::vector<double> v;  // per inputs() entry
    std::vector<char> sim;  // 1 = the preview has a real value
    InputFrame();
};

struct LayerOut {
    double carXYZ[3] = {}, carYPR[3] = {}, camXYZ[3] = {}, camYPR[3] = {};
    double fov = 0, vignette = 0, wheel = 0;
};

// What one layer is doing right now, for the editor's readout.
struct LayerProbe {
    bool active = false;    // evaluated (enabled, known input and output, not an impulse)
    bool simulated = false; // its input has a real value in the preview
    double input = 0, x = 0, factor = 0, out[4] = {};
};

class LayerStack {
public:
    void load(const std::vector<std::string>& layers);
    LayerOut step(const InputFrame& in, double dt);
    int size() const { return (int)defs_.size(); }
    const LayerProbe& probe(int i) const { return probes_[i]; }

private:
    struct Def {
        bool enabled = false, island = false, deriv = false, clamp = false, env = false, sprung = false, impulse = false;
        int input = -1, output = -1, mapping = 0, noise = 0, noiseMap = 0;
        double cmin = 0, cmax = 0, attack = 0, decay = 0, k = 0, d = 1, param[4] = {};
        CurvePts curve, ncurve;
        double nfreq = 0, nseed = 0, nmin = -1, nmax = 1;
        std::string sig; // input+output, to keep spring state across reloads
    };
    struct State { bool hasPrev = false; double prev = 0, env = 0, phase = 0, x[4] = {}, v[4] = {}; };
    std::vector<Def> defs_;
    std::vector<State> states_;
    std::vector<LayerProbe> probes_;
};

} // namespace fh6
