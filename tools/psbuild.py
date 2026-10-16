#!/usr/bin/env python3
"""psbuild.py -- build the operating system ON the P-System with Tiny-C's CC,
running on the operating system in C itself, and compare it with SYSTEM.C
that build.py made on the host (the two must be the same, byte for byte, as
Tiny-C's own builds are).

The sources go onto the work disk (#5) as SYSC.C, SEG0.C, SEGS.C and
GLOBALS.H (as on SYS-C:), with CC.CODE (the host's build of Tiny-C's modules),
its messages and library; then X(ecute CC, SYSC: SYSC.CODE on #5, the same as
SYSTEM.C.  Uses
PSYS_MODE (native or z80) and UCSD-TinyC (../ucsd-tinyc, or $TINYC).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
sys.path.insert(0, os.path.join(TINYC, 'tools'))
from psys import PSystem
from selfhost import put_headers
from buildtc import build
sys.path.insert(0, HERE)
import suite


def main():
    code, log = build()
    ps = PSystem(blocks=3000)
    ps.put('CC.CODE', open(code, 'rb').read())
    put_headers(ps)
    src = os.path.join(ROOT, 'src')
    ps.put('SYSC.C', open(os.path.join(src, 'sysc.c')).read())
    ps.put('SEG0.C', open(os.path.join(src, 'seg0.c')).read())
    ps.put('SEGS.C', open(os.path.join(src, 'segs.c')).read())
    ps.put('GLOBALS.H', open(os.path.join(src, 'globals.h')).read())
    script = ['WAIT "Command:"', 'TYPE "F"', 'WAIT "Filer:"', 'TYPE "P"', 'WAIT "Prefix"',
              'TYPE "#5:\\r"', 'WAIT "Filer:"', 'TYPE "Q"', 'WAIT "Command:"',
              'TYPE "X"', 'WAIT "Execute what file?"', 'TYPE "CC\\r"',
              'WAIT "Compile what file?"', 'TYPE "SYSC\\r"', 'WAIT "Command:"']
    mode = os.environ.get('PSYS_MODE', 'native')
    saved = suite.install(open(os.path.join(ROOT, 'SYSTEM.C'), 'rb').read())   # (on the C one)
    try:
        ok, tr, info = ps.run_script('\n'.join(script) + '\n', 3600 if mode == 'z80' else 600)
    finally:
        suite.restore(saved)
    lines = [l for l in tr.replace('\r', '\n').split('\n') if l.strip()]
    i = max([k for k, l in enumerate(lines) if 'Tiny-C' in l] or [0])
    print('\n'.join(lines[i:]))
    try:
        got = ps.get('SYSC.CODE')
    except (Exception, SystemExit) as e:
        got = b''
        print('no SYSC.CODE: %s' % e)
    want = open(os.path.join(ROOT, 'SYSTEM.C'), 'rb').read()
    same = ok and got[:len(want)] == want and not any(got[len(want):])
    print('built on the P-System (%s): %s' % (mode, 'IDENTICAL to SYSTEM.C' if same else 'DIFFERENT (%d bytes, %d)' % (len(got), len(want))))
    return 0 if same else 1


if __name__ == '__main__':
    sys.exit(main())
