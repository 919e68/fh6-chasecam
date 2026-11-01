// What the editor knows about every camera setting: CameraSettings.ini keys
// and the CameraPhysics.xml layer vocabulary. Meanings come from FH1 game code
// (same key names and layer format), FH4's own CameraPhysics.xml and patterns
// in FH6 camera files; none of it is documented by Playground, so every entry
// carries a confidence level.
#pragma once
#include <string>
#include <vector>

namespace fh6 {

enum class Conf { High, Med, Low };
const char* confTag(Conf c);  // "KNOWN", "LIKELY", "UNSURE"
const char* confHelp(Conf c); // one line for tooltips

// Key flags
enum : unsigned {
    K_FOV = 1,        // vertical FOV in degrees: show horizontal equivalent
    K_SPRING_K = 2,   // spring stiffness; pair = matching damping key
    K_SPRING_D = 4,   // spring damping; pair = matching stiffness key
    K_HEIGHT = 8,     // HeightMin/MaxAboveGround: the real chase height pair
    K_PERCAR = 16,    // per-car camera data replaces this value for many cars
    K_INT = 32,       // whole numbers
};

struct KeyInfo {
    std::string key;    // full path, e.g. "FollowLowCam\\Spring\\YRotSpringK"
    std::string group;  // section title inside the camera page
    std::string label;  // short name for the UI
    std::string unit;   // printed after the value, e.g. " m"
    double min, max, step;
    int dec;
    Conf conf;
    std::string help;
    bool listed;        // false = not in FH6's file (FH1/FH2 key), untested
    double def;         // value for the built-in preview file and for new keys
    unsigned flags;
    std::string pair;   // matching key for springs (K <-> D)
};

struct CameraGroup {
    const char* name;   // "Chase Near"
    const char* blurb;  // one line under the title
    int slot;           // 0/1 = chase near/far (preview follows it), -1 = none
    std::vector<KeyInfo> keys;
};

const std::vector<CameraGroup>& cameraGroups();
const KeyInfo* findKey(const std::string& full);
std::string builtinIni(); // every listed key at its default, in the game's order

// ---------------------------------------------------------------------------
// CameraPhysics.xml vocabulary
enum : unsigned {
    E_LEGACY = 1,    // FH4 only, not in FH6's file: untested
    E_MODONLY = 2,   // only in hand-made mod layers: may not be recognised
    E_COLLISION = 4, // no InputTakeDerivative attribute
    E_NOCLAMP = 8,   // no clamp attributes
    E_IMPULSE = 16,  // output kicks a spring instead of setting an offset
    E_SIM = 32,      // the preview has a value for this input
};

struct InputInfo {
    const char* name;
    const char* label;
    const char* unit;
    const char* help;
    double clampMin, clampMax; // typical range, used for new layers
    unsigned flags;
};
struct OutputInfo {
    const char* name;
    const char* label;
    const char* help;
    const char* comp[4]; // label per Param component, nullptr = unused
    const char* unit;    // unit of the components
    unsigned flags;
};
struct ChoiceInfo { const char* name; const char* label; const char* help; unsigned flags; };
struct SectionInfo { const char* name; const char* label; int slot; };

const std::vector<InputInfo>& inputs();
const std::vector<OutputInfo>& outputs();
const std::vector<ChoiceInfo>& noiseTypes();
const std::vector<ChoiceInfo>& mappings();      // InputToParamMapping
const std::vector<ChoiceInfo>& noiseMappings(); // InputToNoiseFreqMapping (adds Abs)
const std::vector<ChoiceInfo>& locations();
const std::vector<SectionInfo>& sections();

int inputIndex(const std::string& name);  // -1 if unknown
int outputIndex(const std::string& name); // -1 if unknown
int choiceIndex(const std::vector<ChoiceInfo>& v, const std::string& name);
const char* inputLabel(const std::string& name);  // falls back to the raw name
const char* outputLabel(const std::string& name);

} // namespace fh6
