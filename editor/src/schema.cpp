#include "schema.hpp"
#include "core.hpp"
#include <cstdio>

namespace fh6 {

const char* confTag(Conf c) { return c == Conf::High ? "Known" : c == Conf::Med ? "Likely" : "Unsure"; }
const char* confHelp(Conf c) {
    return c == Conf::High ? "Decoded from Forza game code with the same name, or used by FH6 mods with this effect."
           : c == Conf::Med ? "Worked out from the name, the values and how the game's files use it."
                            : "Meaning unknown or a guess. Test it in game.";
}

static const char* DEG = "\xC2\xB0";

namespace {
struct Builder {
    std::vector<KeyInfo>& out;
    std::string prefix, group;
    void operator()(const std::string& k, const std::string& label, const std::string& unit, double mn, double mx, double step, int dec,
                    Conf c, const std::string& help, double def, unsigned flags = 0, const std::string& pair = "", bool listed = true) {
        out.push_back({ prefix + k, group, label, unit, mn, mx, step, dec, c, help, listed, def, flags, pair.empty() ? "" : prefix + pair });
    }
};

std::vector<KeyInfo> chaseKeys(const std::string& p, bool high) {
    std::vector<KeyInfo> v;
    Builder K{ v, p + "\\", "Position" };
    K("HeightMinAboveGround", "Lowest height", " m", 0.3, 5, 0.01, 2, Conf::High,
      "Lowest camera height above the road. The game sets this equal to the highest height, which pins the camera height. "
      "Change both together to raise or lower the camera; the Camera height control does that.",
      high ? 2.027 : 1.7133, K_HEIGHT | K_PERCAR);
    K("HeightMaxAboveGround", "Highest height", " m", 0.3, 5, 0.01, 2, Conf::High,
      "Highest camera height above the road. Keep it at or above the lowest height.", high ? 2.027 : 1.7133, K_HEIGHT | K_PERCAR);
    K("HeightOffsetGlobal", "Height offset", " m", -3, 3, 0.01, 2, Conf::High,
      "Height above the car's roof, before the lowest and highest heights clamp it. While those two are equal, raising this "
      "lowers the aim point instead of moving the camera. Cars with their own camera data ignore it.",
      high ? -1.6751 : 0.6988, K_PERCAR);
    K("RadiusOffsetGlobal", "Distance", " m", -2, 5, 0.01, 2, Conf::High,
      "Distance behind the back of the car, added to the game's base distance. Negative: closer. Cars with their own camera data ignore it.",
      high ? -0.3 : -0.35, K_PERCAR);
    K("TargetYOffsetGlobal", "Aim height", " m", -3, 3, 0.01, 2, Conf::High,
      "Height of the point the camera looks at, above the car's roof. Higher: the car sits lower in the picture. "
      "Cars with their own camera data ignore it.",
      high ? -1.804 : 0.6988, K_PERCAR);
    K.group = "Lens";
    K("FOV", "Field of view", DEG, 20, 120, 0.5, 1, Conf::High,
      "Base field of view in vertical degrees (55 is about 86 horizontal at 16:9). The FOV slider in FH6's Video settings "
      "probably overrides it within the limits below.",
      high ? 60 : 55, K_FOV);
    K("MinFOV", "Narrowest field of view", DEG, 10, 90, 0.5, 1, Conf::Med,
      "Likely the bottom of the in-game FOV slider for this camera.", 40, K_FOV);
    K("MaxFOV", "Widest field of view", DEG, 30, 150, 0.5, 1, Conf::Med,
      "Likely the top of the in-game FOV slider, and the cap for speed widening. FOV mods raise it to about 120.", 70, K_FOV);
    K("FOVRatio", "FOV ratio", "", 0, 1, 0.01, 2, Conf::Low,
      "Unknown. Possibly how much of the speed widening is offset by moving in, or a split-screen scale.", 0.5);
    K.prefix = p + "\\Spring\\";
    K.group = "Turn follow";
    K("YRotSpringK", "Turn stiffness", "", 0.5, 30, 0.1, 2, Conf::High,
      "How hard the camera is pulled back behind the car when the car turns. Higher: follows tighter. "
      "Together with turn damping it sets how fast and how bouncy the camera swings.",
      8.5, K_SPRING_K, "YRotSpringD");
    K("YRotSpringD", "Turn damping", "", 0, 15, 0.05, 2, Conf::High,
      "Stops the camera swinging past the car. Higher: less overshoot but more lag.", 1.2, K_SPRING_D, "YRotSpringK");
    K("YRotCarDampScale", "Turn with the car", "", 0, 1.5, 0.01, 2, Conf::High,
      "How much of the car's rotation the camera copies straight away. Near 0: the camera lags and shows the drift angle. "
      "Near 1: it rotates with the car.",
      0.2);
    K("YRotMaxSpeed", "Max turn speed", " rad/s", 0.5, 10, 0.05, 2, Conf::High,
      "Fastest the camera may rotate around the car (2 rad/s = 115\xC2\xB0 per second).", 4.05);
    K.group = "Height follow";
    K("HeightSpringK", "Height stiffness", "", 5, 200, 0.5, 1, Conf::High,
      "How hard the camera is pulled to its height with wheels on the ground. Higher: follows bumps tighter.", 93.5, K_SPRING_K, "HeightSpringD");
    K("HeightSpringD", "Height damping", "", 0, 30, 0.05, 2, Conf::High,
      "Stops the camera bouncing up and down. Higher: calmer over bumps.", 3.85, K_SPRING_D, "HeightSpringK");
    K("HeightSpringKInAir", "Height stiffness in the air", "", 1, 100, 0.5, 1, Conf::High,
      "Height stiffness while no wheel touches the ground.", 18, K_SPRING_K, "HeightSpringDInAir");
    K("HeightSpringDInAir", "Height damping in the air", "", 0, 30, 0.05, 2, Conf::Med,
      "Height damping while airborne.", 1.5, K_SPRING_D, "HeightSpringKInAir");
    K.group = "Slopes and jumps";
    K("HeightSpringRotMaxSpeed", "Slope follow speed", "", 0, 30, 0.1, 1, Conf::Med,
      "Speed cap on the part of the height that follows the car's pitch over crests and dips. Lower: smoother. "
      "FH6 players tune this against chase camera shake.",
      12);
    K("HeightSpringRotMaxSpeedInAirPos", "Slope follow, air, up", "", 0, 15, 0.1, 1, Conf::Med, "Same cap while airborne, moving up.", 4);
    K("HeightSpringRotMaxSpeedInAirNeg", "Slope follow, air, down", "", 0, 15, 0.1, 1, Conf::Med, "Same cap while airborne, moving down.", 2.5);
    K("HeightSpringRotDampScale", "Slope follow damping", "", 0, 1.5, 0.01, 2, Conf::Low, "Damping scale on the slope-follow part.", 0.3);
    K("MinTimerOffGround", "Air delay", " s", 0, 2, 0.01, 2, Conf::Med,
      "Time off the ground before the camera switches to its in-air behaviour.", 0.2);
    K("MinOrientationUp", "Upright limit", "", 0, 1, 0.01, 2, Conf::Med,
      "How upright the car must be for the camera to follow its tilt (0.4 is about 66\xC2\xB0 of tilt).", 0.4);
    K("MinClampHeight", "Lowest slope offset", " m", -10, 0, 0.1, 1, Conf::Med, "Lower limit on the slope-follow height offset.", -4.5);
    K("MaxClampHeight", "Highest slope offset", " m", 0, 10, 0.1, 1, Conf::Med, "Upper limit on the slope-follow height offset.", 4.5);
    K.group = "Bump dampers";
    K("HeightSlowBumpDamper", "Slow rise damper", "", 0, 2, 0.01, 2, Conf::Low, "Damping while the camera rises slowly.", 0.44);
    K("HeightSlowRebDamper", "Slow drop damper", "", 0, 2, 0.01, 2, Conf::Low, "Damping while the camera drops slowly.", 0.44);
    K("HeightFastBumpDamper", "Fast rise damper", "", 0, 2, 0.01, 2, Conf::Low, "Damping for fast upward moves.", 0.66);
    K("HeightFastRebDamper", "Fast drop damper", "", 0, 2, 0.01, 2, Conf::Low, "Damping for fast downward moves.", 0.33);
    K("HeightFastBumpVelTreshold", "Fast rise from", "", 0, 2, 0.01, 2, Conf::Low,
      "Speed that splits slow and fast rising. The game spells the key without the h in threshold.", 0.2);
    K("HeightFastRebVelTreshold", "Fast drop from", "", 0, 2, 0.01, 2, Conf::Low, "Speed that splits slow and fast dropping.", 0.1);
    K.prefix = p + "\\";
    K.group = "Clipping and see-through";
    K("CamNearClip", "Near clip", " m", 0.05, 1, 0.01, 2, Conf::Med,
      "Anything closer than this is not drawn. Higher hides grass and car parts right at the lens.", 0.4);
    K("AutoGhostIntersectionAlpha", "See-through opacity", "", 0, 1, 0.01, 2, Conf::Med,
      "How visible objects that block the view stay (0.01 = nearly invisible).", 0.01);
    K("AutoGhostIntersectionAlphaDistance", "See-through distance", " m", 0, 10, 0.1, 1, Conf::Med,
      "Objects closer than this to the camera turn see-through.", 2.5);
    K.group = "Not in the game's file (untested)";
    K.prefix = p + "\\Spring\\";
    K("HeightSpringK_PDLC", "Height stiffness, DLC cars", "", 5, 200, 0.5, 1, Conf::Low,
      "From Forza Horizon 2, where some DLC cars had their own height spring. May do nothing in FH6.", 70, K_SPRING_K, "HeightSpringD_PDLC", false);
    K("HeightSpringD_PDLC", "Height damping, DLC cars", "", 0, 30, 0.05, 2, Conf::Low, "From FH2. May do nothing in FH6.", 24.95,
      K_SPRING_D, "HeightSpringK_PDLC", false);
    K("HeightSpringKInAir_PDLC", "Air stiffness, DLC cars", "", 1, 100, 0.5, 1, Conf::Low, "From FH2. May do nothing in FH6.", 11, 0, "", false);
    K("HeightSpringDInAir_PDLC", "Air damping, DLC cars", "", 0, 30, 0.05, 2, Conf::Low, "From FH2. May do nothing in FH6.", 8.55, 0, "", false);
    K.prefix = p + "\\";
    K("CamFarClip", "Far clip", " m", 1000, 100000, 100, 0, Conf::Low,
      "Draw distance. FH1 code reads it for every camera; FH6's file has it only for the cockpit.", 50000, 0, "", false);
    return v;
}

std::vector<CameraGroup> makeGroups() {
    std::vector<CameraGroup> g;
    g.push_back({ "Chase Near", "The low, close chase camera. Its effect layers are the FollowCam section.", 0, chaseKeys("FollowLowCam", false) });
    g.push_back({ "Chase Far", "The higher, further chase camera. Its effect layers are the FollowCam2 section.", 1, chaseKeys("FollowHighCam", true) });
    {
        std::vector<KeyInfo> v;
        Builder K{ v, "Driver\\", "Lens" };
        K("FOV", "Field of view", DEG, 30, 120, 0.5, 1, Conf::High, "Base cockpit field of view, vertical degrees.", 85, K_FOV);
        K("MinFOV", "Narrowest field of view", DEG, 10, 150, 0.5, 1, Conf::Med, "Likely the bottom of the in-game FOV slider.", 40, K_FOV);
        K("MaxFOV", "Widest field of view", DEG, 10, 150, 0.5, 1, Conf::Med, "Likely the top of the in-game FOV slider.", 90, K_FOV);
        K.prefix = "CockpitCam\\";
        K("FOVRatio", "FOV ratio", "", 0, 1, 0.01, 2, Conf::Low, "Unknown, like the other FOV ratio keys.", 0.5);
        K.prefix = "Driver\\";
        K.group = "Seat position";
        K("DefaultOffsetFromSteeringWheelX", "Seat left / right", " m", -0.5, 0.5, 0.005, 3, Conf::Med, "Eye position sideways from the default.", 0);
        K("DefaultOffsetFromSteeringWheelY", "Seat up / down", " m", -0.5, 0.5, 0.005, 3, Conf::Med, "Eye height from the default.", 0);
        K("DefaultOffsetFromSteeringWheelZ", "Seat forward / back", " m", -0.5, 0.5, 0.005, 3, Conf::Med, "Eye position along the car from the default.", 0);
        K("TripleScreenAdditionalZOffset", "Triple-screen shift", " m", -1, 1, 0.01, 2, Conf::Med,
          "Extra forward or back eye shift when playing on triple screens.", -0.2);
        K.group = "Split screen";
        K("SplitscreenDefaultOffsetFromSteeringWheelX", "Split screen left / right", " m", -0.5, 0.5, 0.005, 3, Conf::Med,
          "Seat offset in split screen only, which FH6 may not have.", 0);
        K("SplitscreenDefaultOffsetFromSteeringWheelY", "Split screen up / down", " m", -0.5, 0.5, 0.005, 3, Conf::Med, "Split screen only.", 0);
        K("SplitscreenDefaultOffsetFromSteeringWheelZ", "Split screen forward / back", " m", -0.5, 0.5, 0.005, 3, Conf::Med, "Split screen only.", 0);
        K.group = "Clipping";
        K("CamNearClip", "Near clip", " m", 0.01, 0.3, 0.005, 3, Conf::Med, "Kept low so the dashboard isn't cut.", 0.1);
        K("CamFarClip", "Far clip", " m", 1000, 100000, 100, 0, Conf::High, "Draw distance.", 50000);
        g.push_back({ "Cockpit", "Both first-person driver views. Effect layers: DriverCam.", -1, v });
    }
    {
        std::vector<KeyInfo> v;
        Builder K{ v, "HoodCam\\", "Lens" };
        K("FOV", "Field of view", DEG, 20, 120, 0.5, 1, Conf::High, "Base hood field of view, vertical degrees.", 72, K_FOV);
        K("MinFOV", "Narrowest field of view", DEG, 10, 150, 0.5, 1, Conf::Med, "Likely the bottom of the in-game FOV slider.", 40, K_FOV);
        K("MaxFOV", "Widest field of view", DEG, 10, 150, 0.5, 1, Conf::Med, "Likely the top of the in-game FOV slider.", 80, K_FOV);
        K.group = "Position";
        K("CamYOffset", "Height", " m", -0.5, 0.5, 0.005, 3, Conf::Med,
          "Eye height above the hood mount. In FH1 every car's own data replaced this, so it may do nothing.", 0, K_PERCAR);
        K("CamZOffset", "Forward / back", " m", -1, 1, 0.005, 3, Conf::Med, "Eye position along the hood. Likely replaced by per-car data.", 0, K_PERCAR);
        K("CamPitchOffset", "Pitch", " rad", -0.3, 0.3, 0.005, 3, Conf::Med,
          "Tilts the view (0.04 rad = 2.3\xC2\xB0). In FH1 negative looks down. Likely replaced by per-car data.", 0.04, K_PERCAR);
        K.group = "Clipping and see-through";
        K("CamNearClip", "Near clip", " m", 0.01, 0.5, 0.005, 3, Conf::Med, "Anything closer than this is not drawn.", 0.1);
        K("AutoGhostIntersectionAlpha", "See-through opacity", "", 0, 1, 0.01, 2, Conf::Med, "How visible objects that block the view stay.", 0);
        K("AutoGhostIntersectionAlphaDistance", "See-through distance", " m", 0, 10, 0.1, 1, Conf::Med, "Objects closer than this turn see-through.", 2.5);
        K.group = "Not in the game's file (untested)";
        K("CamFarClip", "Far clip", " m", 1000, 100000, 100, 0, Conf::Low, "Draw distance. Not in FH6's file for this camera.", 50000, 0, "", false);
        g.push_back({ "Hood", "The view from the bonnet. Effect layers: Hood.", -1, v });
    }
    {
        std::vector<KeyInfo> v;
        Builder K{ v, "BumperHighCam\\", "Lens" };
        K("FOV", "Field of view", DEG, 20, 120, 0.5, 1, Conf::High, "Base bumper field of view, vertical degrees.", 68, K_FOV);
        K("MinFOV", "Narrowest field of view", DEG, 10, 150, 0.5, 1, Conf::Med, "Likely the bottom of the in-game FOV slider.", 40, K_FOV);
        K("MaxFOV", "Widest field of view", DEG, 10, 150, 0.5, 1, Conf::Med, "Likely the top of the in-game FOV slider.", 80, K_FOV);
        K("FOVRatio", "FOV ratio", "", 0, 1, 0.01, 2, Conf::Low, "Unknown.", 0.5);
        K.group = "Position";
        K("RestHeightAboveGround", "Height", " m", 0.1, 2, 0.01, 2, Conf::High, "Eye height above the road.", 0.72);
        K("CamZOffset", "Forward / back", " m", -1.5, 1, 0.01, 2, Conf::High,
          "Position along the car from the front bumper. Applies to every car in FH1.", -0.22);
        K.group = "Clipping and see-through";
        K("CamNearClip", "Near clip", " m", 0.01, 0.5, 0.005, 3, Conf::Med, "Anything closer than this is not drawn.", 0.1);
        K("AutoGhostIntersectionAlpha", "See-through opacity", "", 0, 1, 0.01, 2, Conf::Med, "How visible objects that block the view stay.", 0);
        K("AutoGhostIntersectionAlphaDistance", "See-through distance", " m", 0, 10, 0.1, 1, Conf::Med, "Objects closer than this turn see-through.", 2.5);
        K.group = "Not in the game's file (untested)";
        K("CamFarClip", "Far clip", " m", 1000, 100000, 100, 0, Conf::Low, "Draw distance. Not in FH6's file for this camera.", 50000, 0, "", false);
        g.push_back({ "Bumper", "The low view from the front bumper. Effect layers: BumperHigh.", -1, v });
    }
    {
        std::vector<KeyInfo> v;
        Builder K{ v, "PhotoModeCam\\", "Range" };
        K("MaxHeight", "Max height", " m", 0.5, 50, 0.1, 1, Conf::High,
          "How far above the car the photo camera may rise. One FH6 mod reports a \"max camera height\" of 10,000, so test it.", 2);
        K("BoundaryCylinderRadius", "Roam radius", " m", 10, 500, 1, 0, Conf::High, "How far from the car the camera may go.", 100);
        K("BoundaryCeilingHeight", "Roam ceiling", " m", 2, 100, 1, 0, Conf::High, "Top of the roam area.", 30);
        K("BoundaryFloorDepth", "Roam floor depth", " m", 0, 100, 1, 0, Conf::Med, "How far below the car the roam area reaches.", 30);
        K("MinFOVPhotoMode", "Deepest zoom", DEG, 0.5, 20, 0.05, 2, Conf::High, "Narrowest field of view, so maximum zoom. Mods lower it to 1.", 2.25);
        K.group = "Start";
        K("StartingHeight", "Start height", " m", 0, 10, 0.05, 2, Conf::Med, "Camera height when photo mode opens.", 0.75);
        K("StartingDistance", "Start distance", " m", 1, 30, 0.1, 1, Conf::Med, "Camera distance when photo mode opens.", 5);
        K.group = "Collision";
        K("CollisionSphereRadius", "Collision size", " m", 0.05, 2, 0.01, 2, Conf::Med, "Size of the camera's collision ball.", 0.5);
        K("CollisionFixIterations", "Collision passes", "", 1, 20, 1, 0, Conf::Med, "Tries used to push the camera out of walls.", 5, K_INT);
        K("CollisionPullbackNormalExtension", "Collision push-out", " m", 0, 0.1, 0.001, 3, Conf::Med, "Extra push away from a hit surface.", 0.001);
        K("CarBoundingBoxHorizontalExpansion", "Car padding, sides", " m", 0, 3, 0.05, 2, Conf::Med, "Space around the car the camera can't enter.", 0.5);
        K("CarBoundingBoxVerticalExpansion", "Car padding, top", " m", 0, 3, 0.05, 2, Conf::Med, "Space above the car the camera can't enter.", 0.5);
        g.push_back({ "Photo mode", "The orbit camera in photo mode. No effect layers.", -1, v });
    }
    {
        std::vector<KeyInfo> v;
        Builder K{ v, "FreeCam\\", "Speed" };
        K("MoveForwardBackwardSpeed", "Forward speed", " m/s", 0, 500, 1, 0, Conf::Med, "Top speed forward and back at full stick.", 75);
        K("MoveLeftRightSpeed", "Sideways speed", " m/s", 0, 500, 1, 0, Conf::Med, "Top speed sideways.", 40);
        K("MoveUpDownSpeed", "Up / down speed", " m/s", 0, 500, 1, 0, Conf::Med, "Top speed up and down.", 20);
        K("YawSpeed", "Turn speed", " rad/s", 0, 10, 0.05, 2, Conf::Med, "Top turn rate at full stick.", 3);
        K("PitchSpeed", "Pitch speed", " rad/s", 0, 10, 0.05, 2, Conf::Med, "Top up and down look rate.", 1.5);
        K("RollSpeed", "Roll speed", " rad/s", 0, 3.14, 0.01, 3, Conf::Med, "Roll rate (0.785 = 45\xC2\xB0 per second).", 0.785398);
        K("FOVSpeed", "Zoom speed", " rad/s", 0, 3.14, 0.01, 3, Conf::Med, "Zoom rate (0.524 = 30\xC2\xB0 per second).", 0.523599);
        K.group = "Ease";
        K("TranslationalAccel", "Move ease-in", "", 0, 100, 0.5, 1, Conf::Low, "How fast movement speeds up.", 5);
        K("TranslationalDecel", "Move ease-out", "", 0, 100, 0.5, 1, Conf::Low, "How fast movement stops.", 25);
        K("RotationalAccel", "Turn ease-in", "", 0, 100, 0.5, 1, Conf::Low, "How fast turning speeds up.", 10);
        K("RotationalDecel", "Turn ease-out", "", 0, 100, 0.5, 1, Conf::Low, "How fast turning stops.", 20);
        K.group = "Lens";
        K("MinFOV", "Narrowest field of view", DEG, 0.1, 179, 0.1, 1, Conf::Med, "Free camera zoom limit.", 0.5, K_FOV);
        K("MaxFOV", "Widest field of view", DEG, 1, 179, 0.5, 1, Conf::Med, "Free camera wide limit.", 90, K_FOV);
        K("AutoGhostIntersectionAlpha", "See-through opacity", "", 0, 1, 0.01, 2, Conf::Med, "How visible objects the camera enters stay.", 0);
        K("AutoGhostIntersectionAlphaDistance", "See-through distance", " m", 0, 10, 0.1, 1, Conf::Med, "Objects closer than this turn see-through.", 2.5);
        g.push_back({ "Free cam", "The free-flying camera in photo mode and replays.", -1, v });
    }
    {
        std::vector<KeyInfo> v;
        Builder K{ v, "FollowCam\\SplitScreen\\", "Chase in split screen" };
        K("HeightRatio", "Height scale", "", 0, 3, 0.05, 2, Conf::Low, "Chase height scale in split screen. FH6 may not use it.", 1);
        K("RadiusRatio", "Distance scale", "", 0, 3, 0.05, 2, Conf::Low, "Chase distance scale in split screen.", 2);
        K("FOVRatio", "FOV scale", "", 0, 3, 0.05, 2, Conf::Low, "Chase FOV scale in split screen.", 0.5);
        K.prefix = "CTSCCamera\\";
        K.group = "Cinematic near clip";
        K("NearClipFOV0", "Telephoto FOV", DEG, 0.5, 90, 0.5, 1, Conf::Low, "FOV where the first near clip applies. Likely a replay or cinematic camera.", 5);
        K("NearClip0", "Telephoto near clip", " m", 0.01, 10, 0.05, 2, Conf::Low, "Near clip at that FOV.", 3);
        K("NearClipFOV1", "Wide FOV", DEG, 0.5, 90, 0.5, 1, Conf::Low, "FOV where the second near clip applies.", 20);
        K("NearClip1", "Wide near clip", " m", 0.01, 10, 0.05, 2, Conf::Low, "Near clip at that FOV.", 0.3);
        K.prefix = "TuneFX\\DebugDOF\\FocusFOV0\\";
        K.group = "Debug depth of field";
        K("FOV", "FOV", "", 0, 2, 0.05, 2, Conf::Low, "Debug depth of field preset. Probably unused in normal play.", 0.7);
        K("Dist0Def", "Near focus", " m", 0, 1000, 1, 0, Conf::Low, "Debug preset.", 20);
        K("Dist1Def", "Far focus", " m", 0, 1000, 1, 0, Conf::Low, "Debug preset.", 400);
        K("FocalLength0", "Focal length 1", " mm", 10, 2000, 10, 0, Conf::Low, "Debug preset.", 200);
        K("FocalLength1", "Focal length 2", " mm", 10, 2000, 10, 0, Conf::Low, "Debug preset.", 600);
        K("MaxBlurPixels0", "Blur cap 1", " px", 0, 50, 1, 0, Conf::Low, "Debug preset.", 10);
        K("MaxBlurPixels1", "Blur cap 2", " px", 0, 50, 1, 0, Conf::Low, "Debug preset.", 7);
        K.group = "Not in the game's file (untested)";
        K.prefix = "FollowCam\\";
        K("EnableCollision", "Chase collision", "", 0, 1, 1, 0, Conf::Low,
          "FH1 reads this (1 = on): switches the chase camera's wall sweep against a simple ground probe.", 1, K_INT, "", false);
        K.prefix = "WheelCam\\";
        K("WheelOffsetPosX", "Wheel cam X", " m", -2, 2, 0.01, 2, Conf::Low, "Forza Horizon 2's wheel camera. Probably gone.", 0.46, 0, "", false);
        K("WheelOffsetPosY", "Wheel cam Y", " m", -2, 2, 0.01, 2, Conf::Low, "FH2 wheel camera.", 0, 0, "", false);
        K("WheelOffsetPosZ", "Wheel cam Z", " m", -2, 2, 0.01, 2, Conf::Low, "FH2 wheel camera.", -0.88, 0, "", false);
        K("WheelOffsetTargetX", "Wheel cam aim X", " m", -2, 2, 0.01, 2, Conf::Low, "FH2 wheel camera.", 0.05, 0, "", false);
        K("WheelOffsetTargetY", "Wheel cam aim Y", " m", -2, 2, 0.01, 2, Conf::Low, "FH2 wheel camera.", -0.05, 0, "", false);
        K("WheelOffsetTargetZ", "Wheel cam aim Z", " m", -2, 2, 0.01, 2, Conf::Low, "FH2 wheel camera.", 0, 0, "", false);
        K("FOVDegrees", "Wheel cam FOV", DEG, 10, 120, 0.5, 1, Conf::Low, "FH2 wheel camera.", 45, 0, "", false);
        K("WheelId", "Wheel cam wheel", "", 0, 3, 1, 0, Conf::Low, "FH2 wheel camera.", 2, K_INT, "", false);
        g.push_back({ "Other", "Split screen, a cinematic camera and a debug preset. Mostly inert in normal play.", -1, v });
    }
    return g;
}
} // namespace

const std::vector<CameraGroup>& cameraGroups() {
    static const std::vector<CameraGroup> g = makeGroups();
    return g;
}

const KeyInfo* findKey(const std::string& full) {
    for (auto& g : cameraGroups())
        for (auto& k : g.keys)
            if (k.key == full) return &k;
    return nullptr;
}

std::string builtinIni() {
    // block order of the game's file
    static const char* order[] = { "CTSCCamera\\", "TuneFX\\", "FollowCam\\SplitScreen\\", "FreeCam\\", "PhotoModeCam\\", "FollowLowCam\\",
                                   "FollowHighCam\\", "CockpitCam\\", "HoodCam\\", "BumperHighCam\\", "Driver\\" };
    std::string out;
    for (const char* pre : order) {
        std::string block;
        for (auto& g : cameraGroups())
            for (auto& k : g.keys)
                if (k.listed && k.key.compare(0, std::string(pre).size(), pre) == 0) block += k.key + " " + fmtShort(k.def) + "\n";
        if (!block.empty()) out += (out.empty() ? "" : "\n") + block;
    }
    return out;
}

// ---------------------------------------------------------------------------
const std::vector<InputInfo>& inputs() {
    static const std::vector<InputInfo> v = {
        { "SpeedMPH", "Speed (mph)", "mph", "Car speed in miles per hour. Speed FOV, high-speed shake, pull-back.", 0, 240, E_SIM },
        { "Speed", "Speed (0 to 1)", "", "Speed scaled 0 to 1; what 1 means is unknown. The preview uses 240 mph.", 0, 1, E_SIM },
        { "Gs_Longitudinal", "G-force forward", "g", "Acceleration along the car: + speeding up, - braking.", -1, 1, E_SIM },
        { "Gs_Lateral", "G-force sideways", "g", "Cornering force. Which side is + is unknown; stock layers mirror it.", -1, 1, E_SIM },
        { "Slip_Lateral", "Tyre slip sideways", "", "Sideways tyre slip, signed. About 1 is the grip limit, higher is sliding. Drift effects use it.", -4, 4, E_SIM },
        { "Slip_Longitudinal", "Tyre slip along", "", "Wheelspin (+) or lock-up (-). Stock reads its size through Smooth magnitude.", -8, 0, E_SIM },
        { "Slip", "Tyre slip combined", "", "Combined tyre slip, about 1 at the grip limit.", -1, 1, E_SIM },
        { "SuspensionBias_Longitudinal", "Suspension front vs rear", "", "Front minus rear suspension compression, -1 to 1.", -1, 1, E_SIM },
        { "SuspensionBias_Lateral", "Suspension left vs right", "", "Left minus right suspension compression, -1 to 1.", -1, 1, E_SIM },
        { "SuspensionAngleBias_Longitudinal", "Body pitch", "\xC2\xB0", "Body pitch angle from the suspension, degrees.", -16, 16, E_SIM },
        { "SuspensionAngleBias_Lateral", "Body roll", "\xC2\xB0", "Body roll angle from the suspension, degrees.", -16, 16, E_SIM },
        { "PitchAngleDegrees", "Car pitch", "\xC2\xB0", "The car's pitch angle, degrees.", -16, 16, E_SIM },
        { "SurfaceBumpOffset_FL", "Road bump, front left", "", "Road surface bump at that wheel.", -1, 1, 0 },
        { "SurfaceBumpOffset_FR", "Road bump, front right", "", "Road surface bump at that wheel.", -1, 1, 0 },
        { "SurfaceBumpOffset_RL", "Road bump, rear left", "", "Road surface bump at that wheel.", 0, 4, 0 },
        { "SurfaceBumpOffset_RR", "Road bump, rear right", "", "Road surface bump at that wheel.", 0, 4, 0 },
        { "VerticalAcceleration", "Vertical acceleration", "m/s\xC2\xB2", "Up and down acceleration on any surface.", -21, 21, E_SIM },
        { "OnRoadVerticalAcceleration", "Vertical accel., road", "m/s\xC2\xB2", "Up and down acceleration on tarmac.", -40, 40, E_SIM },
        { "OffRoadVerticalAcceleration", "Vertical accel., off-road", "m/s\xC2\xB2", "Up and down acceleration off-road.", -60, 60, 0 },
        { "OneWhenStationary", "Stopped", "", "1 while the car stands still, else 0. Idle handheld sway.", -1, 1, E_SIM },
        { "TimeInAir", "Time in the air", "s", "Seconds since the wheels left the ground.", 0, 3.5, E_SIM },
        { "TimeInAirDescending", "Time falling", "s", "Seconds since the top of a jump. All stock layers are off.", 0, 1.5, 0 },
        { "VerticalSpeedInAir", "Vertical speed in the air", "m/s", "Up or down speed while airborne. All stock layers are off.", 0, 18, 0 },
        { "JumpYOffset", "Jump height", "m", "Height gained in a jump. All stock layers are off.", 0, 10, 0 },
        { "RecentLandingScore", "Landing", "", "How hard the last landing was, 0 to 1. Use with Smooth magnitude for a jolt that fades.", 0, 1, 0 },
        { "SuspensionLoad", "Suspension load", "", "Wheel or suspension load. All stock layers are off.", -1, 9, 0 },
        { "WorldCollision_Longitudinal", "Crash into world, along", "", "Impact strength against walls and objects along the car.", -15, 15, E_COLLISION },
        { "WorldCollision_Lateral", "Crash into world, sideways", "", "Impact strength against the world sideways.", -12, 12, E_COLLISION },
        { "WorldCollision", "Crash into world", "", "Impact size against the world.", -4, 4, E_COLLISION },
        { "CarCollision_Longitudinal", "Crash into car, along", "", "Impact strength against other cars along the car.", -15, 15, E_COLLISION },
        { "CarCollision_Lateral", "Crash into car, sideways", "", "Impact strength against other cars sideways.", -5, 5, E_COLLISION },
        { "CarCollision", "Crash into car", "", "Impact size against other cars.", -4, 4, E_COLLISION },
        { "WaterDrag", "Water", "", "Drag from driving through water, 0 to 100.", 0, 100, 0 },
        { "WindIntensity", "Wind", "", "Wind strength 0 to 2. Stock only enables the island copies.", 0, 2, 0 },
        { "ExternalSource", "Event shake", "", "Shake pushed by other game systems (events, scripted moments), -1 to 1.", -1, 1, 0 },
        { "HeadTrackingYaw", "Head tracking, turn", "\xC2\xB0", "Head tracker turn, degrees.", -25, 25, 0 },
        { "HeadTrackingX", "Head tracking, sideways", "m", "Head tracker sideways position.", -0.2, 0.2, 0 },
        { "HeadTrackingY", "Head tracking, up", "m", "Head tracker height.", -0.05, 0.15, 0 },
        { "ConstantOne", "Always 1", "", "Always 1: for fixed offsets and pure noise.", 0, 1, E_NOCLAMP | E_SIM },
        { "Constant", "Constant (mod-made)", "", "Only seen in hand-made mod layers. The game may not recognise it; use Always 1.", 0, 1,
          E_NOCLAMP | E_MODONLY | E_COLLISION },
        { "SpeedBoost", "Speed boost (FH4)", "", "Forza Horizon 4 only. Untested in FH6.", 0, 1, E_LEGACY },
        { "IsOffroad", "Off-road (FH4)", "", "Forza Horizon 4 only: 1 off-road. Untested in FH6.", 0, 1, E_LEGACY },
        { "TerrainAngleDiff", "Terrain angle (FH4)", "\xC2\xB0", "Forza Horizon 4 only. Untested in FH6.", -15, 0, E_LEGACY },
        { "CopterNoise", "Copter noise (FH4)", "", "Forza Horizon 4 only. Untested in FH6.", 0, 1, E_LEGACY },
        { "WindIntensityStormyOnly", "Storm wind (FH4)", "", "Forza Horizon 4 only. Untested in FH6.", 0, 1, E_LEGACY },
    };
    return v;
}

const std::vector<OutputInfo>& outputs() {
    static const std::vector<OutputInfo> v = {
        { "CarSpaceXYZOffset", "Move camera (car axes)", "Shifts the camera along the car's axes. Keeps the car centred.",
          { "Right", "Up", "Forward", "W (unused)" }, " m", 0 },
        { "CarSpaceYPROffset", "Rotate view (car axes)", "Turns the view around the car's axes. Yaw here moves the car off-centre.",
          { "Yaw", "Pitch", "Roll", "W (unused)" }, "\xC2\xB0", 0 },
        { "CameraSpaceXYZOffset", "Move camera (view axes)", "Shifts the camera along its own view axes.",
          { "Right", "Up", "Forward", "W (unused)" }, " m", 0 },
        { "CameraSpaceYPROffset", "Rotate view (view axes)", "Turns the view around its own axes. Where stock puts shake.",
          { "Yaw", "Pitch", "Roll", "W (unused)" }, "\xC2\xB0", 0 },
        { "FOV", "Field of view", "Adds degrees to the field of view.", { "Degrees", "Y (unknown)", nullptr, nullptr }, "\xC2\xB0", 0 },
        { "CarSpaceXYZImpulse", "Kick camera (car axes)", "A kick into a spring that bounces back. For impacts and landings.",
          { "Right", "Up", "Forward", "W (unused)" }, "", E_IMPULSE },
        { "CarSpaceYPRImpulse", "Kick view (car axes)", "A rotational kick that bounces back. Values act like a speed.",
          { "Yaw", "Pitch", "Roll", "W (unused)" }, "", E_IMPULSE },
        { "CameraSpaceYPRImpulse", "Kick view (view axes)", "A rotational kick in the view's own axes.",
          { "Yaw", "Pitch", "Roll", "W (unused)" }, "", E_IMPULSE },
        { "Vignette", "Vignette", "Darkens the picture edges. Stock: 1/240 per mph.", { "Strength", "Y (unknown)", "Z (unknown)", nullptr }, "", 0 },
        { "Tone", "Tone", "A tone or exposure flash on crashes. Components unknown.", { "X", "Y", "Z", nullptr }, "", 0 },
        { "SteeringWheelAngle", "Steering wheel", "Adds degrees to the cockpit steering wheel.", { "Degrees", nullptr, "Z (unknown)", nullptr }, "\xC2\xB0", 0 },
        { "CameraSpaceXYZImpulse", "Kick camera (view axes)", "Follows the naming pattern but never seen in a file. Untested.",
          { "Right", "Up", "Forward", "W (unused)" }, "", E_IMPULSE | E_LEGACY },
        { "MotionBlur", "Motion blur (FH4)", "Forza Horizon 4 only. Untested in FH6.", { "X", "Y", "Z", nullptr }, "", E_LEGACY },
    };
    return v;
}

const std::vector<ChoiceInfo>& noiseTypes() {
    static const std::vector<ChoiceInfo> v = {
        { "None", "None", "No noise.", 0 },
        { "Perlin", "Perlin", "Smooth wandering noise. Stock uses it for slow sway.", 0 },
        { "Simplex", "Simplex", "Smooth noise. Stock uses it for mid-frequency shake.", 0 },
        { "Sin", "Sine", "A regular wave.", 0 },
        { "HeightGrid", "Road grid", "Likely samples a road texture grid. Rumble from the surface.", 0 },
    };
    return v;
}

const std::vector<ChoiceInfo>& mappings() {
    static const std::vector<ChoiceInfo> v = {
        { "Linear", "Straight", "The clamped input as it is. Param is then per unit of input.", 0 },
        { "ThreePoint", "Curve, 3 points", "A curve through 3 points.", 0 },
        { "FivePoint", "Curve, 5 points", "A curve through 5 points.", 0 },
        { "ConstantOne", "Always 1", "Ignores the input: always 1.", 0 },
    };
    return v;
}

const std::vector<ChoiceInfo>& noiseMappings() {
    static const std::vector<ChoiceInfo> v = {
        { "ConstantOne", "Fixed", "Noise runs at its base frequency.", 0 },
        { "Linear", "Input", "Frequency times the input.", 0 },
        { "Abs", "Input size", "Frequency times the input's size.", 0 },
        { "ThreePoint", "Curve, 3 points", "Frequency times a curve of the input.", 0 },
        { "FivePoint", "Curve, 5 points", "Frequency times a curve of the input.", 0 },
    };
    return v;
}

const std::vector<ChoiceInfo>& locations() {
    static const std::vector<ChoiceInfo> v = {
        { "All", "Everywhere", "Runs everywhere.", 0 },
        { "MainlandOnly", "Mainland only", "Only on the main map. All stock mainland-only layers are off.", 0 },
        { "IslandOnly", "Island only", "Only on the island area. Stock uses it for wind; which area counts is unknown.", 0 },
    };
    return v;
}

const std::vector<SectionInfo>& sections() {
    static const std::vector<SectionInfo> v = {
        { "FollowCam", "Chase Near", 0 }, { "FollowCam2", "Chase Far", 1 }, { "DriverCam", "Cockpit", -1 },
        { "Hood", "Hood", -1 },           { "BumperHigh", "Bumper", -1 },   { "Copter", "Copter", -1 },
    };
    return v;
}

int inputIndex(const std::string& n) {
    auto& v = inputs();
    for (size_t i = 0; i < v.size(); i++) if (n == v[i].name) return (int)i;
    return -1;
}
int outputIndex(const std::string& n) {
    auto& v = outputs();
    for (size_t i = 0; i < v.size(); i++) if (n == v[i].name) return (int)i;
    return -1;
}
int choiceIndex(const std::vector<ChoiceInfo>& v, const std::string& n) {
    for (size_t i = 0; i < v.size(); i++) if (n == v[i].name) return (int)i;
    return -1;
}
const char* inputLabel(const std::string& n) {
    int i = inputIndex(n);
    if (i >= 0) return inputs()[i].label;
    static std::string tmp;
    tmp = n;
    return tmp.c_str();
}
const char* outputLabel(const std::string& n) {
    int i = outputIndex(n);
    if (i >= 0) return outputs()[i].label;
    static std::string tmp;
    tmp = n;
    return tmp.c_str();
}

} // namespace fh6
