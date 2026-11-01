// Preview car: a generated high-poly coupe in car space (x right, y up from
// the ground, z forward, metres). Built once.
#pragma once
#include "sim.hpp"
#include <vector>

namespace fh6 {

struct CarMesh {
    struct Face { int idx[4]; int n; int part; }; // n = 3 or 4 vertices; part: 0 body, 1 wheel, 2 detail
    std::vector<V3> verts;
    std::vector<Face> faces;
    std::vector<V3> inside; // per face: a point inside its part, to orient normals outward
};

const CarMesh& carMesh();

} // namespace fh6
