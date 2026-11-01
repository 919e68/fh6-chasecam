// Camera settings page (every CameraSettings.ini key, per camera) and effect
// layers page (every CameraPhysics.xml layer, per section).
#include "app.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "schema.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace ui;
using fh6::KeyInfo;
static const double PI = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// helpers
static int groupOfKey(const std::string& key) {
    auto& gs = fh6::cameraGroups();
    for (size_t i = 0; i < gs.size(); i++)
        for (auto& k : gs[i].keys)
            if (k.key == key) return (int)i;
    // unknown key: the group whose keys share its first path segment
    std::string seg = key.substr(0, key.find('\\'));
    for (size_t i = 0; i < gs.size(); i++)
        for (auto& k : gs[i].keys)
            if (k.key.compare(0, seg.size() + 1, seg + "\\") == 0) return (int)i;
    return (int)gs.size() - 1;
}

static std::string springText(double k, double zeta) {
    char b[160];
    const char* feel = zeta < 0.45 ? "swings past the target, then settles" : zeta < 0.95 ? "a little overshoot" : "settles without overshoot";
    std::snprintf(b, sizeof b, "%.2f Hz, damping ratio %.2f: %s.", std::sqrt(std::max(k, 0.0)) / (2 * PI), zeta, feel);
    return b;
}

static double hfov(double v) { return 2 * std::atan(std::tan(v * PI / 360) * 16.0 / 9.0) * 180 / PI; }

static std::string num(double v, int dec = 3) {
    char b[48];
    fmtNum(b, sizeof b, v, dec, false);
    return b;
}

// label + value pair in the details card
static void infoRow(const char* label, const std::string& value, unsigned color = INK) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    drawText(dl, ImVec2(p.x, p.y + 2 * S), Label, INK_MUTED, label);
    drawText(dl, ImVec2(p.x + w - textWidth(Stat, value.c_str()), p.y), Stat, color, value.c_str());
    ImGui::Dummy(ImVec2(w, 18 * S));
}

// Group title limited to a column's width
static void colTitle(const char* t, bool first, float w) {
    if (!first) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + SP1 * S), ImVec2(p.x + w, p.y + SP1 * S + HAIR * S), rgb(LINE));
        ImGui::Dummy(ImVec2(0, SP3 * S));
    }
    text(Nav, INK, t);
}

static void hair() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + HAIR * S), rgb(LINE));
    ImGui::Dummy(ImVec2(0, SP1 * S));
}

// ---------------------------------------------------------------------------
// Camera settings
double App::keyValue(const std::string& key, bool* inBase) const {
    auto b = baseVals.find(key);
    if (inBase) *inBase = b != baseVals.end();
    auto e = edits.ini.find(key);
    if (e != edits.ini.end()) return std::strtod(e->second.c_str(), nullptr);
    if (b != baseVals.end()) return b->second;
    const KeyInfo* k = fh6::findKey(key);
    return k ? k->def : 0;
}

void App::setKey(const std::string& key, double v) {
    auto b = baseVals.find(key);
    if (b != baseVals.end() && std::fabs(b->second - v) < 5e-5) edits.ini.erase(key); // back to the game's value
    else edits.ini[key] = fh6::fmtShort(v);
    editsChanged();
}

void App::settingsPage(float w, float h) {
    ImVec2 o = ImGui::GetCursorScreenPos();
    float g = SP4 * S, navW = 176 * S;
    float rightW = std::clamp(w * 0.32f, 280 * S, 420 * S);
    float keysW = w - navW - rightW - 2 * g;
    ImGui::SetCursorScreenPos(o);
    settingsNav(navW, h);
    ImGui::SetCursorScreenPos(ImVec2(o.x + navW + g, o.y));
    settingsKeys(keysW, h);
    float rx = o.x + navW + g + keysW + g, ph = std::floor(rightW * 9 / 16);
    ImGui::SetCursorScreenPos(ImVec2(rx, o.y));
    previewCard(rightW, ph, true);
    ImGui::SetCursorScreenPos(ImVec2(rx, o.y + ph + g));
    settingsDetails(rightW, h - ph - g);
}

void App::settingsNav(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    panel(dl, a, b, false);
    auto& gs = fh6::cameraGroups();
    std::vector<int> counts(gs.size(), 0);
    for (auto& kv : edits.ini) counts[groupOfKey(kv.first)]++;
    float y = a.y + SP3 * S, rh = 34 * S;
    for (size_t i = 0; i < gs.size(); i++) {
        ImVec2 r0(a.x + 1 * S, y), r1(b.x - 1 * S, y + rh);
        ImGui::SetCursorScreenPos(r0);
        ImGui::PushID((int)i);
        if (ImGui::InvisibleButton("##cam", ImVec2(r1.x - r0.x, rh)) && setCam != (int)i) { setCam = (int)i; selKey.clear(); }
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        bool sel = setCam == (int)i;
        if (sel) { dl->AddRectFilled(r0, r1, rgb(BG300)); dl->AddRectFilled(r0, ImVec2(r0.x + RULE * S, r1.y), rgb(ORANGE)); }
        drawText(dl, ImVec2(a.x + SP4 * S, y + (rh - 13 * S) / 2 - 1 * S), Nav, sel || hov ? INK : INK_MUTED, gs[i].name);
        if (counts[i]) {
            char c[16];
            std::snprintf(c, sizeof c, "%d", counts[i]);
            drawText(dl, ImVec2(b.x - SP4 * S - textWidth(Stat, c), y + (rh - 13 * S) / 2 - 1 * S), Stat, ORANGE_HOT, c);
        }
        y += rh;
    }
    // what the numbers mean
    float ty = std::max(y + SP4 * S, b.y - 96 * S);
    ImGui::SetCursorScreenPos(ImVec2(a.x + SP4 * S, ty));
    textWrapped(Caption, INK_MUTED,
                baseReal ? "Orange numbers: settings you changed. Saved into the game with Save to game."
                         : "No game camera files yet, so these are typical values. Choose your FH6 folder to edit the game's own.",
                w - 2 * SP4 * S);
}

