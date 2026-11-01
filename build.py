"""Builds FH6 ChaseCam from the stock camera files.

    python3 build.py              # every preset
    python3 build.py drift        # one preset

Reads stock/CameraSettings.ini and stock/CameraPhysics.xml (copied from the
game), writes dist/<preset>/mediapc/camera/ plus a ready-to-share zip.
"""
import os, sys, zipfile
from fh6cam import SLOTS, edit_ini, edit_xml, section_layers
from presets import PRESETS

VER = '0.1.0'
ROOT = os.path.dirname(os.path.abspath(__file__))


def read(path):
    with open(path, 'rb') as f:
        return f.read().decode('latin-1')  # byte-exact round trip


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(text.encode('latin-1'))


def build(key, stock_dir, out_dir):
    pr = PRESETS[key]
    slot = SLOTS[pr['slot']]
    ini = read(os.path.join(stock_dir, 'CameraSettings.ini'))
    xml = read(os.path.join(stock_dir, 'CameraPhysics.xml'))
    ini, changes = edit_ini(ini, slot['ini'], pr.get('settings', {}))
    before = len(section_layers(xml, slot['xml']))
    xml, off = edit_xml(xml, slot['xml'], pr.get('layers', ()), pr.get('disable', ()))
    after = len(section_layers(xml, slot['xml']))

    cam = os.path.join(out_dir, key, 'mediapc', 'camera')
    write(os.path.join(cam, 'CameraSettings.ini'), ini)
    write(os.path.join(cam, 'CameraPhysics.xml'), xml)
    zpath = os.path.join(out_dir, 'fh6-chasecam-%s-v%s.zip' % (key, VER))
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in ('CameraSettings.ini', 'CameraPhysics.xml'):
            z.write(os.path.join(cam, f), 'mediapc/camera/' + f)
        z.writestr('README.txt', install_text(pr))

    print('%s (%s, replaces Chase %s)' % (key, pr['title'], pr['slot'].capitalize()))
    for k, old, new in changes:
        print('  %-40s %8.4g -> %.4g' % (k, old, new))
    print('  %s: %d -> %d layers (%d stock layers off)' % (slot['xml'], before, after, off))
    return zpath


def install_text(pr):
    return (
        '%s v%s by SPIRE (drift.ph)\r\n\r\n'
        'Replaces the Chase %s camera. Chase Near stays stock.\r\n\r\n'
        'INSTALL\r\n'
        '  Steam: copy the mediapc folder into the folder that has forzahorizon6.exe.\r\n'
        '  Xbox app / Game Pass: copy it into the game\'s Content folder,\r\n'
        '  for example D:\\XboxGames\\Forza Horizon 6\\Content. Allow it to merge.\r\n'
        '  Back up any mediapc\\camera files that are already there first.\r\n\r\n'
        'UNINSTALL\r\n'
        '  Delete mediapc\\camera\\CameraSettings.ini and CameraPhysics.xml\r\n'
        '  (or put your backups back).\r\n\r\n'
        'Game files are modified at your own risk. Playground does not support mods.\r\n'
    ) % (pr['title'], VER, pr['slot'].capitalize())


if __name__ == '__main__':
    stock = os.path.join(ROOT, 'stock')
    if not os.path.exists(os.path.join(stock, 'CameraPhysics.xml')):
        sys.exit('stock/CameraPhysics.xml and stock/CameraSettings.ini are missing: copy them from the game first')
    for k in sys.argv[1:] or list(PRESETS):
        build(k, stock, os.path.join(ROOT, 'dist'))
