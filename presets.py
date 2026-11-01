"""FH6 ChaseCam presets.

slot      which in-game chase camera the preset replaces: 'far' (Chase Far,
          default, so stock Chase Near stays available) or 'near'
settings  CameraSettings.ini values for that camera. A number replaces the
          stock value, rel(x) adds to it, scale(x) multiplies it.
layers    effect layers added to that camera in CameraPhysics.xml
disable   stock (InputType, OutputType) layers to switch off for that camera
"""
from fh6cam import layer, rel, scale

# Directions of the drift effects. Confirm with the "calibrate" build: in a
# LEFT-hand drift the view should turn left (into the corner) and the horizon
# should tip toward the outside. Flip a sign here if the game shows the opposite.
AIM_SIGN = 1
TILT_SIGN = 1

# Drift input: lateral tyre slip. About 1 is the grip limit, more is sliding.
# Signed, so one mirrored curve covers left and right drifts.
DRIFT_CLAMP = (-8, 8)
DRIFT_CURVE = [(1.0, 0.0), (2.5, 0.6), (6.0, 1.0)]


def corner_aim(deg, spring=(10, 1)):
    """Turns the view toward the inside of the corner while drifting, so the
    car sits toward the outside of the frame and the corner opens up."""
    return layer('Slip_Lateral', 'CameraSpaceYPROffset', (AIM_SIGN * deg, 0, 0),
                 DRIFT_CLAMP, DRIFT_CURVE, mirrored=True, spring=spring)


def drift_tilt(deg, spring=(8, 1)):
    """Tips the horizon while drifting (positive: toward the outside)."""
    return layer('Slip_Lateral', 'CameraSpaceYPROffset', (0, 0, TILT_SIGN * deg),
                 DRIFT_CLAMP, DRIFT_CURVE, mirrored=True, spring=spring)


def speed_fov(deg, spring=(20, 1)):
    """Widens the view with speed (mph), on top of the stock speed FOV."""
    return layer('SpeedMPH', 'FOV', (deg, 0, 0), (0, 200),
                 [(40, 0), (70, 0.35), (110, 0.7), (150, 0.9), (200, 1)], spring=spring)


PRESETS = {
    # The look from the reference clip: low (about roof height), a bit closer,
    # wider, looks into the corner in a drift, slight outward tilt.
    'drift': {
        'title': 'FH6 ChaseCam Drift',
        'slot': 'far',
        'settings': {
            'HeightOffsetGlobal': rel(-0.7),
            'HeightMinAboveGround': rel(-0.7),
            'HeightMaxAboveGround': rel(-0.7),
            'TargetYOffsetGlobal': rel(-0.55),  # less than the camera: flatter, more horizon
            'RadiusOffsetGlobal': rel(-0.5),
            'FOV': rel(6),
            'MaxFOV': rel(10),
            'Spring\\YRotSpringK': scale(0.8),  # follows the car's rotation a little more loosely
        },
        'layers': [corner_aim(6), drift_tilt(3), speed_fov(6)],
    },

    # Test build: only big drift effects on Chase Far, nothing else changed.
    # Drift left on Chase Far and note which way the view turns and the horizon tips.
    'calibrate': {
        'title': 'FH6 ChaseCam Calibration',
        'slot': 'far',
        'settings': {},
        'layers': [corner_aim(20), drift_tilt(15)],
    },
}
