#include "theme.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gen/jetbrainsmono_medium.inc"
#include "gen/jetbrainsmono_regular.inc"
#include "gen/oxanium_medium.inc"
#include "gen/oxanium_semibold.inc"
#include "gen/saira_medium.inc"
#include "gen/saira_regular.inc"

namespace ui {

float S = 1;
static ImFont *oxM, *oxSB, *sans, *sansM, *mono, *monoM;

void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig c;
    c.OversampleH = 2;
    auto add = [&](const unsigned char* d, unsigned n) { return io.Fonts->AddFontFromMemoryCompressedTTF(d, (int)n, 14, &c); };
    oxM = add(oxanium_medium_compressed_data, oxanium_medium_compressed_size);
    oxSB = add(oxanium_semibold_compressed_data, oxanium_semibold_compressed_size);
    sans = add(saira_regular_compressed_data, saira_regular_compressed_size);
    sansM = add(saira_medium_compressed_data, saira_medium_compressed_size);
    mono = add(jetbrainsmono_regular_compressed_data, jetbrainsmono_regular_compressed_size);
    monoM = add(jetbrainsmono_medium_compressed_data, jetbrainsmono_medium_compressed_size);
    io.FontDefault = sans;
}

// Sizes, weights and tracking from tokens.json; Saira 600 isn't shipped, so
// body-strong uses Saira Medium.
TypeStyle typeStyle(Style s) {
    switch (s) {
    case Headline: return { oxSB, 28, 0.02f, true };
    case Title: return { oxSB, 18, 0.04f, true };
    case Nav: return { oxM, 13, 0.14f, true };
    case Label: return { oxM, 11, 0.16f, true };
    case Body: return { sans, 14, 0, false };
    case BodyStrong: return { sansM, 14, 0, false };
    case Caption: return { sansM, 12, 0, false };
    case ReadoutXL: return { monoM, 56, -0.03f, false };
    case Readout: return { monoM, 20, 0, false };
    case Stat: return { mono, 13, 0, false };
    case BtnText: return { oxSB, 13, 0.12f, true };
    case BtnTextLg: return { oxSB, 16, 0.12f, true };
    case Sub: return { sans, 11, 0.02f, false };
    case Key: return { monoM, 10, 0, false };
    case Lockup: return { oxSB, 18, 0.06f, true };
    }
    return { sans, 14, 0, false };
}

static ImVec4 v4(unsigned hex, float a = 1) {
    return ImVec4(((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f, a);
}

void applyTheme(float scale) {
    S = scale;
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(12, 12);
    s.ItemSpacing = ImVec2(SP3, SP4);
    s.ItemInnerSpacing = ImVec2(SP2, SP2);
    s.ScrollbarSize = 6;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.FrameBorderSize = 1;
    s.PopupBorderSize = 1;
    s.WindowRounding = s.ChildRounding = s.FrameRounding = s.PopupRounding = 0;
    s.GrabRounding = s.ScrollbarRounding = s.TabRounding = 0;
    s.FontSizeBase = 14;
    s.FontScaleDpi = scale;
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = v4(INK);
    c[ImGuiCol_TextDisabled] = v4(INK_DISABLED);
    c[ImGuiCol_WindowBg] = v4(BG100);
    c[ImGuiCol_ChildBg] = v4(BG100, 0);
    c[ImGuiCol_PopupBg] = v4(BG200);
    c[ImGuiCol_Border] = v4(LINE);
    c[ImGuiCol_FrameBg] = v4(BG100);
    c[ImGuiCol_FrameBgHovered] = v4(BG300);
    c[ImGuiCol_FrameBgActive] = v4(BG300);
    c[ImGuiCol_Button] = v4(BG300);
    c[ImGuiCol_ButtonHovered] = v4(BG300);
    c[ImGuiCol_ButtonActive] = v4(BG300);
    c[ImGuiCol_CheckMark] = v4(ORANGE);
    c[ImGuiCol_ScrollbarBg] = v4(BG100, 0);
    c[ImGuiCol_ScrollbarGrab] = v4(LINE);
    c[ImGuiCol_ScrollbarGrabHovered] = v4(LINE_STRONG);
    c[ImGuiCol_ScrollbarGrabActive] = v4(LINE_STRONG);
    c[ImGuiCol_Separator] = v4(LINE);
    c[ImGuiCol_TextSelectedBg] = v4(ORANGE, 0.35f);
    c[ImGuiCol_NavCursor] = v4(FOCUS_RING);
    s.ScaleAllSizes(scale);
}

// ---------------------------------------------------------------------------
// Text
static std::string cased(Style s, const char* t) {
    std::string r = t;
    if (typeStyle(s).upper) for (auto& ch : r) ch = (char)std::toupper((unsigned char)ch);
    return r;
}

// Walks UTF-8 codepoints so tracking applies per character.
template <class F> static void eachChar(const std::string& s, F fn) {
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end) {
        unsigned int c;
        int n = ImTextCharFromUtf8(&c, p, end);
        if (n <= 0) n = 1;
        fn(p, p + n);
        p += n;
    }
}

float textWidth(Style s, const char* t) {
    TypeStyle st = typeStyle(s);
    float px = st.size * S, tr = st.track * px, w = 0;
    std::string str = cased(s, t);
    if (st.track == 0) return st.font->CalcTextSizeA(px, 1e9f, 0, str.c_str()).x;
    int n = 0;
    eachChar(str, [&](const char* a, const char* b) { w += st.font->CalcTextSizeA(px, 1e9f, 0, a, b).x + tr; n++; });
    return n ? w - tr : 0;
}

float textHeight(Style s) { return typeStyle(s).size * S; }

void drawText(ImDrawList* dl, ImVec2 pos, Style s, unsigned color, const char* t, int alpha) {
    TypeStyle st = typeStyle(s);
    float px = st.size * S, tr = st.track * px;
    std::string str = cased(s, t);
    pos.x = std::floor(pos.x);
    pos.y = std::floor(pos.y);
    if (st.track == 0) { dl->AddText(st.font, px, pos, rgb(color, alpha), str.c_str()); return; }
    eachChar(str, [&](const char* a, const char* b) {
        dl->AddText(st.font, px, pos, rgb(color, alpha), a, b);
        pos.x += st.font->CalcTextSizeA(px, 1e9f, 0, a, b).x + tr;
    });
}

void text(Style s, unsigned color, const char* t) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    drawText(ImGui::GetWindowDrawList(), p, s, color, t);
    ImGui::Dummy(ImVec2(textWidth(s, t), textHeight(s) * 1.25f));
}

