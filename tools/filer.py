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
BIGGY's SYSTEM.FILER up to 1.27 byte for byte (that of the reference disk,
UCSD-TinyC's build/img/Big_Disk.BLK: so the build is the one it came from).
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
REFERENCE = os.path.join(TINYC, 'build', 'img', 'Big_Disk.BLK')     # (its SYSTEM.FILER is v1.13's)
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


def text_lines(raw):
    """a UCSD text file's lines: [(text, their bytes as in the file)], and its header (2 blocks)"""
    lines, body, i, start = [], raw[1024:], 0, 0
    while i < len(body):
        if body[i] == 13:
            b = body[start:i + 1]
            lines.append((ucsdvol.ucsd_to_text(bytes(1024) + b)[:-1], b))
            start = i + 1
        elif body[i] == 0 and start == i:
            start = i + 1                                # the end of a page
        elif body[i] == 16:
            i += 1
        i += 1
    return raw[:1024], lines


def encode_line(t):
    n = len(t) - len(t.lstrip(' '))
    return (bytes([16, 32 + n]) if n else b'') + t[n:].encode('latin1') + b'\r'


def patch_text(raw, old, new):
    """raw (a text file) with the lines of old replaced by those of new; every other line,
    and the header, as it was (the pages packed again: 1023 bytes at most)"""
    head, lines = text_lines(raw)
    texts = [t for t, b in lines]
    o, n = old.rstrip('\n').split('\n'), new.rstrip('\n').split('\n')
    k = next(i for i in range(len(texts)) if texts[i:i + len(o)] == o)
    lines[k:k + len(o)] = [(t, encode_line(t)) for t in n]
    out, page = bytearray(head), bytearray()
    for t, b in lines:
        if len(page) + len(b) > 1023:
            out += page + bytes(1024 - len(page))
            page = bytearray()
        page += b
    out += page + bytes(1024 - len(page))
    return bytes(out)


def source_volume():
    """build/U134.4_OS_SOURCE_v1.14.BLK"""
    os.makedirs(os.path.dirname(OUT_SOURCE), exist_ok=True)
    shutil.copy(SOURCE, OUT_SOURCE)
    v = ucsdvol.Volume(OUT_SOURCE)
    raw = patch_text(v.read('FILER.E.TEXT')[0], OLD, NEW)
    for a, b in COMMENTS:
        line = next(t for t, _ in text_lines(raw)[1] if a in t)
        raw = patch_text(raw, line, line.replace(a, b))
    v.write('FILER.E.TEXT', raw, 3)
    raw = v.read('VERSION.TEXT')[0]
    raw = patch_text(raw, VERSION_HEAD[0], VERSION_HEAD[1])
    build = next(t for t, _ in text_lines(raw)[1] if t.startswith('Build:'))
    v.write('VERSION.TEXT', patch_text(raw, build, VERSION_NOTE + build), 3)
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
    biggy_filer = ucsdvol.Volume(REFERENCE).read('SYSTEM.FILER')[0]
    old = compile_filer(SOURCE)
    if old != biggy_filer:
        print('v1.13\'s II.0.FILER compiled is not the reference disk\'s SYSTEM.FILER')
        return 1
    print('v1.13: II.0.FILER compiled is the reference disk\'s (BIGGY 1.27\'s) SYSTEM.FILER byte for byte')
    source_volume()
    new = compile_filer(OUT_SOURCE)
    open(OUT_FILER, 'wb').write(new)
    print('%s: v1.14 (%d bytes; %d bytes differ from v1.13\'s)' % (os.path.relpath(OUT_FILER, ROOT), len(new),
                                                                  sum(a != b for a, b in zip(new, old))))
    print('%s' % os.path.relpath(OUT_SOURCE, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
