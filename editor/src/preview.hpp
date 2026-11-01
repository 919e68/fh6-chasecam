#pragma once
#include "imgui.h"
#include "sim.hpp"

namespace ui {
// Draws the preview scene seen from the simulated camera into [a, b].
// The 16:9 frame [fa, fb] sets the projection (what the game would show); the
// scene is drawn across [a, b] and dimmed outside the frame.
void drawPreview(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec2 fa, ImVec2 fb, const fh6::CamPose& cam, const fh6::Inputs& in, const fh6::Scene& scene);
}