void textWrapped(Style s, unsigned color, const char* t, float w) {
    TypeStyle st = typeStyle(s);
    ImGui::PushFont(st.font, st.size);
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(color));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextUnformatted(t);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void fmtNum(char* out, size_t n, double v, int decimals, bool signedDelta) {
    double r = std::pow(10.0, decimals);
    v = std::round(v * r) / r;
    if (v == 0) v = 0; // no "-0"
    char b[48];
    std::snprintf(b, sizeof b, "%.*f", decimals, std::fabs(v));
    if (!signedDelta || v == 0) std::snprintf(out, n, "%s%s", v < 0 ? "\xE2\x88\x92" : "", b);
    else std::snprintf(out, n, "%s %s", v > 0 ? "+" : "\xE2\x88\x92", b);
}

// ---------------------------------------------------------------------------
// Shapes
void cutFill(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 col) {
    cut = std::min(cut, std::min(b.x - a.x, b.y - a.y));
    ImVec2 p[5] = { a, ImVec2(b.x - cut, a.y), ImVec2(b.x, a.y + cut), b, ImVec2(a.x, b.y) };
    dl->AddConvexPolyFilled(p, 5, col);
}

void cutLine(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 col, float th) {
    float h = th * 0.5f; // stroke inside the shape, like an inset box-shadow
    a.x += h; a.y += h; b.x -= h; b.y -= h;
    cut = std::min(cut, std::min(b.x - a.x, b.y - a.y));
    ImVec2 p[5] = { a, ImVec2(b.x - cut, a.y), ImVec2(b.x, a.y + cut), b, ImVec2(a.x, b.y) };
    dl->AddPolyline(p, 5, col, th, ImDrawFlags_Closed);
}

void panel(ImDrawList* dl, ImVec2 a, ImVec2 b, bool brackets, int glassPct) {
    cutFill(dl, a, b, CH_LG * S, glassPct ? glass(glassPct) : rgb(BG200));
    cutLine(dl, a, b, CH_LG * S, glassPct ? GLASS_LINE : rgb(LINE), HAIR * S);
    if (brackets) {
        float t = SP3 * S, w = RULE * S;
        dl->AddRectFilled(a, ImVec2(a.x + t, a.y + w), rgb(ORANGE));
        dl->AddRectFilled(a, ImVec2(a.x + w, a.y + t), rgb(ORANGE));
        dl->AddRectFilled(ImVec2(b.x - t, b.y - w), b, rgb(ORANGE));
        dl->AddRectFilled(ImVec2(b.x - w, b.y - t), b, rgb(ORANGE));
    }
}

