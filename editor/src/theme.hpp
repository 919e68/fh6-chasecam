// Look of the editor: asphalt greys, one sakura accent, Saira type.
#pragma once
#include "imgui.h"

namespace ui {

// Palette
constexpr ImU32 rgb(unsigned hex, unsigned a = 255) {
    return IM_COL32((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, a);
}
inline const unsigned ASPHALT = 0x16181D, TARMAC = 0x1E2128, KERB = 0x2A2E37, LINE = 0x343946,
                      SMOKE = 0xD9DCE3, MUTED = 0x8A909C, DIM = 0x5D6370,
                      SAKURA = 0xF29BBF, SAKURA_HI = 0xF7BCD3, SAKURA_DEEP = 0xB9708F,
                      AMBER = 0xE9A23B, RED = 0xE5534B;

struct Fonts { ImFont* regular = nullptr; ImFont* medium = nullptr; };
extern Fonts fonts;
extern float S; // UI scale (DPI)

void loadFonts();
void applyTheme(float scale);

// Text helpers. Sizes are unscaled pixels.
void text(const char* s, float size, unsigned color, bool medium = false);
// Numbers on a fixed digit grid so live values don't jitter. Returns width.
float numWidth(const char* s, float size, bool medium = false);
void drawNum(ImDrawList* dl, ImVec2 pos, const char* s, float size, unsigned color, bool medium = false);

// Controls
// Slider row: label and value on one line, thin track below. Bipolar ranges
// fill from zero. Right-click resets, double-click types a value.
bool slider(const char* id, const char* label, double* v, double min, double max, double step,
            const char* fmt, double def, const char* help);
bool segmented(const char* id, int* v, const char* const* items, int count, float width = 0);
bool button(const char* label, bool primary, float width = 0, bool enabled = true);
bool toggle(const char* id, const char* label, bool* v, const char* help);
void sectionTitle(const char* s);
void helpTooltip(const char* help);

} // namespace ui
