#include "theme.hpp"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#include "gen/saira_regular.inc"
#include "gen/saira_medium.inc"

namespace ui {

Fonts fonts;
float S = 1;

static ImVec4 v4(unsigned hex, float a = 1) {
    return ImVec4(((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f, a);
}

void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig c;
    c.OversampleH = 2;
    fonts.regular = io.Fonts->AddFontFromMemoryCompressedTTF(saira_regular_compressed_data, saira_regular_compressed_size, 15, &c);
    fonts.medium = io.Fonts->AddFontFromMemoryCompressedTTF(saira_medium_compressed_data, saira_medium_compressed_size, 15, &c);
    io.FontDefault = fonts.regular;
}

void applyTheme(float scale) {
    S = scale;
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 10;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.FrameBorderSize = 0;
    s.PopupBorderSize = 1;
    s.WindowRounding = 0;
    s.ChildRounding = 10;
    s.FrameRounding = 7;
    s.PopupRounding = 8;
    s.GrabRounding = 7;
    s.ScrollbarRounding = 6;
    s.FontSizeBase = 15;
    s.FontScaleDpi = scale;
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = v4(SMOKE);
    c[ImGuiCol_TextDisabled] = v4(DIM);
    c[ImGuiCol_WindowBg] = v4(ASPHALT);
    c[ImGuiCol_ChildBg] = v4(TARMAC);
    c[ImGuiCol_PopupBg] = v4(TARMAC);
    c[ImGuiCol_Border] = v4(LINE);
    c[ImGuiCol_FrameBg] = v4(KERB);
    c[ImGuiCol_FrameBgHovered] = v4(LINE);
    c[ImGuiCol_FrameBgActive] = v4(LINE);
    c[ImGuiCol_Button] = v4(KERB);
    c[ImGuiCol_ButtonHovered] = v4(LINE);
    c[ImGuiCol_ButtonActive] = v4(SAKURA_DEEP);
    c[ImGuiCol_Header] = v4(KERB);
    c[ImGuiCol_HeaderHovered] = v4(LINE);
    c[ImGuiCol_HeaderActive] = v4(LINE);
    c[ImGuiCol_CheckMark] = v4(SAKURA);
    c[ImGuiCol_SliderGrab] = v4(SAKURA);
    c[ImGuiCol_SliderGrabActive] = v4(SAKURA_HI);
    c[ImGuiCol_ScrollbarBg] = v4(ASPHALT, 0);
    c[ImGuiCol_ScrollbarGrab] = v4(KERB);
    c[ImGuiCol_ScrollbarGrabHovered] = v4(LINE);
    c[ImGuiCol_ScrollbarGrabActive] = v4(DIM);
    c[ImGuiCol_Separator] = v4(LINE);
    c[ImGuiCol_TextSelectedBg] = v4(SAKURA, 0.35f);
    c[ImGuiCol_NavCursor] = v4(SAKURA);
    s.ScaleAllSizes(scale);
}

// ---------------------------------------------------------------------------
void text(const char* str, float size, unsigned color, bool medium) {
    ImGui::PushFont(medium ? fonts.medium : fonts.regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(color));
    ImGui::TextUnformatted(str);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Walks the string as digits (drawn in equal-width cells) and runs of other
// characters (drawn as-is, so UTF-8 like the degree sign stays intact).
template <class F> static void numRuns(const char* s, F fn) {
    const char* p = s;
    while (*p) {
        const char* e = p;
        if (isDigit(*p)) e++;
        else while (*e && !isDigit(*e)) e++;
        fn(p, e, isDigit(*p));
        p = e;
    }
}

float numWidth(const char* s, float size, bool medium) {
    ImFont* f = medium ? fonts.medium : fonts.regular;
    float px = size * S;
    float dw = f->CalcTextSizeA(px, 1e9f, 0, "0").x; // '0' is among Saira's widest digits
    float w = 0;
    numRuns(s, [&](const char* a, const char* b, bool digit) { w += digit ? dw : f->CalcTextSizeA(px, 1e9f, 0, a, b).x; });
    return w;
}

void drawNum(ImDrawList* dl, ImVec2 pos, const char* s, float size, unsigned color, bool medium) {
    ImFont* f = medium ? fonts.medium : fonts.regular;
    float px = size * S;
    float dw = f->CalcTextSizeA(px, 1e9f, 0, "0").x;
    numRuns(s, [&](const char* a, const char* b, bool digit) {
        float gw = f->CalcTextSizeA(px, 1e9f, 0, a, b).x;
        if (digit) {
            dl->AddText(f, px, ImVec2(std::floor(pos.x + (dw - gw) * 0.5f), pos.y), rgb(color), a, b);
            pos.x += dw;
        } else {
            dl->AddText(f, px, pos, rgb(color), a, b);
            pos.x += gw;
        }
    });
}

void helpTooltip(const char* help) {
    if (!help || !*help || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * S, 8 * S));
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(280 * S);
    ImGui::PushFont(fonts.regular, 14);
    ImGui::TextUnformatted(help);
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleVar();
}

void sectionTitle(const char* s) {
    ImGui::Dummy(ImVec2(0, 4 * S));
    text(s, 16, SMOKE, true);
    ImGui::Dummy(ImVec2(0, 1 * S));
}

// ---------------------------------------------------------------------------
bool slider(const char* id, const char* label, double* v, double mn, double mx, double step,
            const char* fmt, double def, const char* help) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float labelH = 15 * S, trackY = p.y + labelH + 11 * S, rowH = labelH + 20 * S;
    bool changed = false;

    ImGui::InvisibleButton("##hit", ImVec2(w, rowH));
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    helpTooltip(help);
    float x0 = p.x + 7 * S, x1 = p.x + w - 7 * S;
    if (act && !ImGui::IsMouseDoubleClicked(0)) {
        double t = (ImGui::GetIO().MousePos.x - x0) / (x1 - x0);
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        double nv = mn + t * (mx - mn);
        nv = std::round(nv / step) * step;
        if (std::fabs(nv - *v) > 1e-12) { *v = nv; changed = true; }
    }
    if (hov && ImGui::IsMouseClicked(1)) { *v = def; changed = true; }
    if (hov && ImGui::IsMouseDoubleClicked(0)) ImGui::OpenPopup("##edit");

    // label, value
    char buf[48];
    std::snprintf(buf, sizeof buf, fmt, *v);
    if (std::strncmp(buf, "-0", 2) == 0 && std::strtod(buf, nullptr) == 0) std::memmove(buf, buf + 1, std::strlen(buf));
    dl->AddText(fonts.regular, 15 * S, p, rgb(hov || act ? SMOKE : MUTED), label);
    bool isDef = std::fabs(*v - def) < step * 0.5;
    float vw = numWidth(buf, 15, true);
    drawNum(dl, ImVec2(p.x + w - vw, p.y), buf, 15, isDef ? MUTED : SMOKE, true);

    // track: fill from zero for ranges that cross zero, else from the minimum
    float t = (float)((*v - mn) / (mx - mn));
    float zero = mn < 0 && mx > 0 ? (float)((0 - mn) / (mx - mn)) : 0.f;
    float tx = x0 + (x1 - x0) * t, zx = x0 + (x1 - x0) * zero;
    dl->AddRectFilled(ImVec2(x0, trackY - 2 * S), ImVec2(x1, trackY + 2 * S), rgb(KERB), 2 * S);
    if (zero > 0) dl->AddRectFilled(ImVec2(zx - 1 * S, trackY - 5 * S), ImVec2(zx + 1 * S, trackY + 5 * S), rgb(LINE));
    dl->AddRectFilled(ImVec2(std::fmin(tx, zx), trackY - 2 * S), ImVec2(std::fmax(tx, zx), trackY + 2 * S), rgb(SAKURA, 220), 2 * S);
    // default value tick
    float dx = x0 + (x1 - x0) * (float)((def - mn) / (mx - mn));
    if (!isDef) dl->AddCircleFilled(ImVec2(dx, trackY), 2 * S, rgb(DIM));
    float r = (act ? 7.5f : hov ? 7.f : 6.f) * S;
    dl->AddCircleFilled(ImVec2(tx, trackY), r + 2 * S, rgb(ASPHALT, 200));
    dl->AddCircleFilled(ImVec2(tx, trackY), r, rgb(act ? SAKURA_HI : SAKURA));

    // typed value
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * S, 10 * S));
    if (ImGui::BeginPopup("##edit")) {
        static double tmp;
        if (ImGui::IsWindowAppearing()) { tmp = *v; ImGui::SetKeyboardFocusHere(); }
        ImGui::SetNextItemWidth(120 * S);
        if (ImGui::InputDouble("##v", &tmp, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
            *v = tmp < mn ? mn : tmp > mx ? mx : tmp;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0, 2 * S));
    ImGui::PopID();
    return changed;
}