void App::settingsKeys(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    panel(dl, a, b, false);
    const fh6::CameraGroup& G = fh6::cameraGroups()[setCam];
    float pad = SP4 * S;
    drawText(dl, ImVec2(a.x + pad, a.y + pad - 2 * S), Title, INK, G.name);
    // reset this camera
    bool any = false;
    for (auto& kv : edits.ini) if (groupOfKey(kv.first) == setCam) { any = true; break; }
    float rw = buttonWidth("Reset camera", Btn::Ghost);
    ImGui::SetCursorScreenPos(ImVec2(b.x - pad - rw + SP3 * S, a.y + pad - 12 * S));
    if (button("Reset camera", Btn::Ghost, false, rw, any)) {
        for (auto it = edits.ini.begin(); it != edits.ini.end();) it = groupOfKey(it->first) == setCam ? edits.ini.erase(it) : std::next(it);
        editsChanged();
    }
    helpTooltip("Puts every setting of this camera back to the game's value.");
    ImGui::SetCursorScreenPos(ImVec2(a.x + pad, a.y + pad + 22 * S));
    textWrapped(Caption, INK_MUTED, G.blurb, w - 2 * pad);
    float top = pad + 22 * S + 24 * S;

    ImGui::SetCursorScreenPos(ImVec2(a.x + 1 * S, a.y + top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad - 1 * S, 0));
    ImGui::BeginChild("##keys", ImVec2(w - 2 * S, h - top - 2 * S), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SP2 * S, SP3 * S));
    ImDrawList* cl = ImGui::GetWindowDrawList();
    std::string group;
    bool first = true;
    auto row = [&](const std::string& key, const char* label, const char* unit, double mn, double mx, double step, int dec, double base,
                   const char* help) {
        bool inBase;
        double v = keyValue(key, &inBase);
        bool edited = edits.ini.count(key) > 0;
        ImVec2 r0 = ImGui::GetCursorScreenPos();
        float rw2 = ImGui::GetContentRegionAvail().x;
        double lo = std::min(mn, v), hi = std::max(mx, v);
        (void)help; // shown in the details card instead of a tooltip
        if (slider(key.c_str(), label, &v, lo, hi, step, unit, dec, false, base, nullptr, false, &base)) setKey(key, v);
        ImVec2 r1(r0.x + rw2, r0.y + 34 * S);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(r0, r1)) selKey = key;
        if (edited) cl->AddRectFilled(ImVec2(r0.x - 9 * S, r0.y + 4 * S), ImVec2(r0.x - 5 * S, r0.y + 8 * S), rgb(ORANGE));
        // value shown in the label colour orange when changed
        if (edited) {
            char nb[48];
            fmtNum(nb, sizeof nb, v, dec, false);
            float uw = unit && *unit ? textWidth(Stat, unit) : 0;
            drawText(cl, ImVec2(r1.x - uw - textWidth(Stat, nb), r0.y - 1 * S), Stat, ORANGE_HOT, nb);
        }
        auto dc = driftChange.find(key);
        if (dc != driftChange.end()) {
            std::string t = "Drift setup: " + num(dc->second, dec);
            drawText(cl, ImVec2(r0.x + textWidth(Label, label) + SP3 * S, r0.y + 1 * S), Key, ORANGE_HOT, t.c_str());
        }
        if (key == selKey) cl->AddRectFilled(ImVec2(r0.x - 16 * S + 1 * S, r0.y - 2 * S), ImVec2(r0.x - 16 * S + 1 * S + RULE * S, r1.y), rgb(LINE_STRONG));
    };
    // Chase cameras: one control for the real height (min and max together)
    if (G.slot >= 0) {
        groupTitle("Quick", true);
        first = false;
        std::string pre = G.slot ? "FollowHighCam\\" : "FollowLowCam\\";
        double lo = keyValue(pre + "HeightMinAboveGround"), hi = keyValue(pre + "HeightMaxAboveGround");
        double v = (lo + hi) / 2, base = 0;
        auto bl = baseVals.find(pre + "HeightMinAboveGround"), bh = baseVals.find(pre + "HeightMaxAboveGround");
        base = bl != baseVals.end() && bh != baseVals.end() ? (bl->second + bh->second) / 2 : v;
        ImVec2 r0 = ImGui::GetCursorScreenPos();
        if (slider("height-pair", "Camera height", &v, std::min(0.3, v), std::max(5.0, v), 0.01, " m", 2, false, base,
                   "Sets the lowest and highest height together, which is how the game's file pins the chase height.", false, &base)) {
            double d = v - (lo + hi) / 2;
            setKey(pre + "HeightMinAboveGround", lo + d);
            setKey(pre + "HeightMaxAboveGround", hi + d);
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(r0, ImVec2(r0.x + ImGui::GetContentRegionAvail().x, r0.y + 34 * S)))
            selKey = pre + "HeightMinAboveGround";
    }
    std::vector<const KeyInfo*> addable;
    for (auto& k : G.keys) {
        bool inBase = baseVals.count(k.key) > 0, edited = edits.ini.count(k.key) > 0;
        if (!inBase && !edited) { addable.push_back(&k); continue; }
        if (k.group != group) { groupTitle(k.group.c_str(), first); first = false; group = k.group; }
        double base = inBase ? baseVals.at(k.key) : k.def;
        row(k.key, k.label.c_str(), k.unit.c_str(), k.min, k.max, k.step, k.dec, base, k.help.c_str());
    }
    // keys in the game's file the editor doesn't know
    std::vector<std::string> unknown;
    for (auto& kv : baseVals) if (!fh6::findKey(kv.first) && groupOfKey(kv.first) == setCam) unknown.push_back(kv.first);
    for (auto& kv : edits.ini) if (!fh6::findKey(kv.first) && !baseVals.count(kv.first) && groupOfKey(kv.first) == setCam) unknown.push_back(kv.first);
    if (!unknown.empty()) {
        groupTitle("Other keys in this file", first);
        first = false;
        for (auto& k : unknown) {
            double base = baseVals.count(k) ? baseVals.at(k) : 0, v = keyValue(k);
            double span = std::max(1.0, std::fabs(base) * 2);
            row(k, k.substr(k.rfind('\\') + 1).c_str(), "", base - span, base + span, span / 200, 3, base, "Not documented anywhere. Test in game.");
        }
    }
    // keys the game may read that its file leaves out
    if (!addable.empty()) {
        groupTitle(baseReal ? "Add a key the file leaves out" : "Keys not in the typical file", first);
        textWrapped(Caption, INK_MUTED, "Read by older Forza games or missing from this file. They may do nothing in FH6.", ImGui::GetContentRegionAvail().x);
        for (auto* k : addable) {
            ImVec2 r0 = ImGui::GetCursorScreenPos();
            float rw2 = ImGui::GetContentRegionAvail().x, bw = buttonWidth("Add", Btn::Ghost);
            drawText(cl, ImVec2(r0.x, r0.y + 6 * S), Label, INK_MUTED, k->label.c_str());
            drawText(cl, ImVec2(r0.x, r0.y + 22 * S), Caption, INK_DISABLED, k->key.c_str());
            ImGui::SetCursorScreenPos(ImVec2(r0.x + rw2 - bw + SP3 * S, r0.y));
            ImGui::PushID(k->key.c_str());
            if (button("Add", Btn::Ghost, false, bw)) { edits.ini[k->key] = fh6::fmtShort(k->def); selKey = k->key; editsChanged(); }
            ImGui::PopID();
            helpTooltip(k->help.c_str());
            if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(r0, ImVec2(r0.x + rw2, r0.y + 40 * S))) selKey = k->key;
            ImGui::SetCursorScreenPos(ImVec2(r0.x, r0.y + 40 * S));
            ImGui::Dummy(ImVec2(0, 0));
        }
    }
    ImGui::Dummy(ImVec2(0, SP2 * S));
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void App::settingsDetails(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    panel(dl, a, b, false);
    float pad = SP4 * S;
    ImGui::SetCursorScreenPos(ImVec2(a.x + 1 * S, a.y + pad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad - 1 * S, 0));
    ImGui::BeginChild("##details", ImVec2(w - 2 * S, h - pad - 2 * S), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SP2 * S, SP2 * S));
    float cw = ImGui::GetContentRegionAvail().x;
    if (selKey.empty() || groupOfKey(selKey) != setCam) {
        text(Nav, INK, "How this page works");
        textWrapped(Body, INK_MUTED,
                    "Every setting in CameraSettings.ini for this camera. Point at one to see what it does here. "
                    "Right-click a slider to put back the game's value, double-click to type any value.",
                    cw);
        textWrapped(Body, INK_MUTED,
                    "Known: decoded from Forza game code with the same names. Likely: worked out from the values. "
                    "Unsure: a guess, test it in game.",
                    cw);
        if (fh6::cameraGroups()[setCam].slot >= 0)
            textWrapped(Body, INK_MUTED, "The preview above follows this chase camera and runs its effect layers.", cw);
        else
            textWrapped(Body, INK_MUTED, "The preview shows chase cameras only.", cw);
    } else {
        const KeyInfo* k = fh6::findKey(selKey);
        bool inBase;
        double v = keyValue(selKey, &inBase);
        bool edited = edits.ini.count(selKey) > 0;
        size_t cut = selKey.rfind('\\');
        text(Nav, INK, k ? k->label.c_str() : selKey.substr(cut + 1).c_str());
        text(Caption, INK_MUTED, selKey.substr(0, cut + 1).c_str());
        text(Stat, INK, selKey.substr(cut + 1).c_str());
        // tags
        ImVec2 tp = ImGui::GetCursorScreenPos();
        float tx = tp.x;
        auto addTag = [&](Tone t, const char* s) { tag(dl, ImVec2(tx, tp.y), t, s); tx += tagWidth(s) + SP2 * S; };
        if (k) addTag(k->conf == fh6::Conf::High ? Tone::Cool : k->conf == fh6::Conf::Med ? Tone::Neutral : Tone::Danger, fh6::confTag(k->conf));
        if (edited) addTag(Tone::Orange, "Changed");
        if (!inBase) addTag(Tone::Neutral, "Not in file");
        if (k && (k->flags & fh6::K_PERCAR)) addTag(Tone::Neutral, "Per-car");
        ImGui::Dummy(ImVec2(cw, 22 * S));
        if (k) textWrapped(Body, INK, k->help.c_str(), cw);
        if (k) textWrapped(Caption, INK_MUTED, fh6::confHelp(k->conf), cw);
        if (k && (k->flags & fh6::K_PERCAR))
            textWrapped(Caption, INK_MUTED, "Over 100 cars have their own camera data in the game's database, which replaces this value for them.", cw);
        hair();
        const char* unit = k ? k->unit.c_str() : "";
        int dec = k ? std::max(k->dec, 2) : 3;
        infoRow("Game file", inBase ? num(baseVals.at(selKey), dec) + unit : "not in file", INK_MUTED);
        infoRow("Your value", num(v, dec) + unit, edited ? ORANGE_HOT : INK);
        auto fv = finalVals.find(selKey);
        if (fv != finalVals.end() && std::fabs(fv->second - v) > 1e-9) infoRow("With drift setup", num(fv->second, dec) + unit, ORANGE_HOT);
        if (k) infoRow("Typical range", num(k->min, k->dec) + " to " + num(k->max, k->dec) + unit, INK_MUTED);
        if (k && (k->flags & fh6::K_FOV)) infoRow("Horizontal at 16:9", num(hfov(fv != finalVals.end() ? fv->second : v), 1) + "\xC2\xB0");
        if (k && (k->flags & (fh6::K_SPRING_K | fh6::K_SPRING_D)) && !k->pair.empty()) {
            double kk = (k->flags & fh6::K_SPRING_K) ? v : keyValue(k->pair), dd = (k->flags & fh6::K_SPRING_D) ? v : keyValue(k->pair);
            textWrapped(Caption, INK_MUTED, ("With its partner setting: " + springText(kk, dd / (2 * std::sqrt(std::max(kk, 1e-6))))).c_str(), cw);
        }
        if (k && (k->flags & fh6::K_HEIGHT)) {
            std::string pre = selKey.substr(0, cut + 1);
            double lo = keyValue(pre + "HeightMinAboveGround"), hi = keyValue(pre + "HeightMaxAboveGround");
            if (lo > hi + 1e-9) textWrapped(Caption, DANGER, "The lowest height is above the highest. Keep lowest at or below highest.", cw);
        }
        if (edited) {
            ImGui::Dummy(ImVec2(0, SP1 * S));
            if (button(inBase ? "Use game value" : "Remove key", Btn::Secondary)) { edits.ini.erase(selKey); editsChanged(); }
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Effect layers
static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string layerTitle(const fh6::Layer& l) {
    return std::string(fh6::inputLabel(l.str("InputType"))) + "  \xE2\x80\xBA  " + fh6::outputLabel(l.str("OutputType"));
}

static std::string layerSummary(const fh6::Layer& l) {
    std::string s;
    auto add = [&](const std::string& t) { s += (s.empty() ? "" : "  \xC2\xB7  ") + t; };
    std::string m = l.str("InputToParamMapping");
    int mi = fh6::choiceIndex(fh6::mappings(), m);
    if (mi >= 0) add(fh6::mappings()[mi].label);
    std::string n = l.str("NoiseType", "None");
    if (n != "None") add(std::string(fh6::noiseTypes()[std::max(0, fh6::choiceIndex(fh6::noiseTypes(), n))].label) + " noise");
    if (l.flag("Sprung") || l.str("OutputType").find("Impulse") != std::string::npos) add("spring");
    if (l.flag("InputMagAttackDecay")) add("smoothed");
    std::string loc = l.str("LocationType", "All");
    if (loc != "All") add(loc == "IslandOnly" ? "island only" : "mainland only");
    // the biggest Param
    double big = 0;
    char ax = 'x';
    for (char c : std::string("xyz")) {
        double v = l.num(std::string("Param.") + c);
        if (std::fabs(v) > std::fabs(big)) { big = v; ax = c; }
    }
    if (big != 0) {
        int oi = fh6::outputIndex(l.str("OutputType"));
        const char* cn = oi >= 0 ? fh6::outputs()[oi].comp[ax - 'x'] : nullptr;
        add(std::string(cn ? cn : "") + " " + num(big, std::fabs(big) < 0.1 ? 3 : 2) + (oi >= 0 ? fh6::outputs()[oi].unit : ""));
    }
    return s;
}

static fh6::Layer makeLayer(const char* in, const char* out) {
    fh6::Layer l;
    l.set("Enabled", "1");
    l.set("LocationType", "All");
    l.set("InputType", in);
    l.set("NoiseType", "None");
    l.set("OutputType", out);
    int ii = fh6::inputIndex(in);
    if (ii >= 0) { l.setNum("InputClampMin", fh6::inputs()[ii].clampMin); l.setNum("InputClampMax", fh6::inputs()[ii].clampMax); }
    l.set("InputToParamMapping", "Linear");
    fh6::normalizeLayer(l);
    return l;
}

static void setCurve(fh6::Layer& l, std::initializer_list<fh6::Pt> pts, bool mirrored) {
    fh6::setMapping(l, "InputToParam", pts.size() == 5 ? "FivePoint" : "ThreePoint");
    int i = 0;
    for (auto& p : pts) {
        l.setNum("InputToParamCurveInput" + std::to_string(i), p.x);
        l.setNum("InputToParamCurveOutput" + std::to_string(i), p.y);
        i++;
    }
    l.set("InputToParamCurveMirrored", mirrored ? "1" : "0");
}

static void setSpring(fh6::Layer& l, double k, double d) {
    l.set("Sprung", "1");
    fh6::normalizeLayer(l);
    l.setNum("SpringK", k);
    l.setNum("SpringDPercent", d);
}

struct Template { const char* name; const char* help; };
static const Template TEMPLATES[] = {
    { "Fixed offset", "Always on: move or turn the camera by a set amount." },
    { "Drift look", "Turns the view as the tyres slide sideways." },
    { "Corner lean", "Rolls the view with cornering force." },
    { "Brake dive", "Tips the view down when braking." },
    { "Speed field of view", "Widens the view with speed." },
    { "Speed shake", "Fine shake that grows with speed." },
    { "Landing jolt", "A dip after hard landings that fades." },
    { "Idle sway", "Slow handheld sway while stopped." },
};

static fh6::Layer templateLayer(int t) {
    fh6::Layer l;
    switch (t) {
    case 0:
        l = makeLayer("ConstantOne", "CarSpaceXYZOffset");
        fh6::setMapping(l, "InputToParam", "ConstantOne");
        break;
    case 1:
        l = makeLayer("Slip_Lateral", "CameraSpaceYPROffset");
        l.setNum("InputClampMin", -8);
        l.setNum("InputClampMax", 8);
        setCurve(l, { { 1, 0 }, { 2.5, 0.6 }, { 6, 1 } }, true);
        l.setNum("Param.x", 6);
        setSpring(l, 10, 1);
        break;
    case 2:
        l = makeLayer("Gs_Lateral", "CarSpaceYPROffset");
        setCurve(l, { { 0, 0 }, { 0.5, 0.6 }, { 1, 1 } }, true);
        l.setNum("Param.z", 2);
        setSpring(l, 60, 0.8);
        break;
    case 3:
        l = makeLayer("Gs_Longitudinal", "CarSpaceYPROffset");
        l.setNum("InputClampMin", -1.5);
        l.setNum("InputClampMax", 0);
        setCurve(l, { { -1.5, -1 }, { -0.75, -0.6 }, { 0, 0 } }, false);
        l.setNum("Param.y", 2);
        setSpring(l, 80, 0.7);
        break;
    case 4:
        l = makeLayer("SpeedMPH", "FOV");
        l.setNum("InputClampMax", 200);
        setCurve(l, { { 40, 0 }, { 70, 0.35 }, { 110, 0.7 }, { 150, 0.9 }, { 200, 1 } }, false);
        l.setNum("Param.x", 8);
        setSpring(l, 20, 1);
        break;
    case 5:
        l = makeLayer("SpeedMPH", "CameraSpaceYPROffset");
        l.set("NoiseType", "Simplex");
        fh6::normalizeLayer(l);
        l.setNum("NoiseFreq", 18);
        l.setNum("NoiseSeed", 7);
        setCurve(l, { { 60, 0 }, { 120, 0.5 }, { 200, 1 } }, false);
        l.setNum("Param.x", 0.15);
        l.setNum("Param.y", 0.25);
        l.setNum("Param.z", 0.1);
        break;
    case 6:
        l = makeLayer("RecentLandingScore", "CarSpaceXYZOffset");
        l.set("InputMagAttackDecay", "1");
        fh6::normalizeLayer(l);
        l.setNum("InputAttack", 1000);
        l.setNum("InputDecay", 2.6);
        setCurve(l, { { 0.05, 0 }, { 0.5, 0.6 }, { 1, 1 } }, false);
        l.setNum("Param.y", -0.25);
        setSpring(l, 140, 0.55);
        break;
    default:
        l = makeLayer("OneWhenStationary", "CameraSpaceYPROffset");
        l.set("NoiseType", "Perlin");
        fh6::normalizeLayer(l);
        l.setNum("NoiseFreq", 0.22);
        l.setNum("NoiseSeed", 31);
        l.setNum("Param.x", 0.5);
        l.setNum("Param.y", 0.3);
        break;
    }
    fh6::normalizeLayer(l);
    return l;
}

void App::syncWork() {
    const std::string sec = fh6::sections()[laySec].name;
    if (workFor == sec) return;
    std::vector<std::string> texts;
    auto e = edits.sections.find(sec);
    if (e != edits.sections.end()) texts = e->second;
    else if (const fh6::XmlSection* s = fh6::findSection(baseSecs, sec)) texts = s->layers;
    work.clear();
    for (auto& t : texts) work.push_back(fh6::parseLayer(t));
    workText = texts;
    workMine.assign(texts.size(), 0);
    workFor = sec;
    if (laySel >= (int)work.size()) laySel = work.empty() ? -1 : (int)work.size() - 1;
}

void App::commitWork() {
    for (size_t i = 0; i < work.size(); i++)
        if (workMine[i]) workText[i] = fh6::writeLayer(work[i]);
    const fh6::XmlSection* s = fh6::findSection(baseSecs, workFor);
    if (s && s->layers == workText) edits.sections.erase(workFor);
    else edits.sections[workFor] = workText;
    editsChanged();
}

void App::layerChanged(int i) {
    if (i < 0 || i >= (int)work.size()) return;
    fh6::normalizeLayer(work[i]);
    workMine[i] = 1;
    commitWork();
}

void App::layersPage(float w, float h) {
    syncWork();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float g = SP4 * S, listW = std::clamp(w * 0.36f, 320 * S, 430 * S);
    ImGui::SetCursorScreenPos(o);
    layerList(listW, h);
    ImGui::SetCursorScreenPos(ImVec2(o.x + listW + g, o.y));
    layerEditor(w - listW - g, h);
}

void App::layerList(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    panel(dl, a, b, false);
    float pad = SP4 * S, cw = w - 2 * pad;
    const auto& secs = fh6::sections();
    const std::string sec = secs[laySec].name;
    bool driftHere = sec == fh6::SLOTS[p.slot ? 1 : 0].xml;
    std::vector<fh6::Layer> drift;
    if (driftHere)
        for (auto& t : fh6::quickLayers(p)) drift.push_back(fh6::parseLayer(t));

    // section picker
    std::vector<std::string> names;
    std::vector<const char*> labels;
    for (auto& s : secs) {
        const fh6::XmlSection* bs = fh6::findSection(baseSecs, s.name);
        auto e = edits.sections.find(s.name);
        size_t n = e != edits.sections.end() ? e->second.size() : bs ? bs->layers.size() : 0;
        names.push_back(std::string(s.label) + "   " + std::to_string(n) + " layers" + (e != edits.sections.end() ? ", changed" : ""));
    }
    for (auto& n : names) labels.push_back(n.c_str());
    ImGui::SetCursorScreenPos(ImVec2(a.x + pad, a.y + pad));
    int ls = laySec;
    if (combo("section", &ls, labels.data(), (int)labels.size(), cw)) { laySec = ls; laySel = -1; syncWork(); }
    // search
    ImGui::SetCursorScreenPos(ImVec2(a.x + pad, a.y + pad + (ROW_H + SP2) * S));
    searchBox("##filter", layFilter, sizeof layFilter, "Filter by input or output", cw);
    float listTop = pad + 2 * (ROW_H + SP2) * S + SP1 * S, toolH = ROW_H * S + 2 * pad;

    if (!baseReal) {
        ImGui::SetCursorScreenPos(ImVec2(a.x + pad, a.y + listTop + SP2 * S));
        textWrapped(Body, INK_MUTED,
                    "Choose your FH6 folder (bottom left) to load the game's effect layers. They come from its CameraPhysics.xml.", cw);
        return;
    }
    // rows
    std::string f = lower(layFilter);
    std::vector<int> rows; // index into work, or work.size()+j for drift layers
    auto match = [&](const fh6::Layer& l) {
        if (f.empty()) return true;
        std::string s = lower(l.str("InputType") + " " + l.str("OutputType") + " " + fh6::inputLabel(l.str("InputType")) + " " +
                              fh6::outputLabel(l.str("OutputType")) + " " + l.str("NoiseType"));
        return s.find(f) != std::string::npos;
    };
    for (size_t i = 0; i < work.size(); i++) if (match(work[i])) rows.push_back((int)i);
    for (size_t j = 0; j < drift.size(); j++) if (match(drift[j])) rows.push_back((int)(work.size() + j));

    ImGui::SetCursorScreenPos(ImVec2(a.x + 1 * S, a.y + listTop));
    ImGui::BeginChild("##layers", ImVec2(w - 2 * S, h - listTop - toolH));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImDrawList* cl = ImGui::GetWindowDrawList();
    float rh = 42 * S, rw = ImGui::GetContentRegionAvail().x;
    bool previewing = secs[laySec].slot == stackSlot && secs[laySec].slot >= 0;
    ImGuiListClipper clip;
    clip.Begin((int)rows.size(), rh);
    if (layScrollTo >= 0) {
        for (size_t r = 0; r < rows.size(); r++)
            if (rows[r] == layScrollTo) { clip.IncludeItemByIndex((int)r); break; }
    }
    while (clip.Step()) {
        for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
            int idx = rows[r];
            bool isDrift = idx >= (int)work.size();
            const fh6::Layer& l = isDrift ? drift[idx - work.size()] : work[idx];
            ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + rw, p0.y + rh);
            ImGui::PushID(idx);
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::InvisibleButton("##row", ImVec2(rw, rh))) laySel = idx;
            bool hov = ImGui::IsItemHovered();
            // enable square, on top of the row
            bool on = l.flag("Enabled");
            ImVec2 ea(p0.x + pad - 2 * S, p0.y + 9 * S), eb(ea.x + 12 * S, ea.y + 12 * S);
            ImGui::SetCursorScreenPos(ImVec2(ea.x - 4 * S, ea.y - 4 * S));
            if (ImGui::InvisibleButton("##en", ImVec2(20 * S, 20 * S)) && !isDrift) {
                work[idx].set("Enabled", on ? "0" : "1");
                layerChanged(idx);
                on = !on;
            }
            bool enHov = ImGui::IsItemHovered();
            if (enHov) {
                hov = false;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                helpTooltip(isDrift ? "Set by the Drift setup page." : "Turn this layer on or off.");
            }
            ImGui::PopID();
            if (layScrollTo == idx) { ImGui::SetScrollHereY(0.5f); layScrollTo = -1; }
            bool sel = laySel == idx;
            if (sel || hov) cl->AddRectFilled(p0, p1, rgb(sel ? BG300 : BG200));
            if (sel) cl->AddRectFilled(p0, ImVec2(p0.x + RULE * S, p1.y), rgb(ORANGE));
            if (on) cl->AddRectFilled(ea, eb, rgb(isDrift ? ORANGE_HOT : ORANGE));
            else cl->AddRect(ea, eb, rgb(enHov ? INK : LINE_STRONG), 0, HAIR * S);
            float tx = eb.x + SP3 * S;
            char ix[16];
            std::snprintf(ix, sizeof ix, "%d", idx + 1);
            float iw = textWidth(Key, "000");
            drawText(cl, ImVec2(tx, p0.y + 8 * S), Key, INK_DISABLED, ix);
            tx += iw + SP2 * S;
            cl->PushClipRect(ImVec2(tx, p0.y), ImVec2(p1.x - SP3 * S, p1.y), true);
            drawText(cl, ImVec2(tx, p0.y + 4 * S), Body, on ? INK : INK_DISABLED, layerTitle(l).c_str());
            std::string sum = isDrift ? "Drift setup  \xC2\xB7  " + layerSummary(l) : layerSummary(l);
            drawText(cl, ImVec2(tx, p0.y + 22 * S), Caption, isDrift ? ORANGE_HOT : INK_MUTED, sum.c_str());
            cl->PopClipRect();
            // live activity dot: the layer is moving the camera right now
            if (previewing && idx < stack.size()) {
                const fh6::LayerProbe& pr = stack.probe(idx);
                double mag = std::fabs(pr.out[0]) + std::fabs(pr.out[1]) + std::fabs(pr.out[2]);
                if (pr.active && mag > 1e-3) {
                    float al = (float)std::min(1.0, mag * 4);
                    cl->AddRectFilled(ImVec2(p1.x - 10 * S, p0.y + 9 * S), ImVec2(p1.x - 6 * S, p0.y + 13 * S), rgb(COOL, (int)(80 + 175 * al)));
                }
            }
            cl->AddRectFilled(ImVec2(p0.x + pad, p1.y - HAIR * S), ImVec2(p1.x, p1.y), rgb(LINE));
            ImGui::SetCursorScreenPos(p0);
            ImGui::Dummy(ImVec2(rw, rh));
        }
    }
    if (rows.empty()) textWrapped(Body, INK_MUTED, work.empty() ? "  No layers in this section." : "  No layer matches the filter.", rw);
    ImGui::PopStyleVar();
    ImGui::EndChild();

    // toolbar
    float ty = b.y - pad - ROW_H * S, tx = a.x + pad, gap = SP2 * S;
    bool selWork = laySel >= 0 && laySel < (int)work.size();
    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    if (iconButton("add", Icon::Plus, true, "Add a layer from a template")) ImGui::OpenPopup("##templates");
    ImGui::SetNextWindowPos(ImVec2(tx, ty), ImGuiCond_Always, ImVec2(0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1 * S, 1 * S));
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(LINE_STRONG));
    if (ImGui::BeginPopup("##templates")) {
        ImDrawList* pl = ImGui::GetWindowDrawList();
        float pw = 300 * S;
        for (int t = 0; t < (int)(sizeof TEMPLATES / sizeof TEMPLATES[0]); t++) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(t);
            if (ImGui::InvisibleButton("##t", ImVec2(pw, 42 * S))) {
                int at = selWork ? laySel + 1 : (int)work.size();
                fh6::Layer nl = templateLayer(t);
                work.insert(work.begin() + at, nl);
                workText.insert(workText.begin() + at, fh6::writeLayer(nl));
                workMine.insert(workMine.begin() + at, 1);
                laySel = at;
                layScrollTo = at;
                commitWork();
                ImGui::CloseCurrentPopup();
            }
            bool ih = ImGui::IsItemHovered();
            ImGui::PopID();
            if (ih) pl->AddRectFilled(p, ImVec2(p.x + pw, p.y + 42 * S), rgb(BG300));
            drawText(pl, ImVec2(p.x + SP3 * S, p.y + 5 * S), Body, ih ? INK : INK_MUTED, TEMPLATES[t].name);
            drawText(pl, ImVec2(p.x + SP3 * S, p.y + 23 * S), Caption, INK_MUTED, TEMPLATES[t].help);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    tx += ROW_H * S + gap;
    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    if (iconButton("dup", Icon::Copy, selWork, "Duplicate the selected layer")) {
        work.insert(work.begin() + laySel + 1, work[laySel]);
        workText.insert(workText.begin() + laySel + 1, workText[laySel]);
        workMine.insert(workMine.begin() + laySel + 1, workMine[laySel]);
        laySel++;
        layScrollTo = laySel;
        commitWork();
    }
    tx += ROW_H * S + gap;
    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    if (iconButton("up", Icon::Up, selWork && laySel > 0, "Move up")) {
        std::swap(work[laySel], work[laySel - 1]);
        std::swap(workText[laySel], workText[laySel - 1]);
        std::swap(workMine[laySel], workMine[laySel - 1]);
        laySel--;
        layScrollTo = laySel;
        commitWork();
    }
    tx += ROW_H * S + gap;
    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    if (iconButton("down", Icon::Down, selWork && laySel + 1 < (int)work.size(), "Move down")) {
        std::swap(work[laySel], work[laySel + 1]);
        std::swap(workText[laySel], workText[laySel + 1]);
        std::swap(workMine[laySel], workMine[laySel + 1]);
        laySel++;
        layScrollTo = laySel;
        commitWork();
    }
    tx += ROW_H * S + gap;
    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    if (iconButton("del", Icon::Trash, selWork, "Delete the selected layer", true)) {
        work.erase(work.begin() + laySel);
        workText.erase(workText.begin() + laySel);
        workMine.erase(workMine.begin() + laySel);
        if (laySel >= (int)work.size()) laySel = (int)work.size() - 1;
        commitWork();
    }
    // reset section
    bool changedSec = edits.sections.count(sec) > 0;
    float rw2 = buttonWidth("Reset section", Btn::Ghost);
    ImGui::SetCursorScreenPos(ImVec2(b.x - pad - rw2 + SP3 * S, ty - 6 * S));
    if (button("Reset section", Btn::Ghost, false, rw2, changedSec)) {
        edits.sections.erase(sec);
        workFor.clear();
        syncWork();
        editsChanged();
    }
    helpTooltip("Puts this section back to the game's layers.");
}