void helpTooltip(const char* help) {
    if (!help || !*help || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SP3 * S, SP2 * S));
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(LINE));
    ImGui::BeginTooltip();
    textWrapped(Body, INK, help, 280 * S);
    ImGui::EndTooltip();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// Hover animation, 0..1, kept per item
static float anim(ImGuiID id, bool on, float seconds) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float t = st->GetFloat(id, 0);
    float dt = ImGui::GetIO().DeltaTime / seconds;
    t = on ? std::min(1.f, t + dt) : std::max(0.f, t - dt);
    st->SetFloat(id, t);
    float e = 1 - t;
    return 1 - e * e * e; // ease out
}

// ---------------------------------------------------------------------------
// Button: key hint, label (+ sub-line on large), arrow; fill sweeps in from the left
static void arrowIcon(ImDrawList* dl, ImVec2 p, float sz, ImU32 col) {
    float k = sz / 14.f, th = 1.6f * k;
    dl->AddLine(ImVec2(p.x + 1.5f * k, p.y + 7 * k), ImVec2(p.x + 11.5f * k, p.y + 7 * k), col, th);
    ImVec2 c[3] = { ImVec2(p.x + 7.5f * k, p.y + 3 * k), ImVec2(p.x + 11.5f * k, p.y + 7 * k), ImVec2(p.x + 7.5f * k, p.y + 11 * k) };
    dl->AddPolyline(c, 3, col, th, 0);
}

float buttonWidth(const char* label, Btn v, bool large, const char* hint, const char* sub) {
    Style ls = large ? BtnTextLg : BtnText;
    float padL = (large ? SP8 : (v == Btn::Ghost ? SP3 : SP6)) * S, padR = (v == Btn::Ghost ? SP3 : SP6) * S, gap = (large ? SP4 : SP3) * S;
    float hintW = hint ? textWidth(Key, hint) + 10 * S : 0;
    float lw = std::max(textWidth(ls, label), sub ? textWidth(Sub, sub) : 0.f);
    float w = padL + (hint ? hintW + gap : 0) + lw + (v == Btn::Primary ? gap + (large ? 18 : 14) * S : 0) + padR;
    return large ? std::max(w, 240 * S) : w;
}

bool button(const char* label, Btn v, bool large, float width, bool enabled, const char* hint, const char* sub) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Style ls = large ? BtnTextLg : BtnText;
    float h = (large ? 56 : 40) * S, padL = (large ? SP8 : (v == Btn::Ghost ? SP3 : SP6)) * S,
          padR = (v == Btn::Ghost ? SP3 : SP6) * S, gap = (large ? SP4 : SP3) * S;
    bool arrow = v == Btn::Primary;
    float hintW = hint ? textWidth(Key, hint) + 10 * S : 0;
    float w = width > 0 ? width : buttonWidth(label, v, large, hint, sub);
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##b", ImVec2(w, h)) && enabled;
    bool hov = enabled && ImGui::IsItemHovered(), act = enabled && ImGui::IsItemActive();
    float sweep = anim(ImGui::GetID("sweep"), hov, 0.24f);
    ImGui::PopID();
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float drop = act ? 1 * S : 0;
    ImVec2 p0(a.x, a.y + drop), p1(a.x + w, a.y + h + drop);
    unsigned fg;
    if (!enabled) {
        if (v != Btn::Ghost) dl->AddRectFilled(p0, p1, rgb(BG300));
        fg = INK_DISABLED;
    } else if (v == Btn::Primary) {
        if (sweep > 0) // glow-orange: 0 0 20px rgba(255,106,19,.28)
            for (int i = 4; i >= 1; i--) {
                float g = i * 5 * S;
                dl->AddRectFilled(ImVec2(p0.x - g, p0.y - g), ImVec2(p1.x + g, p1.y + g), IM_COL32(255, 106, 19, (int)(16 * sweep / i)));
            }
        cutFill(dl, p0, p1, CH_MD * S, rgb(ORANGE));
        if (sweep > 0) {
            dl->PushClipRect(p0, ImVec2(p0.x + w * sweep, p1.y), true);
            cutFill(dl, p0, p1, CH_MD * S, rgb(ORANGE_PRESS));
            dl->PopClipRect();
        }
        fg = ON_ORANGE;
    } else if (v == Btn::Secondary || v == Btn::Danger) {
        unsigned edge = v == Btn::Danger ? DANGER : (hov ? INK : LINE_STRONG);
        if (sweep > 0) dl->AddRectFilled(p0, ImVec2(p0.x + w * sweep, p1.y), rgb(v == Btn::Danger ? DANGER : BG300));
        dl->AddRect(p0, p1, rgb(edge), 0, HAIR * S);
        fg = v == Btn::Danger ? (hov ? ON_ORANGE : DANGER) : INK;
    } else {
        fg = hov ? INK : INK_MUTED;
    }
    // keyboard focus
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible) {
        if (v == Btn::Primary && enabled) cutLine(dl, ImVec2(p0.x + 2 * S, p0.y + 2 * S), ImVec2(p1.x - 2 * S, p1.y - 2 * S), CH_MD * S, rgb(ON_ORANGE), RULE * S);
        else dl->AddRect(ImVec2(p0.x - 3 * S, p0.y - 3 * S), ImVec2(p1.x + 3 * S, p1.y + 3 * S), rgb(FOCUS_RING), 0, RULE * S);
    }
    float x = p0.x + padL, cy = (p0.y + p1.y) * 0.5f;
    if (hint) {
        float kh = textHeight(Key) + 4 * S;
        ImVec2 ka(x, cy - kh / 2), kb(x + hintW, cy + kh / 2);
        dl->AddRect(ka, kb, rgb(fg), 2 * S, HAIR * S);
        drawText(dl, ImVec2(x + 5 * S, ka.y + 2 * S), Key, fg, hint);
        x += hintW + gap;
    }
    if (sub) {
        float lh = textHeight(ls), sh = textHeight(Sub);
        float top = cy - (lh + 2 * S + sh) / 2;
        drawText(dl, ImVec2(x, top), ls, fg, label);
        drawText(dl, ImVec2(x, top + lh + 2 * S), Sub, fg, sub);
    } else {
        drawText(dl, ImVec2(x, cy - textHeight(ls) / 2 - 1 * S), ls, fg, label);
    }
    if (arrow) {
        float az = (large ? 18 : 14) * S;
        arrowIcon(dl, ImVec2(p1.x - padR - az + 3 * S * sweep, cy - az / 2), az, rgb(fg));
    }
    return clicked;
}

