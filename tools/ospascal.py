#!/usr/bin/env python3
"""ospascal.py -- SYSTEM.PASCAL that gives each file the time of day it was written.

  ospascal.py     build/U134.4_OS_SOURCE_v1.16.BLK and SYSTEM.PASCAL (compiled on the P-System)

Revision 1.16 of the OS source: FCLOSE, when it writes a file's directory entry,
gives it the time of day as well as the date -- seconds since midnight + 1
(1..86400, 0: none), its low 11 bits in DFKIND's word (bits 4..14, FILLER2,
now 0..2047) and its high 6 in DLASTBYTE's (bits 10..15, the new DTIMEHI: the
PACKED field DLASTBYTE needs only 10).  The emulator (2.13) gives the time and
today's date through TIME: both variables 25611 -- the time's two parts; both
25612 -- today's DATEREC (0 when its clock is off), which THEDATE follows (past
midnight too).  As the date: a new file or one written gets now, one not
changed keeps its time, the Filer's special case (a copy keeping its date)
gets none (the Filer fills a new entry field by field: those bits are not its).  The operating system in C (src/seg0.c fclose) does the
same.  (1.15: a new NAME.BAS is a text file.)

The source volume is UCSD-Pascal-Volumes' U134.4_OS_SOURCE_v1.15.BLK with
GLOBALS.TEXT and SYSTEM.B.TEXT changed so and VERSION.TEXT revision 1.16.  It
is compiled with BIGGY's Pascal compiler (the prefix on the source volume:
C(ompile SYSTEM to OS -- named SYSTEM.PASCAL the compiler stops with String
overflow).  First v1.15's SYSTEM is compiled the same way: it must be BIGGY's
SYSTEM.PASCAL but for the bytes the compiler never sets (a block's end after a
segment's code, the pad byte that aligns a case table), which are 0xE5 in a
disk's copy and whatever was in memory in a compile (a block's last bytes, so
two compiles differ there).  Uses UCSD-TinyC's build/run_verify and UCSD-Pascal-Volumes (both
beside this repository, or $TINYC, $VOLUMES).
"""
import os, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import filer
from filer import TINYC, VOLUMES, ucsdvol, text_lines, patch_text

SOURCE = os.path.join(VOLUMES, 'BLK_format', 'U134.4_OS_SOURCE_v1.15.BLK')
BIGGY = os.path.join(VOLUMES, 'BLK_format', 'Big_Disk.BLK')
OUT_SOURCE = os.path.join(ROOT, 'build', 'U134.4_OS_SOURCE_v1.16.BLK')
OUT_SYSTEM = os.path.join(ROOT, 'SYSTEM.PASCAL')

