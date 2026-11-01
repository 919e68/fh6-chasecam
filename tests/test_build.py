"""Tests for the generator. Run: python3 tests/test_build.py

Uses a small hand-written fixture in the same format as the game files, so it
runs without the game. If stock/ has the real files, the build is checked
against those too.
"""
import os, sys, re, tempfile, zipfile
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
import build
from fh6cam import SLOTS, edit_ini, edit_xml, read_ini, section_layers, rel, scale, layer
from presets import PRESETS

FIX = os.path.join(ROOT, 'tests', 'fixture')


def sections(xml):
    return re.findall(r'<(\w+)\s+NumLayers="(\d+)"', xml)


def check_build(stock):
    sx = build.read(os.path.join(stock, 'CameraPhysics.xml'))
    si = build.read(os.path.join(stock, 'CameraSettings.ini'))
    with tempfile.TemporaryDirectory() as out:
        for key, pr in PRESETS.items():
            z = build.build(key, stock, out)
            cam = os.path.join(out, key, 'mediapc', 'camera')
            x = build.read(os.path.join(cam, 'CameraPhysics.xml'))
            i = build.read(os.path.join(cam, 'CameraSettings.ini'))
            slot = SLOTS[pr['slot']]

            # well-formed; the camera we edit declares its real layer count
            # (other cameras keep whatever the original file says)
            ET.fromstring(x.encode('latin-1'))
            num = dict(sections(x))[slot['xml']]
            assert int(num) == len(section_layers(x, slot['xml'])), '%s NumLayers wrong' % slot['xml']

            # other cameras untouched, byte for byte
            for name, _ in sections(sx):
                if name != slot['xml']:
                    a = re.search(r'<%s\b.*?</%s>' % (name, name), sx, re.S).group(0)
                    b = re.search(r'<%s\b.*?</%s>' % (name, name), x, re.S).group(0)
                    assert a == b, '%s changed' % name

            # our camera: stock layers kept in order, new ones appended
            old, new = section_layers(sx, slot['xml']), section_layers(x, slot['xml'])
            assert len(new) == len(old) + len(pr.get('layers', ()))
            for a, b in zip(old, new):
                if b != a:  # only allowed difference: switched off
                    assert a['Enabled'] == '1' and b['Enabled'] == '0' and dict(a, Enabled='0') == b

            # ini: exactly the preset's keys changed, everything else identical
            ki, ko = read_ini(si), read_ini(i)
            assert set(ki) == set(ko)
            changed = {k for k in ki if ki[k] != ko[k]}
            want = {slot['ini'] + '\\' + k for k, v in pr.get('settings', {}).items()
                    if abs(float(ki[slot['ini'] + '\\' + k]) - (v.apply(float(ki[slot['ini'] + '\\' + k])) if hasattr(v, 'apply') else v)) > 1e-9}
            assert changed == want, (changed, want)
            assert len(si.splitlines()) == len(i.splitlines())

            with zipfile.ZipFile(z) as zf:
                assert sorted(zf.namelist()) == ['README.txt', 'mediapc/camera/CameraPhysics.xml', 'mediapc/camera/CameraSettings.ini']
                assert zf.read('mediapc/camera/CameraPhysics.xml') == x.encode('latin-1')


def test_units():
    # relative, scaled and absolute values
    t = 'A\\X 1.5\nA\\Y 8.5\nA\\Z 60\nB\\X 3\n'
    out, ch = edit_ini(t, 'A', {'X': rel(-0.7), 'Y': scale(0.8), 'Z': 66})
    assert out == 'A\\X 0.8\nA\\Y 6.8\nA\\Z 66\nB\\X 3\n', out
    try:
        edit_ini(t, 'A', {'Nope': 1})
        raise AssertionError('missing key should fail')
    except KeyError:
        pass
    # Windows line endings survive, and the new layer uses them too
    x = '<CameraPhysics>\r\n  <FollowCam2 NumLayers="1">\r\n  <Layer Enabled="1" InputType="Gs_Lateral" OutputType="FOV" />\r\n  </FollowCam2>\r\n</CameraPhysics>\r\n'
    nx, off = edit_xml(x, 'FollowCam2', [layer('SpeedMPH', 'FOV', (1, 0, 0), (0, 1))], [('Gs_Lateral', 'FOV')])
    assert off == 1 and 'NumLayers="2"' in nx and '\n' not in nx.replace('\r\n', '')
    assert nx.count('Enabled="0"') == 1 and nx.endswith('  </FollowCam2>\r\n</CameraPhysics>\r\n')
    ET.fromstring(nx)
    # layer shape matches Playground's attribute order
    a = re.findall(r'(\w[\w.]*)=', layer('Slip_Lateral', 'CameraSpaceYPROffset', (5, 0, 0), (-8, 8), [(1, 0), (2, 0.5), (6, 1)], mirrored=True, spring=(10, 1)))
    assert a[:6] == ['Enabled', 'LocationType', 'InputType', 'NoiseType', 'OutputType', 'InputTakeDerivative'] and a[-3:] == ['Sprung', 'SpringK', 'SpringDPercent']


test_units()
check_build(FIX)
print('fixture build OK')
if os.path.exists(os.path.join(ROOT, 'stock', 'CameraPhysics.xml')):
    check_build(os.path.join(ROOT, 'stock'))
    print('stock build OK')
else:
    print('(stock/ is empty: copy the game files there to test the real build)')
print('ALL TESTS PASSED')