// ---------------------------------------------------------------------------
// Slider: label + mono value, 2px track with orange fill, 4x16 bar thumb,
// line-strong center line. Right-click resets, double-click types a value.
bool slider(const char* id, const char* label, double* v, double mn, double mx, double step,
            const char* unit, int dec, bool signedDelta, double def, const char* help, bool clampTyped, const double* tick) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float labelH = 14 * S, inputTop = p.y + labelH + SP1 * S, inputH = 16 * S;
    float rowH = labelH + (SP1 + 16) * S;
    bool changed = false;
    ImGui::InvisibleButton("##hit", ImVec2(w, rowH));
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    helpTooltip(help);
    float x0 = p.x + 2 * S, x1 = p.x + w - 2 * S;
    if (act && !ImGui::IsMouseDoubleClicked(0)) {
        double t = std::clamp((double)(ImGui::GetIO().MousePos.x - x0) / (x1 - x0), 0.0, 1.0);
        double nv = std::round((mn + t * (mx - mn)) / step) * step;
        if (std::fabs(nv - *v) > 1e-12) { *v = nv; changed = true; }
    }
    if (ImGui::IsItemFocused()) { // keyboard: arrow keys step the value
        double d = (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? step : 0) - (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? step : 0);
        if (d != 0) { *v = std::clamp(*v + d, mn, mx); changed = true; }
    }
    if (hov && ImGui::IsMouseClicked(1)) { *v = def; changed = true; }
    if (hov && ImGui::IsMouseDoubleClicked(0)) ImGui::OpenPopup("##edit");
    if (hov || act) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    drawText(dl, p, Label, hov || act ? INK : INK_MUTED, label);
    char num[48], unitBuf[24];
    fmtNum(num, sizeof num, *v, dec, signedDelta);
    std::snprintf(unitBuf, sizeof unitBuf, "%s", unit ? unit : "");
    float uw = unitBuf[0] ? textWidth(Stat, unitBuf) : 0, nw = textWidth(Stat, num);
    float vy = p.y - 1 * S;
    if (unitBuf[0]) drawText(dl, ImVec2(p.x + w - uw, vy), Stat, INK_MUTED, unitBuf);
    drawText(dl, ImVec2(p.x + w - uw - nw, vy), Stat, INK, num);

    float cy = inputTop + inputH / 2, t = (float)((*v - mn) / (mx - mn));
    float tx = x0 + (x1 - x0) * t, mid = (x0 + x1) / 2;
    if (tick) mid = x0 + (x1 - x0) * (float)std::clamp((*tick - mn) / (mx - mn), 0.0, 1.0);
    dl->AddRectFilled(ImVec2(x0, cy - RULE * S / 2), ImVec2(x1, cy + RULE * S / 2), rgb(BG300));
    dl->AddRectFilled(ImVec2(x0, cy - RULE * S / 2), ImVec2(tx, cy + RULE * S / 2), rgb(ORANGE));
    dl->AddRectFilled(ImVec2(mid - HAIR * S / 2, inputTop), ImVec2(mid + HAIR * S / 2, inputTop + inputH), rgb(LINE_STRONG));
    dl->AddRectFilled(ImVec2(tx - 2 * S, inputTop), ImVec2(tx + 2 * S, inputTop + inputH), rgb(ORANGE));
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        dl->AddRect(ImVec2(p.x - 4 * S, p.y - 4 * S), ImVec2(p.x + w + 4 * S, p.y + rowH + 2 * S), rgb(FOCUS_RING), 0, RULE * S);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SP3 * S, SP3 * S));
    if (ImGui::BeginPopup("##edit")) {
        static double tmp;
        if (ImGui::IsWindowAppearing()) { tmp = *v; ImGui::SetKeyboardFocusHere(); }
        ImGui::PushFont(typeStyle(Stat).font, 13);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SP3 * S, SP2 * S));
        ImGui::PushStyleColor(ImGuiCol_Border, rgb(LINE_STRONG));
        ImGui::SetNextItemWidth(120 * S);
        char f[16];
        std::snprintf(f, sizeof f, "%%.%df", dec);
        if (ImGui::InputDouble("##v", &tmp, 0, 0, f, ImGuiInputTextFlags_EnterReturnsTrue)) {
            *v = clampTyped ? std::clamp(tmp, mn, mx) : tmp;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopFont();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

// Toggle: square track, ON/OFF in mono, then the label
bool toggle(const char* id, const char* label, bool* v, const char* help) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x, h = 20 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool changed = false;
    if (ImGui::InvisibleButton("##t", ImVec2(w, h))) { *v = !*v; changed = true; }
    bool hov = ImGui::IsItemHovered();
    helpTooltip(help);
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float k = anim(ImGui::GetID("k"), *v, 0.12f);
    ImGui::PopID();
    ImVec2 ta(p.x, p.y + (h - 16 * S) / 2), tb(ta.x + 32 * S, ta.y + 16 * S);
    if (*v) dl->AddRectFilled(ta, tb, rgb(ORANGE));
    else dl->AddRect(ta, tb, rgb(hov ? INK : LINE_STRONG), 0, HAIR * S);
    float tx = ta.x + (4 + 16 * k) * S;
    dl->AddRectFilled(ImVec2(tx, ta.y + 4 * S), ImVec2(tx + 8 * S, ta.y + 12 * S), rgb(*v ? ON_ORANGE : INK_MUTED), 2 * S);
    float sx = tb.x + SP3 * S;
    drawText(dl, ImVec2(sx, p.y + (h - 11 * S) / 2 - 1 * S), Key, INK_MUTED, *v ? "ON" : "OFF");
    TypeStyle ks = typeStyle(Key);
    (void)ks;
    drawText(dl, ImVec2(sx + 24 * S + SP3 * S, p.y + (h - 14 * S) / 2 - 2 * S), Body, INK, label);
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        dl->AddRect(ImVec2(ta.x - 3 * S, ta.y - 3 * S), ImVec2(tb.x + 3 * S, tb.y + 3 * S), rgb(FOCUS_RING), 0, RULE * S);
    return changed;
}

