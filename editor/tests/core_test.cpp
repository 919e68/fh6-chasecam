// Core tests (native build): core_test                -> unit tests
//                            core_test STOCK OUT       -> writes the default build of STOCK to OUT
//                            core_test --ref DIR       -> unit tests + checks against real camera files in DIR
#include "../src/camfiles.hpp"
#include "../src/core.hpp"
#include "../src/layereval.hpp"
#include "../src/schema.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace fh6;

static void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

static void refTests(const std::string& dir) {
    std::string ini, xml;
    check(readFile(dir + "/CameraSettings.ini", ini) && readFile(dir + "/CameraPhysics.xml", xml), "read reference files");
    // every key in the file is in the schema
    int known = 0, total = 0;
    for (auto& e : iniEntries(ini)) { total++; if (findKey(e.key)) known++; else std::printf("  not in schema: %s\n", e.key.c_str()); }
    std::printf("  ini: %d of %d keys in the schema\n", known, total);
    check(known == total, "schema covers every key");
    // writing back each key's own value changes nothing
    std::string same = ini;
    for (auto& e : iniEntries(ini)) same = iniSet(same, e.key, e.value);
    check(same == ini, "iniSet with the same values is a no-op");
    // the layer writer reproduces the game's attribute order for every standard layer
    auto secs = xmlSections(xml);
    int layers = 0, exact = 0, constant = 0;
    for (auto& s : secs)
        for (auto& t : s.layers) {
            layers++;
            Layer l = parseLayer(t);
            if (l.str("InputType") == "Constant") { constant++; continue; }
            Layer n = l;
            normalizeLayer(n);
            if (writeLayer(n) == t) exact++;
            else std::printf("  differs: %.120s\n", t.c_str());
        }
    std::printf("  xml: %zu sections, %d layers, %d rewritten byte-exact, %d hand-made Constant layers\n", secs.size(), layers, exact, constant);
    check(exact == layers - constant, "normalizeLayer reproduces every standard layer");
    // a section rewritten with its own layers keeps them and gets the real count
    // (whitespace is tidied: mod files have stray blank lines and indents)
    for (auto& s : secs) {
        std::string r = replaceSectionLayers(xml, s.name, s.layers);
        auto rs = xmlSections(r);
        const XmlSection* n = findSection(rs, s.name);
        check(n && n->layers == s.layers && n->declared == (int)s.layers.size(), "layers kept, NumLayers = real count");
        for (auto& o : secs) if (o.name != s.name) check(findSection(rs, o.name)->layers == o.layers, "other sections untouched");
    }
    // empty edits: same as the plain build
    Built a = build(ini, xml, CamParams()), b = buildAll(ini, xml, Edits(), CamParams());
    check(a.ini == b.ini && a.xml == b.xml, "buildAll without edits = build");
    // the chase sections run in the evaluator
    LayerStack st;
    st.load(findSection(secs, "FollowCam")->layers);
    InputFrame f;
    f.v[inputIndex("SpeedMPH")] = 120;
    LayerOut o;
    for (int i = 0; i < 300; i++) o = st.step(f, 1 / 60.0);
    std::printf("  FollowCam at 120 mph: FOV %+.2f, pull %.3f m\n", o.fov, o.carXYZ[2]);
    check(std::isfinite(o.fov) && std::isfinite(o.camYPR[0]), "evaluator stays finite");
}

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--ref") != 0) {
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

    // --- file model ---
    // iniSet: in place, into its block, CRLF, end of file
    std::string ini = "A\\X 1\nA\\S\\K 2\n\nB\\X 3\n";
    check(iniSet(ini, "A\\X", "1.5") == "A\\X 1.5\nA\\S\\K 2\n\nB\\X 3\n", "iniSet replaces");
    check(iniSet(ini, "A\\S\\K2", "7") == "A\\X 1\nA\\S\\K 2\nA\\S\\K2 7\n\nB\\X 3\n", "iniSet inserts into its block");
    check(iniSet(ini, "B\\Y", "4") == "A\\X 1\nA\\S\\K 2\n\nB\\X 3\nB\\Y 4\n", "iniSet inserts at the block end");
    check(iniSet(ini, "C\\Z", "5") == ini + "\nC\\Z 5\n", "iniSet adds a new block");
    check(iniSet("A\\X 1\r\nB\\X 2\r\n", "A\\Y", "3") == "A\\X 1\r\nA\\Y 3\r\nB\\X 2\r\n", "iniSet keeps CRLF");
    check(iniValues(ini)["A\\S\\K"] == 2, "iniValues");
    // layers: parse, write, normalize, switch groups on and off
    std::string lt = layer("Slip_Lateral", "CameraSpaceYPROffset", 6, 0, 0, -8, 8, DRIFT_CURVE, true, 10, 1);
    Layer l = parseLayer(lt);
    check(writeLayer(l) == lt, "layer round trip");
    Layer n = l;
    normalizeLayer(n);
    check(writeLayer(n) == lt, "normalize keeps a game-shaped layer");
    n.set("Sprung", "0");
    normalizeLayer(n);
    check(!n.has("SpringK") && !n.has("SpringDPercent"), "Sprung off drops the spring");
    n.set("NoiseType", "Perlin");
    n.set("InputMagAttackDecay", "1");
    normalizeLayer(n);
    check(n.has("NoiseFreq") && n.has("InputToNoiseFreqMapping") && n.has("InputAttack"), "noise and envelope groups added");
    check(n.a[9].name == "InputAttack" && n.a[11].name == "InputToNoiseFreqMapping", "group order");
    n.set("OutputType", "CarSpaceYPRImpulse");
    normalizeLayer(n);
    check(!n.has("Sprung") && n.has("ImpulseDelay") && n.has("SpringK"), "impulse shape");
    setMapping(n, "InputToParam", "FivePoint");
    CurvePts c5 = curvePts(n, "InputToParam");
    check(c5.n == 5 && c5.mirrored && std::fabs(curveEval(c5, 2.5) - 0.6) < 0.08, "resampled curve");
    check(std::fabs(curveEval(c5, -2.5) + curveEval(c5, 2.5)) < 1e-9, "mirrored = odd");
    setMapping(n, "InputToParam", "Linear");
    check(!n.has("InputToParamCurveInput0") && !n.has("InputToParamCurveMirrored"), "Linear has no points");
    // hand-made layers are left alone
    Layer hm = parseLayer("<Layer InputType=\"Constant\" OutputType=\"FOV\" NoiseType=\"Perlin\" Enabled=\"1\" FrequencyMultiplier=\"0.5\" />");
    std::string hmt = writeLayer(hm);
    normalizeLayer(hm);
    check(writeLayer(hm) == hmt, "Constant layer untouched");
    // sections and edits
    std::string px = "<CameraPhysics>\n  <FollowCam NumLayers=\"1\">\n  " + lt + "\n  </FollowCam>\n  <Hood NumLayers=\"0\">\n  </Hood>\n</CameraPhysics>\n";
    auto secs = xmlSections(px);
    check(secs.size() == 2 && secs[0].layers.size() == 1 && secs[0].layers[0] == lt, "sections");
    check(replaceSectionLayers(px, "FollowCam", secs[0].layers) == px, "rewrite unchanged");
    std::string r2 = replaceSectionLayers(px, "Hood", { lt, lt });
    check(r2.find("<Hood NumLayers=\"2\">\n  " + lt + "\n  " + lt + "\n  </Hood>") != std::string::npos, "rewrite Hood");
    Edits e;
    e.ini["A\\X"] = "9";
    e.sections["Hood"] = { lt };
    e.sections["FollowCam"] = {};
    Edits e2;
    check(readEdits(writeEdits(e), e2) && e2.ini == e.ini && e2.sections == e.sections, "edits round trip");
    std::string oi, ox;
    applyEdits(ini, px, e, oi, ox);
    check(oi.find("A\\X 9\n") == 0 && xmlSections(ox)[0].layers.empty() && xmlSections(ox)[1].layers.size() == 1, "applyEdits");
    // evaluator: a drift look layer settles at its full Param at big slip
    LayerStack st;
    st.load({ lt });
    InputFrame f;
    f.v[inputIndex("Slip_Lateral")] = 8;
    LayerOut o;
    for (int i = 0; i < 240; i++) o = st.step(f, 1 / 60.0);
    check(std::fabs(o.camYPR[0] - 6) < 0.05, "drift look settles");
    f.v[inputIndex("Slip_Lateral")] = -8;
    for (int i = 0; i < 240; i++) o = st.step(f, 1 / 60.0);
    check(std::fabs(o.camYPR[0] + 6) < 0.05, "mirrored the other way");
    // schema: built-in ini parses, keys unique
    auto be = iniEntries(builtinIni());
    check(be.size() == 140, "built-in ini has the 140 listed keys");
    int dup = 0;
    for (auto& g : cameraGroups()) for (auto& k : g.keys) for (auto& g2 : cameraGroups()) for (auto& k2 : g2.keys) if (&k != &k2 && k.key == k2.key) dup++;
    check(dup == 0, "unique keys");
    // camera reset: only that camera's settings and layers go back, and the result equals the loaded files
    {
        int nearG = -1, farG = -1, hoodG = -1;
        for (size_t i = 0; i < cameraGroups().size(); i++) {
            std::string n = cameraGroups()[i].name;
            if (n == "Chase Near") nearG = (int)i;
            if (n == "Chase Far") farG = (int)i;
            if (n == "Hood") hoodG = (int)i;
        }
        check(groupOfKey("FollowLowCam\\FOV") == nearG && groupOfKey("FollowHighCam\\Spring\\YRotSpringK") == farG && groupOfKey("FollowHighCam\\Spring\\NotAKey") == farG &&
                  groupOfKey("HoodCam\\FOV") == hoodG && groupOfSection("FollowCam2") == farG && groupOfSection("Copter") == -1,
              "key and section lookup");
        std::string bi = builtinIni(), bx = "<CameraPhysics>\n  <FollowCam NumLayers=\"1\">\n  " + lt + "\n  </FollowCam>\n  <FollowCam2 NumLayers=\"1\">\n  " + lt +
                                              "\n  </FollowCam2>\n</CameraPhysics>\n";
        Edits ed;
        ed.ini["FollowHighCam\\FOV"] = "70";
        ed.ini["FollowHighCam\\Spring\\YRotSpringK"] = "5";
        ed.ini["FollowLowCam\\FOV"] = "50";
        ed.ini["HoodCam\\FOV"] = "80";
        ed.sections["FollowCam2"] = {};
        ed.sections["FollowCam"] = { lt, lt };
        CameraChanges cf = cameraChanges(ed, farG);
        check(cf.keys == 2 && cf.layers, "changes on Chase Far");
        resetCamera(ed, farG);
        check(ed.ini.size() == 2 && ed.ini.count("FollowLowCam\\FOV") && ed.ini.count("HoodCam\\FOV") && ed.sections.size() == 1 &&
                  ed.sections.count("FollowCam"),
              "reset leaves other cameras");
        Built rb = buildAll(bi, bx, ed, noChangeParams(1));
        auto far = [](const std::string& t) {
            std::string o;
            for (auto& e : iniEntries(t)) if (e.key.rfind("FollowHighCam", 0) == 0) o += e.key + " " + e.value + "\n";
            return o;
        };
        check(far(rb.ini) == far(bi), "Chase Far settings back to the loaded file");
        check(findSection(xmlSections(rb.xml), "FollowCam2")->layers == findSection(xmlSections(bx), "FollowCam2")->layers, "Chase Far layers back");
        check(findSection(xmlSections(rb.xml), "FollowCam")->layers.size() == 2, "Chase Near layers still edited");
        check(isNoChange(noChangeParams(1)) && !isNoChange(CamParams()), "no-change drift setup");
        Built nb = buildAll(bi, bx, Edits(), noChangeParams(1));
        check(nb.ini == bi && nb.xml == bx, "no edits and no drift setup = the loaded files, byte for byte");
    }
    std::printf("core unit tests OK\n");
    if (argc == 3) refTests(argv[2]);
    return 0;
}
