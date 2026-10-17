#!/usr/bin/env python3
"""filer.py -- SYSTEM.FILER whose K(runch) knows the running operating system.

  filer.py        build/U134.4_OS_SOURCE_v1.14.BLK and SYSTEM.FILER (compiled on the P-System)

The Filer's K(runch) asked for a reboot ("Please re-boot", then it waits) when it
had moved a file named SYSTEM.PASCAL on the system disk: the system reads its
segments by their blocks (SYSCOM^.SEGTABLE), so they must not move under it.
With SYSTEM.C running, a K(runch) that moved SYSTEM.C went on, and the next
segment read was garbage.  Revision 1.14 of the OS and Filer source asks
for the reboot when the file moved holds the running system's segment 0
(SEGTABLE[0]: its unit and block), whatever its name.

The source volume is UCSD-Pascal-Volumes' U134.4_OS_SOURCE_v1.13.BLK with
FILER.E.TEXT changed so and VERSION.TEXT revision 1.14.  It is compiled with
BIGGY's Pascal compiler (Z80 mode, the prefix on the source volume: C(ompile
II.0.FILER).  First v1.13's II.0.FILER is compiled the same way: it must be
BIGGY's SYSTEM.FILER byte for byte (so the build is the one BIGGY's came from).
Uses UCSD-TinyC's build/run_verify and UCSD-Pascal-Volumes (both beside this
repository, or $TINYC, $VOLUMES).
"""
import os, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
VOLUMES = os.environ.get('VOLUMES', os.path.join(os.path.dirname(ROOT), 'UCSD-Pascal-Volumes'))
sys.path.insert(0, os.path.join(TINYC, 'tools'))
import ucsdvol

SOURCE = os.path.join(VOLUMES, 'BLK_format', 'U134.4_OS_SOURCE_v1.13.BLK')
BIGGY = os.path.join(VOLUMES, 'BLK_format', 'Big_Disk.BLK')
OUT_SOURCE = os.path.join(ROOT, 'build', 'U134.4_OS_SOURCE_v1.14.BLK')
OUT_FILER = os.path.join(ROOT, 'SYSTEM.FILER')

OLD = """          IF DTID = 'SYSTEM.PASCAL' THEN
            REBOOT := GVID = SYVID;        { Is this being done on root disk ? }
"""
NEW = """          WITH SYSCOM^.SEGTABLE[0] DO      { The running OS (SYSTEM.PASCAL or  }
            IF (CODEUNIT = GUNIT) AND      { SYSTEM.C): its segment 0 is here  }
               (CODEDESC.DISKADDR >= DFIRSTBLK) AND (CODEDESC.DISKADDR < DLASTBLK) THEN
              REBOOT := TRUE;
"""
COMMENTS = [("REBOOT := FALSE; { Will be set to true if *SYSTEM.PASCAL is moved }",
             "REBOOT := FALSE; { Will be set to true if the running OS is moved }"),
            ("BEGIN { *SYSTEM.PASCAL was moved }", "BEGIN { the running OS was moved }")]
VERSION_HEAD = ('U134.4 OS and Filer source, revision 1.13  (14-Oct-26)',
                'U134.4 OS and Filer source, revision 1.14  (17-Oct-26)')
VERSION_NOTE = """Since 1.13: FILER.E.TEXT KRUNCH asks for a reboot when it has moved the
running operating system, found by its segment 0's unit and block
(SYSCOM^.SEGTABLE[0]): SYSTEM.C as well as SYSTEM.PASCAL (it was: a file
named SYSTEM.PASCAL on the system disk).
"""


def source_volume():
    """build/U134.4_OS_SOURCE_v1.14.BLK"""
    os.makedirs(os.path.dirname(OUT_SOURCE), exist_ok=True)
    shutil.copy(SOURCE, OUT_SOURCE)
    v = ucsdvol.Volume(OUT_SOURCE)
    t = ucsdvol.ucsd_to_text(v.read('FILER.E.TEXT')[0])
    assert OLD in t, 'FILER.E.TEXT is not v1.13\'s'
    t = t.replace(OLD, NEW)
    for a, b in COMMENTS:
        assert a in t
        t = t.replace(a, b)
    v.write('FILER.E.TEXT', ucsdvol.text_to_ucsd(t), 3)
    t = ucsdvol.ucsd_to_text(v.read('VERSION.TEXT')[0])
    assert VERSION_HEAD[0] in t and 'Build:' in t
    t = t.replace(VERSION_HEAD[0], VERSION_HEAD[1]).replace('Build:', VERSION_NOTE + 'Build:', 1)
    v.write('VERSION.TEXT', ucsdvol.text_to_ucsd(t), 3)
    v.save()


def compile_filer(source, mode='z80'):
    """II.0.FILER of the source volume, compiled on BIGGY: the code file"""
    d = tempfile.mkdtemp(prefix='filer_')
    os.makedirs(os.path.join(d, 'data'))
    shutil.copy(os.path.join(TINYC, 'build', 'data', 'pascal.bin'), os.path.join(d, 'data'))
    shutil.copy(BIGGY, os.path.join(d, 'data', 'Big_Disk.BLK'))
    shutil.copy(source, os.path.join(d, 'SRC.BLK'))
    ucsdvol.main(['new', os.path.join(d, 'S.BLK'), 'S', '100'])
    open(os.path.join(d, 'c.script'), 'w').write(
        'WAIT "Command:"\nTYPE "F"\nWAIT "Filer:"\nTYPE "P"\nWAIT "Prefix"\nTYPE "#5:\\r"\nWAIT "Filer:"\n'
        'TYPE "Q"\nWAIT "Command:"\nTYPE "C"\nWAIT "Compile what text?"\nTYPE "II.0.FILER\\r"\n'
        'WAIT "To what codefile?"\nTYPE "FILER\\r"\nWAIT "lines"\nWAIT "Command:"\n')
    r = subprocess.run([os.path.join(TINYC, 'build', 'run_verify'), os.path.join(d, 'data'), os.path.join(d, 'SRC.BLK'),
                        os.path.join(d, 'S.BLK'), os.path.join(d, 'c.script'), mode, os.path.join(d, 'out'), '', '1800'],
                       capture_output=True, text=True)
    if 'VERIFY SCRIPT COMPLETED' not in r.stdout:
        raise SystemExit('the compile did not finish (%s):\n%s' % (d, r.stdout[-1500:]))
    return ucsdvol.Volume(os.path.join(d, 'out', 'VERIFY_SOURCE.BLK')).read('FILER.CODE')[0]


def main():
    biggy_filer = ucsdvol.Volume(BIGGY).read('SYSTEM.FILER')[0]
    old = compile_filer(SOURCE)
    if old != biggy_filer:
        print('v1.13\'s II.0.FILER compiled is not BIGGY\'s SYSTEM.FILER')
        return 1
    print('v1.13: II.0.FILER compiled is BIGGY\'s SYSTEM.FILER byte for byte')
    source_volume()
    new = compile_filer(OUT_SOURCE)
    open(OUT_FILER, 'wb').write(new)
    print('%s: v1.14 (%d bytes; %d bytes differ from v1.13\'s)' % (os.path.relpath(OUT_FILER, ROOT), len(new),
                                                                  sum(a != b for a, b in zip(new, old))))
    print('%s' % os.path.relpath(OUT_SOURCE, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