// Curve editor: the response curve with draggable points, the mirrored half
// for negative inputs, and the live input marker.
static bool curveGraph(fh6::Layer& l, const char* pre, float w, float h, bool editable, bool hasNow, double nowX) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    std::string P = pre;
    std::string mapping = l.str(P + "Mapping", "Linear");
    fh6::CurvePts c = fh6::curvePts(l, P);
    bool hasClamp = l.has("InputClampMin") && l.has("InputClampMax");
    double lo = hasClamp ? l.num("InputClampMin") : 0, hi = hasClamp ? l.num("InputClampMax") : 1;
    for (int i = 0; i < c.n; i++) { lo = std::min(lo, c.x[i]); hi = std::max(hi, c.x[i]); }
    if (c.mirrored && c.n) lo = std::min(lo, -hi);
    if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
    auto f = [&](double x) {
        if (mapping == "ConstantOne") return 1.0;
        if (mapping == "Abs") return std::fabs(x);
        if (c.n == 0) return hasClamp ? std::clamp(x, l.num("InputClampMin"), l.num("InputClampMax")) : x;
        return fh6::curveEval(c, x);
    };
    double ylo = 0, yhi = 0;
    for (int i = 0; i <= 64; i++) { double y = f(lo + (hi - lo) * i / 64); ylo = std::min(ylo, y); yhi = std::max(yhi, y); }
    if (yhi - ylo < 1e-9) { yhi += 1; ylo -= (ylo < 0 ? 1 : 0); }
    double ypad = (yhi - ylo) * 0.12;
    ylo -= ypad;
    yhi += ypad;
    float gx0 = a.x + 2 * S, gx1 = b.x - 2 * S, gy0 = a.y + 6 * S, gy1 = b.y - 18 * S;
    auto X = [&](double x) { return gx0 + (gx1 - gx0) * (float)((x - lo) / (hi - lo)); };
    auto Y = [&](double y) { return gy1 - (gy1 - gy0) * (float)((y - ylo) / (yhi - ylo)); };
    auto IX = [&](float px) { return lo + (hi - lo) * (px - gx0) / (gx1 - gx0); };
    auto IY = [&](float py) { return ylo + (yhi - ylo) * (gy1 - py) / (gy1 - gy0); };
    dl->AddRectFilled(a, b, rgb(BG100));
    for (int i = 1; i < 4; i++) {
        float yy = std::floor(gy0 + (gy1 - gy0) * i / 4.f), xx = std::floor(gx0 + (gx1 - gx0) * i / 4.f);
        dl->AddRectFilled(ImVec2(gx0, yy), ImVec2(gx1, yy + HAIR * S), rgb(LINE));
        dl->AddRectFilled(ImVec2(xx, gy0), ImVec2(xx + HAIR * S, gy1), rgb(LINE));
    }
    if (lo < 0 && hi > 0) dl->AddRectFilled(ImVec2(std::floor(X(0)), gy0), ImVec2(std::floor(X(0)) + HAIR * S, gy1), rgb(LINE_STRONG));
    if (ylo < 0 && yhi > 0) dl->AddRectFilled(ImVec2(gx0, std::floor(Y(0))), ImVec2(gx1, std::floor(Y(0)) + HAIR * S), rgb(LINE_STRONG));
    // the curve; the mirrored half dimmer
    std::vector<ImVec2> pts;
    double split = c.mirrored && c.n ? 0 : -1e300;
    for (int i = 0; i <= 96; i++) {
        double x = lo + (hi - lo) * i / 96;
        pts.push_back(ImVec2(X(x), Y(f(x))));
    }
    for (size_t i = 1; i < pts.size(); i++) {
        double x = lo + (hi - lo) * i / 96;
        bool dimmed = x <= split;
        dl->AddLine(pts[i - 1], pts[i], rgb(ORANGE, dimmed ? 110 : 255), RULE * S);
    }
    // clamp window
    if (hasClamp) {
        float c0 = X(l.num("InputClampMin")), c1 = X(l.num("InputClampMax"));
        if (c0 > gx0 + 1) dl->AddRectFilled(ImVec2(gx0, gy0), ImVec2(c0, gy1), IM_COL32(0, 0, 0, 90));
        if (c1 < gx1 - 1) dl->AddRectFilled(ImVec2(c1, gy0), ImVec2(gx1, gy1), IM_COL32(0, 0, 0, 90));
    }
    // points
    bool changed = false;
    ImGui::PushID(pre);
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID did = ImGui::GetID("drag");
    int drag = st->GetInt(did, -1);
    ImGui::InvisibleButton("##graph", ImVec2(w, h));
    bool gh = ImGui::IsItemHovered(), ga = ImGui::IsItemActive();
    ImVec2 m = ImGui::GetIO().MousePos;
    int near = -1;
    for (int i = 0; i < c.n; i++) {
        ImVec2 q(X(c.x[i]), Y(c.y[i]));
        if (std::fabs(m.x - q.x) < 7 * S && std::fabs(m.y - q.y) < 7 * S) near = i;
    }
    if (editable && gh && ImGui::IsMouseClicked(0) && near >= 0) drag = near;
    if (!ga) drag = -1;
    if (editable && drag >= 0 && drag < c.n) {
        double nx = IX(m.x), ny = IY(m.y);
        double xmin = drag > 0 ? c.x[drag - 1] : -1e300, xmax = drag + 1 < c.n ? c.x[drag + 1] : 1e300;
        if (c.mirrored) xmin = std::max(xmin, 0.0);
        nx = std::clamp(nx, xmin, xmax);
        double r = std::pow(10.0, std::floor(std::log10(std::max(hi - lo, 1e-6))) - 2);
        nx = std::round(nx / r) * r;
        double ry = std::pow(10.0, std::floor(std::log10(std::max(yhi - ylo, 1e-6))) - 2);
        ny = std::round(ny / ry) * ry;
        l.setNum(P + "CurveInput" + std::to_string(drag), nx);
        l.setNum(P + "CurveOutput" + std::to_string(drag), ny);
        changed = true;
    }
    st->SetInt(did, drag);
    if (editable && (near >= 0 || drag >= 0)) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopID();
    for (int i = 0; i < c.n; i++) {
        ImVec2 q(X(c.x[i]), Y(c.y[i]));
        bool hot = i == near || i == drag;
        float r = (hot ? 4.5f : 3.5f) * S;
        dl->AddRectFilled(ImVec2(q.x - r, q.y - r), ImVec2(q.x + r, q.y + r), rgb(hot ? INK : BG100));
        dl->AddRect(ImVec2(q.x - r, q.y - r), ImVec2(q.x + r, q.y + r), rgb(editable ? ORANGE : LINE_STRONG), 0, RULE * S);
    }
    // live marker
    if (hasNow && std::isfinite(nowX)) {
        double xn = std::clamp(nowX, lo, hi);
        float mx = std::floor(X(xn)), my = Y(f(xn));
        dl->AddRectFilled(ImVec2(mx, gy0), ImVec2(mx + HAIR * S, gy1), rgb(COOL, 160));
        dl->AddRectFilled(ImVec2(mx - 3 * S, my - 3 * S), ImVec2(mx + 3 * S, my + 3 * S), rgb(COOL));
    }
    // axis labels
    drawText(dl, ImVec2(gx0, gy1 + 3 * S), Caption, INK_MUTED, num(lo, 2).c_str());
    std::string hs = num(hi, 2);
    drawText(dl, ImVec2(gx1 - textWidth(Caption, hs.c_str()), gy1 + 3 * S), Caption, INK_MUTED, hs.c_str());
    std::string ys = num(yhi - ypad, 2), yl = num(ylo + ypad, 2);
    drawText(dl, ImVec2(gx0 + 4 * S, gy0), Caption, INK_DISABLED, ys.c_str());
    drawText(dl, ImVec2(gx0 + 4 * S, Y(ylo + ypad) - 16 * S), Caption, INK_DISABLED, yl.c_str());
    dl->AddRect(a, b, rgb(LINE), 0, HAIR * S);
    return changed;
}

