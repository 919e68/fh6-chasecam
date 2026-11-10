// The editor's model of the two camera files, and the user's edits on top of
// the game's own files.
//
// Build order when saving: the game's files, then the user's key and layer
// edits (absolute values, whole sections), then the drift setup (relative
// changes and its own layers on one chase camera). Lines and layers that no
// edit touches stay byte-for-byte the same.
#pragma once
#include "core.hpp"
#include <map>
#include <string>
#include <vector>

namespace fh6 {

// ---------------------------------------------------------------------------
// CameraSettings.ini
struct IniEntry { std::string key, value; };
std::vector<IniEntry> iniEntries(const std::string& text);              // in file order
std::map<std::string, double> iniValues(const std::string& text);       // key -> number
// Sets a key's value in place, or adds the line after the last key of its block.
std::string iniSet(const std::string& text, const std::string& key, const std::string& value);

// ---------------------------------------------------------------------------
// CameraPhysics.xml
struct Attr { std::string name, value; };
struct Layer {
    std::vector<Attr> a;
    const std::string* find(const std::string& n) const;
    bool has(const std::string& n) const { return find(n) != nullptr; }
    std::string str(const std::string& n, const std::string& def = "") const;
    double num(const std::string& n, double def = 0) const;
    bool flag(const std::string& n) const { return num(n, 0) != 0; }
    void set(const std::string& n, const std::string& v); // appends if missing
    void setNum(const std::string& n, double v);           // "%.6f"
    void erase(const std::string& n);
};
Layer parseLayer(const std::string& text); // lenient: any spacing, either quote
std::string writeLayer(const Layer& l);    // <Layer a="v" ... />
// Puts the attributes in the game's order and adds or drops the optional
// groups to match the layer's switches (impulse output, Sprung, NoiseType,
// mappings, InputMagAttackDecay). Hand-made "Constant" layers are left as they are.
void normalizeLayer(Layer& l);
// Switches a curve mapping (prefix "InputToParam" or "InputToNoiseFreq"), resampling the points.
void setMapping(Layer& l, const std::string& prefix, const std::string& mapping);
struct CurvePts { int n = 0; double x[5] = {}, y[5] = {}; bool mirrored = false; };
CurvePts curvePts(const Layer& l, const std::string& prefix);
double curveEval(const CurvePts& c, double x); // mirrored = odd symmetry
std::string f6(double v);

struct XmlSection { std::string name; int declared = 0; std::vector<std::string> layers; };
std::vector<XmlSection> xmlSections(const std::string& text);
const XmlSection* findSection(const std::vector<XmlSection>& v, const std::string& name);
// Replaces a section's layers, one per line, and sets NumLayers to the real count.
std::string replaceSectionLayers(const std::string& text, const std::string& section, const std::vector<std::string>& layers);

// ---------------------------------------------------------------------------
// The user's edits
struct Edits {
    std::map<std::string, std::string> ini;                   // key -> value
    std::map<std::string, std::vector<std::string>> sections; // section -> all of its layers
    bool empty() const { return ini.empty() && sections.empty(); }
};
std::string writeEdits(const Edits& e);
bool readEdits(const std::string& text, Edits& e);

// One camera's edits: its settings and its effect-layer section (camera group index from cameraGroups()).
struct CameraChanges { int keys = 0; bool layers = false; };
CameraChanges cameraChanges(const Edits& e, int group);
void resetCamera(Edits& e, int group); // back to the loaded camera's values

// Game files + edits (no drift setup)
void applyEdits(const std::string& ini, const std::string& xml, const Edits& e, std::string& outIni, std::string& outXml);
// Game files + edits + drift setup: what Save writes.
Built buildAll(const std::string& ini, const std::string& xml, const Edits& e, const CamParams& p);

// A minimal CameraPhysics.xml with the six sections and no layers (preview without game files).
std::string emptyPhysics();

} // namespace fh6
