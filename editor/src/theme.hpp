// Countersteer UI kit (garage theme) for Dear ImGui: tokens, type styles and
// components, following the kit's tokens.json and component guidelines.
#pragma once
#include "imgui.h"

namespace ui {

constexpr ImU32 rgb(unsigned hex, unsigned a = 255) {
    return IM_COL32((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, a);
}

// Color (garage)
inline const unsigned BG000 = 0x07080A, BG100 = 0x0C0E11, BG200 = 0x12151A, BG300 = 0x1A1E24,
                      LINE = 0x262B33, LINE_STRONG = 0x66707E,
                      INK = 0xEEF1F5, INK_MUTED = 0x9AA4B1, INK_DISABLED = 0x58616E,
                      ORANGE = 0xFF6A13, ORANGE_PRESS = 0xE85A0A, ORANGE_HOT = 0xFFB23E, ORANGE_SOFT = 0x2A1408,
                      ON_ORANGE = 0x0C0E11, COOL = 0x4CC9F0, DANGER = 0xFF5C66, FOCUS_RING = 0xF5F7FA,
                      ON_GLASS = 0xEEF1F5, ON_GLASS_MUTED = 0xAEB7C2;
inline ImU32 glass(int pct) { return IM_COL32(0, 0, 0, pct * 255 / 100); }
inline const ImU32 GLASS_LINE = IM_COL32(255, 255, 255, 36);

// Space, shape, stroke (px, before DPI scale)
enum : int { SP1 = 4, SP2 = 8, SP3 = 12, SP4 = 16, SP6 = 24, SP8 = 32, SP12 = 48 };
enum : int { CH_SM = 6, CH_MD = 10, CH_LG = 14 };
constexpr float HAIR = 1, RULE = 2;

extern float S; // DPI scale

// Type styles
enum Style { Headline, Title, Nav, Label, Body, BodyStrong, Caption, ReadoutXL, Readout, Stat, BtnText, BtnTextLg, Sub, Key, Lockup };
struct TypeStyle { ImFont* font; float size; float track; bool upper; };
TypeStyle typeStyle(Style s);

void loadFonts();
void applyTheme(float scale);

// Text with the style's case and letter spacing
float textWidth(Style s, const char* t);
float textHeight(Style s);
void drawText(ImDrawList* dl, ImVec2 pos, Style s, unsigned color, const char* t, int alpha = 255);
void text(Style s, unsigned color, const char* t);                 // as a layout item
void textWrapped(Style s, unsigned color, const char* t, float w); // sentence-case styles only

// Shapes: one cut corner, top right
void cutFill(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 col);
void cutLine(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 col, float th);
// Panel: bg-200, line hairline, cut corner; brackets = orange corner ticks (one panel per screen);
// glassPct 0 = solid, else black glass at that opacity.
void panel(ImDrawList* dl, ImVec2 a, ImVec2 b, bool brackets, int glassPct = 0);

// Components
enum class Btn { Primary, Secondary, Ghost, Danger };
float buttonWidth(const char* label, Btn variant, bool large = false, const char* hint = nullptr, const char* sub = nullptr);
bool button(const char* label, Btn variant, bool large = false, float width = 0, bool enabled = true,
            const char* hint = nullptr, const char* sublabel = nullptr);
bool slider(const char* id, const char* label, double* v, double min, double max, double step,
            const char* unit, int decimals, bool signedDelta, double def, const char* help);
bool toggle(const char* id, const char* label, bool* v, const char* help);
bool segmentTabs(const char* id, int* v, const char* const* items, int n);
float segmentTabsWidth(const char* const* items, int n);
enum class Tone { Neutral, Orange, Cool, Danger };
void tag(ImDrawList* dl, ImVec2 pos, Tone tone, const char* t); // outline, one cut corner
float tagWidth(const char* t);
void groupTitle(const char* t, bool first); // hairline above, then the group name
void helpTooltip(const char* help);

// Numbers: deltas carry a sign with a space ("+ 6", "− 0.70"), real minus sign
void fmtNum(char* out, size_t n, double v, int decimals, bool signedDelta);

} // namespace ui
