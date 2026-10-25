#!/usr/bin/env python3
"""ospascal.py -- SYSTEM.PASCAL that makes a new NAME.BAS a text file.

  ospascal.py     build/U134.4_OS_SOURCE_v1.15.BLK and SYSTEM.PASCAL (compiled on the P-System)

SCANTITLE gives a new file its kind by the end of its name: .TEXT and .BACK
text, .CODE code, ...; any other a data file.  Revision 1.15 of the OS and
Filer source adds .BAS (BASIC sources, UCSD-BASIC's BC): a file the Editor,
the Filer or a program makes under such a name is a text file, as one named
.TEXT is (an old file is read by its kind already, whatever its name).  The
operating system in C (src/seg0.c scantitle) does the same.

The source volume is UCSD-Pascal-Volumes' U134.4_OS_SOURCE_v1.14.BLK with
SYSTEM.A.TEXT changed so and VERSION.TEXT revision 1.15.  It is compiled with
BIGGY's Pascal compiler (the prefix on the source volume: C(ompile SYSTEM to
OS -- named SYSTEM.PASCAL the compiler stops with String overflow).  First
v1.14's SYSTEM is compiled the same way: it must be BIGGY's SYSTEM.PASCAL
but for the bytes the compiler never sets (a block's end after a segment's
code, the pad byte that aligns a case table), which are 0xE5 in BIGGY's and
whatever was in memory in a compile (two compiles give the same bytes).  Uses UCSD-TinyC's build/run_verify and UCSD-Pascal-Volumes
(both beside this repository, or $TINYC, $VOLUMES).
"""
import os, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import filer
from filer import TINYC, VOLUMES, ucsdvol, text_lines, patch_text

SOURCE = os.path.join(VOLUMES, 'BLK_format', 'U134.4_OS_SOURCE_v1.14.BLK')
BIGGY = os.path.join(VOLUMES, 'BLK_format', 'Big_Disk.BLK')
OUT_SOURCE = os.path.join(ROOT, 'build', 'U134.4_OS_SOURCE_v1.15.BLK')
OUT_SYSTEM = os.path.join(ROOT, 'SYSTEM.PASCAL')

OLD = """                  IF FTITLE = '.FOTO' THEN FKIND := FOTOFILE
                END
"""
NEW = """                  IF FTITLE = '.FOTO' THEN FKIND := FOTOFILE
                END;
              IF OK AND (LENGTH(FTID) > 4) THEN     { a BASIC source is text }
                IF COPY(FTID,LENGTH(FTID)-3,4) = '.BAS' THEN FKIND := TEXTFILE
"""
VERSION_HEAD = ('U134.4 OS and Filer source, revision 1.14  (17-Oct-26)',
                'U134.4 OS and Filer source, revision 1.15  (25-Oct-26)')
VERSION_NOTE = """Since 1.14: SYSTEM.A.TEXT SCANTITLE makes a new file whose name ends
in .BAS (a BASIC source) a text file, as .TEXT and .BACK.
"""


def source_volume():
    """build/U134.4_OS_SOURCE_v1.15.BLK"""
    os.makedirs(os.path.dirname(OUT_SOURCE), exist_ok=True)
    shutil.copy(SOURCE, OUT_SOURCE)
    v = ucsdvol.Volume(OUT_SOURCE)
    v.write('SYSTEM.A.TEXT', patch_text(v.read('SYSTEM.A.TEXT')[0], OLD, NEW), 3)
    raw = v.read('VERSION.TEXT')[0]
    raw = patch_text(raw, VERSION_HEAD[0], VERSION_HEAD[1])
    build = next(t for t, _ in text_lines(raw)[1] if t.startswith('Build:'))
    v.write('VERSION.TEXT', patch_text(raw, build, VERSION_NOTE + build), 3)
    v.save()


def compile_system(source, mode='z80'):
    """SYSTEM of the source volume, compiled on BIGGY: the code file"""
    d = tempfile.mkdtemp(prefix='ospas_')
    os.makedirs(os.path.join(d, 'data'))
    shutil.copy(os.path.join(TINYC, 'build', 'data', 'pascal.bin'), os.path.join(d, 'data'))
    shutil.copy(BIGGY, os.path.join(d, 'data', 'Big_Disk.BLK'))
    shutil.copy(source, os.path.join(d, 'SRC.BLK'))
    ucsdvol.main(['new', os.path.join(d, 'S.BLK'), 'S', '100'])
    open(os.path.join(d, 'c.script'), 'w').write(
        'WAIT "Command:"\nTYPE "F"\nWAIT "Filer:"\nTYPE "P"\nWAIT "Prefix"\nTYPE "#5:\\r"\nWAIT "Filer:"\n'
        'TYPE "Q"\nWAIT "Command:"\nTYPE "C"\nWAIT "Compile what text?"\nTYPE "SYSTEM\\r"\n'
        'WAIT "To what codefile?"\nTYPE "OS\\r"\nWAIT "lines"\nWAIT "Command:"\n')
    r = subprocess.run([os.path.join(TINYC, 'build', 'run_verify'), os.path.join(d, 'data'), os.path.join(d, 'SRC.BLK'),
                        os.path.join(d, 'S.BLK'), os.path.join(d, 'c.script'), mode, os.path.join(d, 'out'), '', '3600'],
                       capture_output=True, text=True)
    if 'VERIFY SCRIPT COMPLETED' not in r.stdout:
        raise SystemExit('the compile did not finish (%s):\n%s' % (d, r.stdout[-1500:]))
    return ucsdvol.Volume(os.path.join(d, 'out', 'VERIFY_SOURCE.BLK')).read('OS.CODE')[0]


def main(a):
    mode = a[0] if a else 'z80'
    biggy = ucsdvol.Volume(BIGGY).read('SYSTEM.PASCAL')[0]
    old = compile_system(SOURCE, mode)
    diff = [i for i in range(len(biggy)) if i >= len(old) or old[i] != biggy[i]]
    if len(old) != len(biggy) or any(biggy[i] != 0xE5 for i in diff):
        print('v1.14\'s SYSTEM compiled (%s) is not BIGGY\'s SYSTEM.PASCAL (%d bytes, %d)' % (mode, len(old), len(biggy)))
        return 1
    print('v1.14: SYSTEM compiled is BIGGY\'s SYSTEM.PASCAL but for %d bytes the compiler never sets' % len(diff))
    source_volume()
    new = compile_system(OUT_SOURCE, mode)
    open(OUT_SYSTEM, 'wb').write(new)
    print('%s: v1.15 (%d bytes, was %d)' % (os.path.relpath(OUT_SYSTEM, ROOT), len(new), len(old)))
    print('%s' % os.path.relpath(OUT_SOURCE, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
