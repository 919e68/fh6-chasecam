"""Edits Forza Horizon 6 camera files.

FH6 reads two text files from mediapc/camera next to the game exe:

  CameraSettings.ini  one "Section\\Key value" per line: position, FOV and the
                      springs that make the chase cameras follow the car.
  CameraPhysics.xml   per camera, a list of effect layers. Each layer reads one
                      input (speed, G-forces, tyre slip, bumps ...), maps it
                      through a curve and adds the result to one output
                      (an offset, a rotation, the FOV ...).

Everything here edits the original text in place, so every line we do not
change stays byte-for-byte the same as Playground's file.
"""
import re

# Which ini section and which xml section belong to each chase camera.
SLOTS = {
    'near': {'ini': 'FollowLowCam', 'xml': 'FollowCam'},   # Chase Near
    'far': {'ini': 'FollowHighCam', 'xml': 'FollowCam2'},  # Chase Far
}


# ---------------------------------------------------------------------------
# Values in presets: a plain number replaces the stock value, rel() adds to it,
# scale() multiplies it. Relative values keep presets valid across game updates.
# ---------------------------------------------------------------------------
class rel:
    def __init__(self, d): self.d = d
    def apply(self, v): return v + self.d
    def __repr__(self): return '%+g' % self.d


class scale:
    def __init__(self, f): self.f = f
    def apply(self, v): return v * self.f
    def __repr__(self): return 'x%g' % self.f


def resolve(stock, value):
    return value.apply(stock) if hasattr(value, 'apply') else float(value)


def fmt(v):
    """Short number for the ini: 4 decimals at most, no trailing zeros."""
    s = ('%.4f' % v).rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


# ---------------------------------------------------------------------------
# CameraSettings.ini
# ---------------------------------------------------------------------------
def read_ini(text):
    """{ 'Section\\Key': value string } for every setting line."""
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if line and ' ' in line and not line.startswith((';', '#')):
            k, v = line.split(' ', 1)
            out[k] = v.strip()
    return out


def edit_ini(text, section, values):
    """Sets section\\key for each key in values. Returns (new text, changes).
    A key the file does not have is an error: we never invent settings."""
    changes = []
    for key, value in values.items():
        full = section + '\\' + key
        pat = re.compile(r'^([ \t]*' + re.escape(full) + r'[ \t]+)(\S+)', re.M)
        m = pat.search(text)
        if not m:
            raise KeyError('%s not found in CameraSettings.ini' % full)
        old = float(m.group(2))
        new = resolve(old, value)
        text = text[:m.start(2)] + fmt(new) + text[m.end(2):]
        changes.append((full, old, new))
    return text, changes


# ---------------------------------------------------------------------------
# CameraPhysics.xml
# ---------------------------------------------------------------------------
def n(v): return '%.6f' % v


def layer(inp, out, param, clamp, curve=None, mirrored=False, spring=None, enabled=True):
    """One effect layer, written the way Playground writes them.

    inp      input, e.g. 'Slip_Lateral', 'SpeedMPH', 'Gs_Lateral'
    out      output, e.g. 'CameraSpaceYPROffset' (turn the view: yaw, pitch,
             roll in degrees), 'CarSpaceYPROffset' (swing the camera around the
             car), 'CarSpaceXYZOffset' (move it, metres), 'FOV' (degrees in x)
    param    (x, y, z) multiplied by the curve output
    clamp    (min, max) the input is clamped to first
    curve    3 or 5 (input, output) points; None = linear (output = input)
    mirrored the curve is drawn for positive input and mirrored for negative
             input, so left and right drifts act the same way
    spring   (stiffness, damping) smooths the result; None = no smoothing
    """
    a = [('Enabled', '1' if enabled else '0'), ('LocationType', 'All'), ('InputType', inp),
         ('NoiseType', 'None'), ('OutputType', out), ('InputTakeDerivative', '0'),
         ('InputClampMin', n(clamp[0])), ('InputClampMax', n(clamp[1])), ('InputMagAttackDecay', '0')]
    if curve is None:
        a.append(('InputToParamMapping', 'Linear'))
    else:
        if len(curve) not in (3, 5):
            raise ValueError('curve needs 3 or 5 points')
        a.append(('InputToParamMapping', 'ThreePoint' if len(curve) == 3 else 'FivePoint'))
        for i, (x, y) in enumerate(curve):
            a += [('InputToParamCurveInput%d' % i, n(x)), ('InputToParamCurveOutput%d' % i, n(y))]
        a.append(('InputToParamCurveMirrored', '1' if mirrored else '0'))
    p = tuple(param) + (0,) * (4 - len(param))
    a += [('Param.x', n(p[0])), ('Param.y', n(p[1])), ('Param.z', n(p[2])), ('Param.w', n(p[3]))]
    if spring:
        a += [('Sprung', '1'), ('SpringK', n(spring[0])), ('SpringDPercent', n(spring[1]))]
    else:
        a.append(('Sprung', '0'))
    return '<Layer ' + ' '.join('%s="%s"' % kv for kv in a) + ' />'


LAYER_RE = re.compile(r'<Layer\b[^>]*/>')


def attrs(layer_text):
    return dict(re.findall(r'([\w.]+)="([^"]*)"', layer_text))


def section_span(text, name):
    """(start of the opening tag, start of the body, start of the closing tag)."""
    m = re.search(r'<' + re.escape(name) + r'\s+NumLayers="(\d+)"\s*>', text)
    if not m:
        raise KeyError('<%s> not found in CameraPhysics.xml' % name)
    end = text.find('</' + name + '>', m.end())
    if end < 0:
        raise KeyError('</%s> not found in CameraPhysics.xml' % name)
    return m.start(), m.end(), end


def section_layers(text, name):
    _, body, end = section_span(text, name)
    return [attrs(t) for t in LAYER_RE.findall(text[body:end])]


def edit_xml(text, name, add=(), disable=()):
    """Appends layers to a camera section and switches off stock layers.

    disable  (InputType, OutputType) pairs: every enabled stock layer in the
             section with that input and output is switched off.
    Returns (new text, number of layers switched off).
    """
    start, body, end = section_span(text, name)
    nl = '\r\n' if '\r\n' in text else '\n'
    seg = text[body:end]
    off = 0
    for inp, out in disable:
        def kill(m):
            nonlocal off
            a = attrs(m.group(0))
            if a.get('InputType') == inp and a.get('OutputType') == out and a.get('Enabled') == '1':
                off += 1
                return m.group(0).replace('Enabled="1"', 'Enabled="0"', 1)
            return m.group(0)
        seg = LAYER_RE.sub(kill, seg)
    if add:
        # same indentation as the section's own layers
        ind = re.search(r'\n([ \t]*)<Layer', text[start:end])
        ind = ind.group(1) if ind else '  '
        tail = len(seg) - len(seg.rstrip(' \t'))  # indentation before the closing tag
        core = seg[:len(seg) - tail]
        if not core.endswith(nl):
            core += nl
        seg = core + ''.join(ind + l + nl for l in add) + seg[len(seg) - tail:]
    count = len(LAYER_RE.findall(seg))
    head = re.sub(r'NumLayers="\d+"', 'NumLayers="%d"' % count, text[start:body], count=1)
    return text[:start] + head + seg + text[end:], off