// SegmentTabs: plain labels on a hairline, the selected one underlined in orange
float segmentTabsWidth(const char* const* items, int n) {
    float w = 0;
    for (int i = 0; i < n; i++) w += textWidth(Nav, items[i]) + (i ? SP6 * S : 0);
    return w;
}

bool segmentTabs(const char* id, int* v, const char* const* items, int n) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = (SP2 + 13 + SP3) * S, x = p.x;
    float total = segmentTabsWidth(items, n);
    dl->AddRectFilled(ImVec2(p.x, p.y + h - HAIR * S), ImVec2(p.x + total, p.y + h), rgb(LINE));
    bool changed = false;
    for (int i = 0; i < n; i++) {
        float tw = textWidth(Nav, items[i]);
        ImGui::SetCursorScreenPos(ImVec2(x, p.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##s", ImVec2(tw, h)) && *v != i) { *v = i; changed = true; }
        bool hov = ImGui::IsItemHovered();
        if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
            dl->AddRect(ImVec2(x - 3 * S, p.y - 1 * S), ImVec2(x + tw + 3 * S, p.y + h + 2 * S), rgb(FOCUS_RING), 0, RULE * S);
        ImGui::PopID();
        drawText(dl, ImVec2(x, p.y + SP2 * S), Nav, *v == i || hov ? INK : INK_MUTED, items[i]);
        if (*v == i) dl->AddRectFilled(ImVec2(x, p.y + h - RULE * S), ImVec2(x + tw, p.y + h), rgb(ORANGE));
        x += tw + SP6 * S;
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(total, 0));
    ImGui::PopID();
    return changed;
}

// Tag: hairline outline in the tone, one cut corner
float tagWidth(const char* t) { return textWidth(Label, t) + 2 * SP2 * S; }

void tag(ImDrawList* dl, ImVec2 pos, Tone tone, const char* t) {
    unsigned c = tone == Tone::Orange ? ORANGE : tone == Tone::Cool ? COOL : tone == Tone::Danger ? DANGER : LINE_STRONG;
    unsigned tc = tone == Tone::Neutral ? INK_MUTED : c;
    ImVec2 b(pos.x + tagWidth(t), pos.y + (14 + 6) * S);
    cutLine(dl, pos, b, CH_SM * S, rgb(c), HAIR * S);
    drawText(dl, ImVec2(pos.x + SP2 * S, pos.y + 3 * S), Label, tc, t);
}

void groupTitle(const char* t, bool first) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!first) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + HAIR * S), rgb(LINE));
        ImGui::Dummy(ImVec2(0, SP2 * S));
    }
    text(Nav, INK, t);
}