void App::layerEditor(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    panel(dl, a, b, false);
    float pad = SP4 * S;
    const auto& secs = fh6::sections();
    bool driftHere = secs[laySec].name == std::string(fh6::SLOTS[p.slot ? 1 : 0].xml);
    std::vector<std::string> dq = driftHere ? fh6::quickLayers(p) : std::vector<std::string>();
    int nAll = (int)work.size() + (int)dq.size();
    if (!baseReal || laySel < 0 || laySel >= nAll) {
        ImGui::SetCursorScreenPos(ImVec2(a.x + pad, a.y + pad));
        text(Title, INK, "Effect layers");
        ImGui::SetCursorScreenPos(ImVec2(a.x + pad, ImGui::GetCursorScreenPos().y));
        textWrapped(Body, INK_MUTED,
                    "Each layer is one rule: when an input moves (speed, G-force, tyre slip, bumps, landings), move or turn the camera "
                    "or change its field of view. The input goes through a range, an optional smoothing, a curve and optional noise, "
                    "then a spring. All enabled layers of a camera add up.",
                    w - 2 * pad);
        ImGui::SetCursorScreenPos(ImVec2(a.x + pad, ImGui::GetCursorScreenPos().y + SP2 * S));
        textWrapped(Body, INK_MUTED,
                    "Pick a layer on the left, or add one with +. Chase Near and Chase Far run live in the preview on the Drift setup "
                    "page; the blue marker on the curve shows your current input.",
                    w - 2 * pad);
        return;
    }
    bool isDrift = laySel >= (int)work.size();
    fh6::Layer dummy;
    fh6::Layer& l = isDrift ? (dummy = fh6::parseLayer(dq[laySel - work.size()])) : work[laySel];
    bool editable = !isDrift && l.str("InputType") != "Constant";
    const fh6::LayerProbe* pr = nullptr;
    if (secs[laySec].slot >= 0 && secs[laySec].slot == stackSlot && laySel < stack.size()) pr = &stack.probe(laySel);

    // header
    char hd[64];
    std::snprintf(hd, sizeof hd, "Layer %d of %d", laySel + 1, nAll);
    drawText(dl, ImVec2(a.x + pad, a.y + pad - 2 * S), Label, INK_MUTED, hd);
    dl->PushClipRect(a, ImVec2(b.x - 110 * S, b.y), true);
    drawText(dl, ImVec2(a.x + pad, a.y + pad + 14 * S), Title, INK, layerTitle(l).c_str());
    dl->PopClipRect();
    bool on = l.flag("Enabled");
    ImGui::SetCursorScreenPos(ImVec2(b.x - pad - 32 * S - 32 * S, a.y + pad + 4 * S));
    drawText(dl, ImVec2(b.x - pad - 32 * S - 30 * S, a.y + pad + 12 * S), Key, INK_MUTED, on ? "ON" : "OFF");
    ImGui::SetCursorScreenPos(ImVec2(b.x - pad - 32 * S, a.y + pad + 4 * S));
    if (editable && smallToggle("enabled", &on, "Turn this layer on or off.")) { l.set("Enabled", on ? "1" : "0"); layerChanged(laySel); }
    float top = pad + 46 * S;
    if (isDrift || !editable) {
        ImGui::SetCursorScreenPos(ImVec2(a.x + pad, a.y + top));
        textWrapped(Caption, isDrift ? ORANGE_HOT : DANGER,
                    isDrift ? "Added by the Drift setup page on this chase camera. Change it there; shown here so you can see how it stacks."
                            : "A hand-made mod layer with the mod-only Constant input. The game may ignore it. Shown read-only.",
                    w - 2 * pad);
        top += 36 * S;
    }

    ImGui::SetCursorScreenPos(ImVec2(a.x + 1 * S, a.y + top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad - 1 * S, 0));
    ImGui::BeginChild("##layer", ImVec2(w - 2 * S, h - top - 2 * S), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SP2 * S, SP2 * S));
    if (!editable) ImGui::BeginDisabled();
    float full = ImGui::GetContentRegionAvail().x;
    bool two = full >= 600 * S;
    float colW = two ? (full - SP6 * S) / 2 : full, labW = std::min(132 * S, colW * 0.4f), fw = colW - labW;
    float col0 = ImGui::GetCursorScreenPos().x, startY = ImGui::GetCursorScreenPos().y;
    bool ch = false;
    std::string in = l.str("InputType"), out = l.str("OutputType");
    int ii = fh6::inputIndex(in), oi = fh6::outputIndex(out);
    bool impulse = oi >= 0 ? (fh6::outputs()[oi].flags & fh6::E_IMPULSE) != 0 : out.find("Impulse") != std::string::npos;

    auto numRow = [&](const char* label, const char* attr, int dec, double step, const char* unit, const char* help) {
        formLabel(label, labW, help);
        double v = l.num(attr);
        if (numField(attr, &v, fw, dec, step, unit)) { l.setNum(attr, v); ch = true; }
    };
    auto pairRow = [&](const char* label, const char* a1, const char* a2, int dec, double step, const char* help) {
        formLabel(label, labW, help);
        float hw = (fw - SP2 * S) / 2;
        double v1 = l.num(a1), v2 = l.num(a2);
        if (numField(a1, &v1, hw, dec, step)) { l.setNum(a1, v1); ch = true; }
        ImGui::SameLine(0, SP2 * S);
        if (numField(a2, &v2, hw, dec, step)) { l.setNum(a2, v2); ch = true; }
    };
    auto toggleRow = [&](const char* label, const char* attr, const char* help) {
        formLabel(label, labW, help);
        bool v = l.flag(attr);
        if (smallToggle(attr, &v, help)) { l.set(attr, v ? "1" : "0"); ch = true; }
    };
    auto choiceRow = [&](const char* label, const char* attr, const std::vector<fh6::ChoiceInfo>& v, const char* help) {
        formLabel(label, labW, help);
        std::vector<const char*> labs, helps;
        for (auto& c : v) { labs.push_back(c.label); helps.push_back(c.help); }
        std::string cur = l.str(attr);
        int ci = fh6::choiceIndex(v, cur);
        if (combo(attr, &ci, labs.data(), (int)labs.size(), fw, helps.data(), nullptr, cur.c_str())) return v[ci].name;
        return (const char*)nullptr;
    };
    auto stepFor = [](double lo, double hi) { return std::max(1e-3, std::fabs(hi - lo) / 300); };
    double range = std::max(1e-3, std::fabs(l.num("InputClampMax", 1) - l.num("InputClampMin", 0)));

    // ---- column A: input, output, spring
    colTitle("Input", true, colW);
    {
        formLabel("Input", labW, ii >= 0 ? fh6::inputs()[ii].help : "Not a known input.");
        std::vector<const char*> labs, helps;
        std::unique_ptr<bool[]> dim(new bool[fh6::inputs().size()]);
        for (auto& x : fh6::inputs()) { dim[labs.size()] = (x.flags & (fh6::E_LEGACY | fh6::E_MODONLY)) != 0; labs.push_back(x.label); helps.push_back(x.help); }
        int ci = ii;
        if (combo("input", &ci, labs.data(), (int)labs.size(), fw, helps.data(), dim.get(), in.c_str())) {
            const fh6::InputInfo& x = fh6::inputs()[ci];
            l.set("InputType", x.name);
            if (!(x.flags & fh6::E_NOCLAMP)) { l.setNum("InputClampMin", x.clampMin); l.setNum("InputClampMax", x.clampMax); }
            if (x.flags & fh6::E_NOCLAMP) { l.erase("InputClampMin"); l.erase("InputClampMax"); }
            if (x.flags & fh6::E_COLLISION) l.erase("InputTakeDerivative");
            ch = true;
        }
    }
    if (l.has("InputClampMin")) pairRow("Range", "InputClampMin", "InputClampMax", 3, stepFor(l.num("InputClampMin"), l.num("InputClampMax")),
                                        "The input is held inside this range before the curve. A one-sided range (0 to max) reacts to one direction only.");
    if (l.has("InputTakeDerivative")) toggleRow("Rate of change", "InputTakeDerivative", "Use how fast the input changes instead of its value (for G-force: the jolt).");
    toggleRow("Smooth size", "InputMagAttackDecay", "Use the input's size through a rise and fall envelope. Fast rise and slow fall: events flash and fade.");
    if (l.flag("InputMagAttackDecay")) pairRow("Rise, fall", "InputAttack", "InputDecay", 2, 1, "How fast the envelope rises and falls (likely per second).");
    if (const char* v = choiceRow("Where", "LocationType", fh6::locations(), "Map region where the layer runs.")) { l.set("LocationType", v); ch = true; }

    colTitle("Output", false, colW);
    {
        formLabel("Output", labW, oi >= 0 ? fh6::outputs()[oi].help : "Not a known output.");
        std::vector<const char*> labs, helps;
        std::unique_ptr<bool[]> dim(new bool[fh6::outputs().size()]);
        for (auto& x : fh6::outputs()) { dim[labs.size()] = (x.flags & fh6::E_LEGACY) != 0; labs.push_back(x.label); helps.push_back(x.help); }
        int ci = oi;
        if (combo("output", &ci, labs.data(), (int)labs.size(), fw, helps.data(), dim.get(), out.c_str())) {
            l.set("OutputType", fh6::outputs()[ci].name);
            ch = true;
        }
    }
    {
        const fh6::OutputInfo* oinf = oi >= 0 ? &fh6::outputs()[oi] : nullptr;
        for (int c = 0; c < 4; c++) {
            std::string attr = std::string("Param.") + "xyzw"[c];
            const char* cl = oinf ? oinf->comp[c] : nullptr;
            double v = l.num(attr);
            if ((!cl || c == 3) && v == 0 && oinf) continue; // unused component
            std::string lab = cl ? cl : attr;
            double sv = std::max(0.001, std::fabs(v) / 100);
            numRow(lab.c_str(), attr.c_str(), std::fabs(v) < 0.1 && v != 0 ? 4 : 3, sv, oinf ? oinf->unit : "",
                   "Size of the effect at a curve value of 1. Negative turns it the other way.");
        }
    }
    colTitle(impulse ? "Kick spring" : "Spring", false, colW);
    if (!impulse) toggleRow("Spring", "Sprung", "Smooth the output through a spring instead of jumping.");
    if (impulse || l.flag("Sprung")) {
        numRow("Stiffness", "SpringK", 1, 1, "", "Higher: faster. About (2 pi Hz) squared.");
        numRow("Damping", "SpringDPercent", 2, 0.01, "", "1: settles without overshoot. Below 1 bounces.");
        double k = l.num("SpringK"), d = l.num("SpringDPercent");
        textWrapped(Caption, INK_MUTED, springText(k, d).c_str(), colW);
    }
    if (impulse) {
        numRow("Min input", "ImpulseInputSizeCutoff", 3, 0.01, "", "Likely the smallest input that fires a kick. Always 0 in the game's file.");
        numRow("Delay", "ImpulseDelay", 3, 0.01, " s", "Likely a delay before the kick. Always 0 in the game's file.");
    }
    float endA = ImGui::GetCursorScreenPos().y;

    // ---- column B: curve and noise
    if (two) { ImGui::SetCursorScreenPos(ImVec2(col0 + colW + SP6 * S, startY)); }
    float colB = ImGui::GetCursorScreenPos().x;
    auto at = [&]() { if (two) ImGui::SetCursorScreenPos(ImVec2(colB, ImGui::GetCursorScreenPos().y)); };
    ImGui::BeginGroup();
    colTitle("Response curve", two, colW);
    at();
    if (const char* v = choiceRow("Shape", "InputToParamMapping", fh6::mappings(), "How the input becomes the curve value.")) {
        fh6::setMapping(l, "InputToParam", v);
        ch = true;
    }
    fh6::CurvePts cp = fh6::curvePts(l, "InputToParam");
    if (cp.n) { at(); toggleRow("Mirrored", "InputToParamCurveMirrored", "Draw the curve for positive inputs; negative inputs give the same shape with the sign flipped."); }
    at();
    bool live = pr && pr->active;
    if (curveGraph(l, "InputToParam", colW, 150 * S, editable && cp.n > 0, live, live ? pr->x : 0)) ch = true;
    at();
    {
        std::string ro;
        if (pr && pr->active) {
            const fh6::OutputInfo* oinf = oi >= 0 ? &fh6::outputs()[oi] : nullptr;
            int big = 0;
            for (int c = 1; c < 3; c++) if (std::fabs(pr->out[c]) > std::fabs(pr->out[big])) big = c;
            ro = "Now: input " + num(pr->input, 2) + "  \xE2\x80\xBA  curve " + num(pr->factor, 2) + "  \xE2\x80\xBA  " +
                 (oinf && oinf->comp[big] ? oinf->comp[big] : "out") + " " + num(pr->out[big], 3) + (oinf ? oinf->unit : "");
            if (!pr->simulated) ro += "  (input not in the preview)";
        } else if (secs[laySec].slot < 0) ro = "Only chase camera layers run in the preview.";
        else if (!on) ro = "Off: not running.";
        else if (impulse) ro = "Kicks need crashes or landings: not in the preview.";
        else ro = secs[laySec].slot != stackSlot ? "Switch the preview to this chase camera on the Drift setup page." : "Not running in the preview.";
        textWrapped(Caption, live ? COOL : INK_MUTED, ro.c_str(), colW);
    }
    for (int i = 0; i < cp.n; i++) {
        at();
        char lb[24];
        std::snprintf(lb, sizeof lb, "Point %d", i + 1);
        std::string ax = "InputToParamCurveInput" + std::to_string(i), ay = "InputToParamCurveOutput" + std::to_string(i);
        pairRow(lb, ax.c_str(), ay.c_str(), 3, stepFor(cp.x[0], cp.x[cp.n - 1]), "Input, then curve value at that input.");
    }
    at();
    colTitle("Noise", false, colW);
    at();
    if (const char* v = choiceRow("Noise", "NoiseType", fh6::noiseTypes(), "Multiplies the output by a moving signal: shake, sway, rumble.")) {
        l.set("NoiseType", v);
        ch = true;
    }
    if (l.str("NoiseType", "None") != "None" && l.has("NoiseFreq")) {
        at(); numRow("Frequency", "NoiseFreq", 2, 0.05, " Hz", "How fast the noise moves.");
        at(); numRow("Seed", "NoiseSeed", 2, 0.1, "", "Offset into the noise so layers don't move in step.");
        at(); pairRow("Out range", "NoiseOutputRangeMin", "NoiseOutputRangeMax", 3, 0.01, "The noise is mapped into this range. 0 to 1 pulses one way.");
        at();
        if (const char* v = choiceRow("Speed from", "InputToNoiseFreqMapping", fh6::noiseMappings(), "What scales the noise frequency.")) {
            fh6::setMapping(l, "InputToNoiseFreq", v);
            ch = true;
        }
        fh6::CurvePts nc = fh6::curvePts(l, "InputToNoiseFreq");
        if (nc.n) {
            at(); toggleRow("Mirrored", "InputToNoiseFreqCurveMirrored", "Same as the response curve's Mirrored.");
            for (int i = 0; i < nc.n; i++) {
                at();
                char lb[24];
                std::snprintf(lb, sizeof lb, "Speed pt %d", i + 1);
                std::string ax = "InputToNoiseFreqCurveInput" + std::to_string(i), ay = "InputToNoiseFreqCurveOutput" + std::to_string(i);
                pairRow(lb, ax.c_str(), ay.c_str(), 3, stepFor(nc.x[0], nc.x[nc.n - 1]), "Input, then frequency multiplier.");
            }
        }
    }
    // attributes the editor doesn't manage (FH4 rebound springs, mod extras)
    {
        fh6::Layer probe = l;
        fh6::normalizeLayer(probe);
        static const char* managedPrefixes[] = { "Enabled", "LocationType", "InputType", "NoiseType", "OutputType", "InputTakeDerivative",
                                                 "InputClamp", "InputMagAttackDecay", "InputAttack", "InputDecay", "InputToNoiseFreq",
                                                 "NoiseFreq", "NoiseSeed", "NoiseOutputRange", "InputToParam", "Param.", "Sprung",
                                                 "SpringK", "SpringDPercent", "ImpulseInputSizeCutoff", "ImpulseDelay" };
        std::vector<std::string> extra;
        for (auto& x : l.a) {
            bool m = false;
            for (const char* pfx : managedPrefixes) if (x.name.rfind(pfx, 0) == 0) { m = true; break; }
            if (!m) extra.push_back(x.name);
        }
        if (!extra.empty()) {
            at();
            colTitle("Other attributes", false, colW);
            for (auto& n : extra) { at(); numRow(n.c_str(), n.c_str(), 3, 0.01, "", "Not part of FH6's usual layer format. Kept as it is."); }
        }
    }
    ImGui::EndGroup();
    if (two) {
        float endB = ImGui::GetCursorScreenPos().y;
        ImGui::SetCursorScreenPos(ImVec2(col0, std::max(endA, endB)));
    }
    ImGui::Dummy(ImVec2(0, SP2 * S));
    if (!editable) ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    (void)range;
    if (ch && editable) layerChanged(laySel);
}