bool segmented(const char* id, int* v, const char* const* items, int n, float width) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = width > 0 ? width : ImGui::GetContentRegionAvail().x, h = 32 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), rgb(KERB), 8 * S);
    bool changed = false;
    float cw = w / n;
    for (int i = 0; i < n; i++) {
        ImVec2 a(p.x + cw * i, p.y), b(a.x + cw, p.y + h);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##s", ImVec2(cw, h))) { if (*v != i) { *v = i; changed = true; } }
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (*v == i) dl->AddRectFilled(ImVec2(a.x + 3 * S, a.y + 3 * S), ImVec2(b.x - 3 * S, b.y - 3 * S), rgb(ASPHALT), 6 * S);
        ImVec2 ts = fonts.medium->CalcTextSizeA(14 * S, 1e9f, 0, items[i]);
        dl->AddText(fonts.medium, 14 * S, ImVec2(std::floor((a.x + b.x - ts.x) / 2), std::floor((a.y + b.y - ts.y) / 2)),
                    rgb(*v == i ? SMOKE : hov ? SMOKE : MUTED), items[i]);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, 0));
    ImGui::PopID();
    return changed;
}

bool button(const char* label, bool primary, float width, bool enabled) {
    ImGui::BeginDisabled(!enabled);
    ImGui::PushFont(fonts.medium, 15);
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(SAKURA));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(SAKURA_HI));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(SAKURA_DEEP));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(ASPHALT));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(KERB));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(LINE));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(LINE));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(SMOKE));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * S, 8 * S));
    bool r = ImGui::Button(label, ImVec2(width, 0));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
    ImGui::EndDisabled();
    return r;
}

bool toggle(const char* id, const char* label, bool* v, const char* help) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x, h = 22 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool changed = false;
    if (ImGui::InvisibleButton("##t", ImVec2(w, h))) { *v = !*v; changed = true; }
    bool hov = ImGui::IsItemHovered();
    helpTooltip(help);
    dl->AddText(fonts.regular, 15 * S, ImVec2(p.x, p.y + 2 * S), rgb(hov ? SMOKE : MUTED), label);
    float sw = 34 * S, sh = 18 * S;
    ImVec2 a(p.x + w - sw, p.y + (h - sh) / 2), b(a.x + sw, a.y + sh);
    dl->AddRectFilled(a, b, rgb(*v ? SAKURA : KERB), sh / 2);
    float kx = *v ? b.x - sh / 2 : a.x + sh / 2;
    dl->AddCircleFilled(ImVec2(kx, a.y + sh / 2), sh / 2 - 3 * S, rgb(*v ? ASPHALT : MUTED));
    ImGui::PopID();
    return changed;
}

} // namespace ui