// ---------------------------------------------------------------------------
// Compact form controls
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float sz, ImU32 col) {
    float k = sz / 14.f, th = 1.5f * k;
    auto P = [&](float x, float y) { return ImVec2(c.x + (x - 7) * k, c.y + (y - 7) * k); };
    switch (icon) {
    case Icon::Plus:
        dl->AddLine(P(7, 2), P(7, 12), col, th);
        dl->AddLine(P(2, 7), P(12, 7), col, th);
        break;
    case Icon::Copy:
        dl->AddRect(P(4.5f, 4.5f), P(12, 12), col, 0, th);
        dl->AddPolyline(std::vector<ImVec2>{ P(9.5f, 2), P(2, 2), P(2, 9.5f) }.data(), 3, col, th, 0);
        break;
    case Icon::Up: {
        ImVec2 p[3] = { P(3, 9), P(7, 5), P(11, 9) };
        dl->AddPolyline(p, 3, col, th, 0);
        break;
    }
    case Icon::Down:
    case Icon::Chevron: {
        ImVec2 p[3] = { P(3, 5), P(7, 9), P(11, 5) };
        dl->AddPolyline(p, 3, col, th, 0);
        break;
    }
    case Icon::Trash:
        dl->AddLine(P(2, 3.5f), P(12, 3.5f), col, th);
        dl->AddLine(P(5.5f, 1.5f), P(8.5f, 1.5f), col, th);
        dl->AddRect(P(3.5f, 3.5f), P(10.5f, 12.5f), col, 0, th);
        dl->AddLine(P(6, 6), P(6, 10), col, th);
        dl->AddLine(P(8, 6), P(8, 10), col, th);
        break;
    case Icon::Reset: {
        dl->PathArcTo(c, 4.5f * k, 3.14159f * 0.25f, 3.14159f * 1.75f, 16);
        dl->PathStroke(col, th);
        ImVec2 a = ImVec2(c.x + 4.5f * k * 0.7071f, c.y - 4.5f * k * 0.7071f);
        dl->AddTriangleFilled(ImVec2(a.x + 2.5f * k, a.y - 2.5f * k), ImVec2(a.x + 2.5f * k, a.y + 1.5f * k), ImVec2(a.x - 1.5f * k, a.y - 2.5f * k), col);
        break;
    }
    case Icon::Lock:
        dl->AddRect(P(3, 6.5f), P(11, 12.5f), col, 0, th);
        dl->PathArcTo(P(7, 6.5f), 2.5f * k, 3.14159f, 2 * 3.14159f, 10);
        dl->PathStroke(col, th);
        break;
    case Icon::Search:
        dl->AddCircle(P(6, 6), 3.8f * k, col, 16, th);
        dl->AddLine(P(8.8f, 8.8f), P(12, 12), col, th);
        break;
    }
}

bool iconButton(const char* id, Icon icon, bool enabled, const char* help, bool danger) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float s = ROW_H * S;
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + s, a.y + s);
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##ib", ImVec2(s, s)) && enabled;
    bool hov = enabled && ImGui::IsItemHovered();
    helpTooltip(help);
    ImGui::PopID();
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    unsigned edge = !enabled ? LINE : danger ? (hov ? DANGER : LINE_STRONG) : (hov ? INK : LINE_STRONG);
    if (hov) dl->AddRectFilled(a, b, rgb(BG300));
    dl->AddRect(a, b, rgb(edge), 0, HAIR * S);
    unsigned fg = !enabled ? INK_DISABLED : danger && hov ? DANGER : hov ? INK : INK_MUTED;
    drawIcon(dl, icon, ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2), 14 * S, rgb(fg));
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        dl->AddRect(ImVec2(a.x - 3 * S, a.y - 3 * S), ImVec2(b.x + 3 * S, b.y + 3 * S), rgb(FOCUS_RING), 0, RULE * S);
    return clicked;
}

