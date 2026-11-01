// Core tests (native build): core_test            -> unit tests
//                            core_test STOCK OUT   -> writes the default build of STOCK to OUT
#include "../src/core.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace fh6;

static void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

int main(int argc, char** argv) {
    if (argc == 3) {
        std::string ini, xml, dir = argv[1], out = argv[2];
        check(readFile(dir + "/CameraSettings.ini", ini) && readFile(dir + "/CameraPhysics.xml", xml), "read stock");
        Built b = build(ini, xml, CamParams());
        check(writeFile(out + "/CameraSettings.ini", b.ini) && writeFile(out + "/CameraPhysics.xml", b.xml), "write");
        for (auto& c : b.changes) std::printf("  %-40s %8.4g -> %.4g\n", c.key.c_str(), c.before, c.after);
        std::printf("  layers %d -> %d\n", b.layersBefore, b.layersAfter);
        return 0;
    }
    // numbers formatted like the game's ini
    check(fmtShort(1.327) == "1.327" && fmtShort(-0.8) == "-0.8" && fmtShort(66) == "66" && fmtShort(-0.00001) == "0", "fmtShort");
    // relative, scaled, absolute
    std::string t = "A\\X 1.5\nA\\Y 8.5\nA\\Z 60\nA\\ZZ 1\nB\\X 3\n";
    t = editIni(t, "A", "X", -0.7, true, false, nullptr);
    t = editIni(t, "A", "Y", 0.8, false, true, nullptr);
    t = editIni(t, "A", "Z", 66, false, false, nullptr);
    check(t == "A\\X 0.8\nA\\Y 6.8\nA\\Z 66\nA\\ZZ 1\nB\\X 3\n", "ini edits");
    bool threw = false;
    try { editIni(t, "A", "Nope", 1, false, false, nullptr); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "missing key throws");
    // Windows line endings, indentation and NumLayers
    std::string x = "<CameraPhysics>\r\n  <FollowCam2 NumLayers=\"1\">\r\n    <Layer Enabled=\"1\" InputType=\"Gs_Lateral\" />\r\n  </FollowCam2>\r\n</CameraPhysics>\r\n";
    std::string nx = appendLayers(x, "FollowCam2", { layer("SpeedMPH", "FOV", 1, 0, 0, 0, 1, {}, false, 0, 0) });
    check(nx.find("NumLayers=\"2\"") != std::string::npos, "NumLayers updated");
    check(nx.find("\r\n    <Layer Enabled=\"1\" LocationType=\"All\" InputType=\"SpeedMPH\"") != std::string::npos, "same indent");
    std::string tail = "  </FollowCam2>\r\n</CameraPhysics>\r\n";
    check(nx.size() >= tail.size() && nx.compare(nx.size() - tail.size(), tail.size(), tail) == 0, "closing tag kept");
    for (size_t i = 0; i < nx.size(); i++) if (nx[i] == '\n') check(i > 0 && nx[i - 1] == '\r', "CRLF only");
    check(countLayers(nx, "FollowCam2") == 2, "count");
    threw = false;
    try { countLayers(nx, "FollowCam"); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "FollowCam is not FollowCam2");
    std::printf("core unit tests OK\n");
    return 0;
}
