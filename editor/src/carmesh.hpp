// Preview car mesh in car space (x right, y up from the ground, z forward,
// metres). Triangles are wound so their normal points outward.
//
// Model: "Nissan Silvia Drift Tuned (S15)" by VuckyZ, CC BY 4.0
// https://sketchfab.com/3d-models/nissan-silvia-drift-tuned-s15-84b63a057e36488589392a19b9313e2b
// Simplified and rescaled by tools/s15_convert.py (see its header for the changes).
#pragma once
#include "sim.hpp"
#include <vector>

namespace fh6 {

struct CarMesh {
    struct Face { int idx[3]; int part; }; // part: 0 body, 1 wheel, 2 detail
    std::vector<V3> verts;
    std::vector<Face> faces;
};

const CarMesh& carMesh();

extern const char* const CAR_CREDIT; // short attribution for the UI

} // namespace fh6