bool numField(const char* id, double* v, float w, int dec, double step, const char* unit, bool enabled) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = ROW_H * S;
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    ImGui::PushID(id);
    ImGuiID eid = ImGui::GetID("##edit");
    ImGuiStorage* st = ImGui::GetStateStorage();
    bool editing = st->GetBool(eid, false);
    bool changed = false;
    if (editing && enabled) {
        static double tmp;
        if (st->GetBool(ImGui::GetID("##first"), true)) { tmp = *v; ImGui::SetKeyboardFocusHere(); st->SetBool(ImGui::GetID("##first"), false); }
        TypeStyle ts = typeStyle(Stat);
        ImGui::PushFont(ts.font, ts.size);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SP2 * S, (h - ts.size * S) / 2));
        ImGui::PushStyleColor(ImGuiCol_Border, rgb(ORANGE));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, rgb(BG000));
        ImGui::SetNextItemWidth(w);
        char f[16];
        std::snprintf(f, sizeof f, "%%.%df", std::max(dec, 3));
        if (ImGui::InputDouble("##in", &tmp, 0, 0, f, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (std::isfinite(tmp)) { *v = tmp; changed = true; }
            st->SetBool(eid, false);
        } else if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape)) st->SetBool(eid, false);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        ImGui::PopFont();
        ImGui::PopID();
        return changed;
    }
    ImGui::InvisibleButton("##nf", ImVec2(w, h));
    bool hov = enabled && ImGui::IsItemHovered(), act = enabled && ImGui::IsItemActive();
    if (hov || act) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (act && ImGui::IsMouseDragging(0, 2)) {
        ImGuiIO& io = ImGui::GetIO();
        double k = io.KeyShift ? 0.1 : io.KeyCtrl ? 10 : 1;
        double d = io.MouseDelta.x / S * step * k;
        if (d != 0) {
            double r = std::pow(10.0, dec);
            double nv = *v + d;
            // keep sub-step drags accumulating, but show rounded values
            *v = nv;
            (void)r;
            changed = true;
        }
    }
    if (hov && ImGui::IsMouseDoubleClicked(0)) { st->SetBool(eid, true); st->SetBool(ImGui::GetID("##first"), true); }
    ImGui::PopID();
    dl->AddRectFilled(a, b, rgb(enabled ? BG100 : BG200));
    dl->AddRect(a, b, rgb(act ? ORANGE : hov ? INK_MUTED : LINE_STRONG), 0, HAIR * S);
    char num[48];
    fmtNum(num, sizeof num, *v, dec, false);
    float uw = unit && *unit ? textWidth(Stat, unit) + SP1 * S : 0;
    float ty = a.y + (h - 13 * S) / 2 - 1 * S;
    dl->PushClipRect(a, b, true);
    drawText(dl, ImVec2(b.x - SP2 * S - uw - textWidth(Stat, num), ty), Stat, enabled ? INK : INK_DISABLED, num);
    if (uw > 0) drawText(dl, ImVec2(b.x - SP2 * S - uw + SP1 * S, ty), Stat, INK_MUTED, unit);
    dl->PopClipRect();
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        dl->AddRect(ImVec2(a.x - 3 * S, a.y - 3 * S), ImVec2(b.x + 3 * S, b.y + 3 * S), rgb(FOCUS_RING), 0, RULE * S);
    return changed;
}