PATCHES = [
    ('GLOBALS.TEXT',
     """                       (FILLER2 : 0..1024; {for downward compatibility}
""",
     """                       (FILLER2 : 0..2047; {the time's low 11 bits (1.16)}
"""),
    ('GLOBALS.TEXT',
     """                        DLASTBYTE: 1..FBLKSIZE; (*NUM BYTES IN LAST BLOCK*)
""",
     """                        DLASTBYTE: 1..FBLKSIZE; (*NUM BYTES IN LAST BLOCK*)
                        DTIMEHI: 0..63;         {the time's high 6 bits (1.16):
                                                 seconds since midnight + 1}
"""),
    ('SYSTEM.B.TEXT',
     """  VAR LINX,DUPINX: DIRRANGE; LDIR: DIRP; FOUND: BOOLEAN;
BEGIN SYSCOM^.IORSLT := INOERROR;
""",
     """  VAR LINX,DUPINX: DIRRANGE; LDIR: DIRP; FOUND: BOOLEAN;
      TLO,THI,NLO,NHI: INTEGER;           {the entry's time, now's (1.16)}
      TODAY: RECORD CASE BOOLEAN OF
               TRUE: (I: INTEGER);
               FALSE: (D: DATEREC)
             END;
BEGIN SYSCOM^.IORSLT := INOERROR;
"""),
    ('SYSTEM.B.TEXT',
     """                        IF LDIR^[LINX].DACCESS.YEAR = 100 THEN
                          IF DACCESS.YEAR = 100 THEN
                            DACCESS := THEDATE
                          ELSE (*LEAVE ALONE...FILER SPECIAL CASE*)
                        ELSE
                          IF FMODIFIED AND (THEDATE.MONTH <> 0) THEN
                            DACCESS := THEDATE
                          ELSE
                            DACCESS := LDIR^[LINX].DACCESS;
                        DLASTBLK := DFIRSTBLK+FMAXBLK;
                        IF FSOFTBUF THEN DLASTBYTE := FMAXBYTE;
                        FHEADER.FILLER1 := 0;  {This had better work, Steve}
""",
     """                        {1.16: today and the time from the PC (TIME's answers,
                         emulator 2.13; 0 when its clock is off)}
                        TODAY.I := 25612; NHI := 25612; TIME(TODAY.I,NHI);
                        IF TODAY.I <> 0 THEN THEDATE := TODAY.D;
                        NLO := 25611; NHI := 25611; TIME(NLO,NHI);
                        TLO := 0; THI := 0;  {FILER SPECIAL CASE: NO TIME}
                        IF LDIR^[LINX].DACCESS.YEAR = 100 THEN
                          BEGIN
                            IF DACCESS.YEAR = 100 THEN
                              BEGIN DACCESS := THEDATE; TLO := NLO; THI := NHI END
                            (*ELSE LEAVE ALONE...FILER SPECIAL CASE*)
                          END
                        ELSE
                          IF FMODIFIED AND (THEDATE.MONTH <> 0) THEN
                            BEGIN DACCESS := THEDATE; TLO := NLO; THI := NHI END
                          ELSE
                            BEGIN DACCESS := LDIR^[LINX].DACCESS;
                              TLO := LDIR^[LINX].FILLER2; THI := LDIR^[LINX].DTIMEHI
                            END;
                        DLASTBLK := DFIRSTBLK+FMAXBLK;
                        IF FSOFTBUF THEN DLASTBYTE := FMAXBYTE;
                        FHEADER.FILLER1 := 0;  {This had better work, Steve}
                        FILLER2 := TLO; DTIMEHI := THI;  {1.16: THE TIME OF DAY}
"""),
]
VERSION_HEAD = ('U134.4 OS and Filer source, revision 1.15  (25-Oct-26)',
                'U134.4 OS and Filer source, revision 1.16  (07-Nov-26)')
VERSION_NOTE = """Since 1.15: FCLOSE gives a file's entry the time of day it was written
(seconds since midnight + 1: GLOBALS.TEXT FILLER2, now 0..2047, the low
11 bits, and DTIMEHI, after DLASTBYTE, the high 6), from TIME's answers
of emulator 2.13 (25611), and THEDATE follows the PC's date (25612).
"""


def source_volume():
    """build/U134.4_OS_SOURCE_v1.16.BLK"""
    os.makedirs(os.path.dirname(OUT_SOURCE), exist_ok=True)
    shutil.copy(SOURCE, OUT_SOURCE)
    v = ucsdvol.Volume(OUT_SOURCE)
    for name, old, new in PATCHES:
        v.write(name, patch_text(v.read(name)[0], old, new), 3)
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
    # never set: 0xE5 in a disk's own copy, or (BIGGY's is now a compile too) memory's
    # leftovers at a block's end after a segment's code
    if len(old) != len(biggy) or any(biggy[i] != 0xE5 and i % 512 < 496 for i in diff):
        print('v1.15\'s SYSTEM compiled (%s) is not BIGGY\'s SYSTEM.PASCAL (%d bytes, %d)' % (mode, len(old), len(biggy)))
        return 1
    print('v1.15: SYSTEM compiled is BIGGY\'s SYSTEM.PASCAL but for %d bytes the compiler never sets' % len(diff))
    source_volume()
    new = compile_system(OUT_SOURCE, mode)
    open(OUT_SYSTEM, 'wb').write(new)
    print('%s: v1.16 (%d bytes, was %d)' % (os.path.relpath(OUT_SYSTEM, ROOT), len(new), len(old)))
    print('%s' % os.path.relpath(OUT_SOURCE, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