bool combo(const char* id, int* cur, const char* const* labels, int n, float w, const char* const* helps, const bool* dim, const char* fallback) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = ROW_H * S;
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##cb", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (clicked) ImGui::OpenPopup("##list");
    bool open = ImGui::IsPopupOpen("##list");
    dl->AddRectFilled(a, b, rgb(BG100));
    dl->AddRect(a, b, rgb(open ? ORANGE : hov ? INK_MUTED : LINE_STRONG), 0, HAIR * S);
    const char* shown = *cur >= 0 && *cur < n ? labels[*cur] : (fallback ? fallback : "");
    dl->PushClipRect(a, ImVec2(b.x - 22 * S, b.y), true);
    drawText(dl, ImVec2(a.x + SP2 * S, a.y + (h - 14 * S) / 2 - 2 * S), Body, *cur >= 0 && *cur < n && !(dim && dim[*cur]) ? INK : INK_MUTED, shown);
    dl->PopClipRect();
    drawIcon(dl, Icon::Chevron, ImVec2(b.x - 12 * S, (a.y + b.y) / 2), 12 * S, rgb(INK_MUTED));
    bool changed = false;
    float pw = std::max(w, helps ? 340 * S : w);
    ImGui::SetNextWindowPos(ImVec2(a.x, b.y + 2 * S));
    ImGui::SetNextWindowSizeConstraints(ImVec2(pw, 0), ImVec2(pw, 380 * S));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1 * S, 1 * S));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(LINE_STRONG));
    if (ImGui::BeginPopup("##list")) {
        ImDrawList* pl = ImGui::GetWindowDrawList();
        for (int i = 0; i < n; i++) {
            float rh = (helps && helps[i] && *helps[i] ? 42 : 28) * S;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float iw = ImGui::GetContentRegionAvail().x;
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##it", ImVec2(iw, rh))) { if (*cur != i) { *cur = i; changed = true; } ImGui::CloseCurrentPopup(); }
            bool ih = ImGui::IsItemHovered();
            if (i == *cur && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.3f);
            ImGui::PopID();
            if (ih) pl->AddRectFilled(p, ImVec2(p.x + iw, p.y + rh), rgb(BG300));
            if (i == *cur) pl->AddRectFilled(p, ImVec2(p.x + RULE * S, p.y + rh), rgb(ORANGE));
            bool dm = dim && dim[i];
            drawText(pl, ImVec2(p.x + SP3 * S, p.y + 5 * S), Body, dm ? INK_DISABLED : (i == *cur || ih ? INK : INK_MUTED), labels[i]);
            if (helps && helps[i] && *helps[i]) {
                pl->PushClipRect(p, ImVec2(p.x + iw - SP2 * S, p.y + rh), true);
                drawText(pl, ImVec2(p.x + SP3 * S, p.y + 23 * S), Caption, dm ? INK_DISABLED : INK_MUTED, helps[i]);
                pl->PopClipRect();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return changed;
}

bool smallToggle(const char* id, bool* v, const char* help) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = ROW_H * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool changed = false;
    if (ImGui::InvisibleButton("##st", ImVec2(32 * S, h))) { *v = !*v; changed = true; }
    bool hov = ImGui::IsItemHovered();
    helpTooltip(help);
    float k = anim(ImGui::GetID("k"), *v, 0.12f);
    ImGui::PopID();
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImVec2 ta(p.x, p.y + (h - 16 * S) / 2), tb(ta.x + 32 * S, ta.y + 16 * S);
    if (*v) dl->AddRectFilled(ta, tb, rgb(ORANGE));
    else dl->AddRect(ta, tb, rgb(hov ? INK : LINE_STRONG), 0, HAIR * S);
    float tx = ta.x + (4 + 16 * k) * S;
    dl->AddRectFilled(ImVec2(tx, ta.y + 4 * S), ImVec2(tx + 8 * S, ta.y + 12 * S), rgb(*v ? ON_ORANGE : INK_MUTED), 2 * S);
    return changed;
}

bool searchBox(const char* id, char* buf, size_t n, const char* hint, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = ROW_H * S;
    ImVec2 a = ImGui::GetCursorScreenPos();
    TypeStyle ts = typeStyle(Body);
    ImGui::PushFont(ts.font, ts.size);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(28 * S, (h - ts.size * S) / 2 - 1 * S));
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(LINE_STRONG));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, rgb(INK_DISABLED));
    ImGui::SetNextItemWidth(w);
    bool ch = ImGui::InputTextWithHint(id, hint, buf, n);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    drawIcon(dl, Icon::Search, ImVec2(a.x + 14 * S, a.y + h / 2), 13 * S, rgb(INK_MUTED));
    return ch;
}

void formLabel(const char* label, float labelW, const char* help) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ROW_H * S;
    ImGui::PushID(label);
    ImGui::PushID((int)(p.y / S));
    ImGui::InvisibleButton("##fl", ImVec2(labelW, h));
    ImGui::PopID();
    ImGui::PopID();
    bool hov = ImGui::IsItemHovered();
    helpTooltip(help);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(p, ImVec2(p.x + labelW - SP2 * S, p.y + h), true);
    drawText(dl, ImVec2(p.x, p.y + (h - 11 * S) / 2 - 1 * S), Label, hov && help ? INK : INK_MUTED, label);
    dl->PopClipRect();
    ImGui::SameLine(0, 0);
}

} // namespace ui
