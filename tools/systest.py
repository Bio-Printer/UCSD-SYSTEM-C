#!/usr/bin/env python3
"""systest.py -- the system programs and the Command: prompt on the operating
system in C against the Pascal one: each scenario runs on both, from the same
disks, and the console transcripts and the disks they leave must be the same,
byte for byte (what differs is printed).

  systest.py [SCENARIO ...]        (none: all of them)

The boot disk is UCSD-Pascal-Volumes' BLK_format/Big_Disk.BLK (BIGGY, as
published; SYSTEST_PASCAL=FILE: that SYSTEM.PASCAL instead, e.g. build/SYSTEM.PASCAL of
tools/ospascal.py), with this repository's SYSTEM.C as SYSTEM.PASCAL for the C one; unit #5 is
WORK: with a few Pascal programs, #9 an empty SPARE:.  PSYS_MODE (native or
z80) as the other tests; SYSTEST_LAYOUT=reclaim or harvard: the native engine's
layouts with the interpreter's memory given back (VERIFY_RECLAIM), and code in an
I-space of its own (VERIFY_HARVARD).  Needs UCSD-TinyC (../ucsd-tinyc, or $TINYC) for the
emulator's build/run_verify and its tools.
"""
import os, re, shutil, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
VOLUMES = os.environ.get('VOLUMES', os.path.join(os.path.dirname(ROOT), 'UCSD-Pascal-Volumes'))
sys.path.insert(0, os.path.join(TINYC, 'tools'))
import ucsdvol

HELLO = """PROGRAM HELLO;
VAR I: INTEGER; S: STRING;
BEGIN
  WRITELN('Hello from Pascal');
  FOR I := 1 TO 3 DO WRITE(I * 7, ' ');
  WRITELN;
  S := 'abc';
  INSERT('XY', S, 2);
  WRITELN(S, ' ', LENGTH(S), ' ', POS('Y', S));
  DELETE(S, 1, 2);
  WRITELN(COPY(CONCAT(S, 'def'), 2, 3))
END.
"""
CRASH = """PROGRAM CRASH;
VAR A: ARRAY [1..3] OF INTEGER; I: INTEGER;
BEGIN
  WRITELN('crashing');
  I := 5;
  A[I] := 1
END.
"""
DEEP = """PROGRAM DEEP;
VAR N: INTEGER;
PROCEDURE R(I: INTEGER);
VAR PAD: ARRAY [1..50] OF INTEGER;
BEGIN PAD[1] := I; N := N + 1; R(I + 1) END;
BEGIN
  WRITELN('recursing');
  N := 0;
  R(1)
END.
"""
IOERR = """PROGRAM IOERR;
VAR F: TEXT;
BEGIN
  WRITELN('opening');
  RESET(F, '#9:NOSUCH.TEXT');
  WRITELN('not here')
END.
"""
KEYS = """PROGRAM KEYS;
VAR I: INTEGER; S: STRING; C: CHAR;
BEGIN
  WRITE('number? '); READLN(I); WRITELN('got ', I);
  WRITE('text? '); READLN(S); WRITELN('got [', S, '] ', LENGTH(S));
  WRITE('char? '); READ(C); WRITELN; WRITELN('got ', ORD(C));
  WRITE('two numbers? '); READ(I); WRITE(' '); READLN(I); WRITELN('got ', I);
  GOTOXY(10, 5); WRITE('at 10,5'); GOTOXY(0, 6); WRITELN('done')
END.
"""
FILES = """PROGRAM FILES;
VAR F: TEXT; G: FILE OF INTEGER; I, J: INTEGER; S: STRING;
BEGIN
  REWRITE(F, '#9:OUT.TEXT');
  FOR I := 1 TO 40 DO WRITELN(F, 'line ', I, '        with blanks');
  CLOSE(F, LOCK);
  RESET(F, '#9:OUT.TEXT');
  J := 0;
  WHILE NOT EOF(F) DO BEGIN READLN(F, S); J := J + 1 END;
  WRITELN('read ', J, ' lines, the last: ', S);
  CLOSE(F);
  REWRITE(G, '#9:NUMS.DATA');
  FOR I := 1 TO 300 DO BEGIN G^ := I * I; PUT(G) END;
  CLOSE(G, LOCK);
  RESET(G, '#9:NUMS.DATA');
  J := 0;
  WHILE NOT EOF(G) DO BEGIN J := J + G^; GET(G) END;
  WRITELN('sum ', J);
  (*$I-*) RESET(F, '#9:NOSUCH.TEXT'); (*$I+*)
  WRITELN('ioresult ', IORESULT);
  WRITE('number? '); READLN(I); WRITELN('got ', I * 2);
  WRITE('name? '); READLN(S); WRITELN('got ', S, '!')
END.
"""
BASFILE = """PROGRAM BASFILE;
VAR F: TEXT; S: STRING;
BEGIN
  REWRITE(F, '#9:PROG.BAS');
  WRITELN(F, '10 PRINT "HELLO"');
  WRITELN(F, '        20 END');
  CLOSE(F, LOCK);
  RESET(F, '#9:PROG.BAS');
  WHILE NOT EOF(F) DO BEGIN READLN(F, S); WRITELN('[', S, ']') END;
  CLOSE(F)
END.
"""

C = 'TYPE "C"\nWAIT "Compile what text?"\n'


def compile_steps(name):
    return (C + 'TYPE "#5:%s\\r"\nWAIT "To what codefile?"\nTYPE "#5:%s\\r"\nWAIT "Command:"\n' % (name, name))


def x(name, then='WAIT "Command:"\n'):
    return 'TYPE "X"\nWAIT "Execute what file?"\nTYPE "%s\\r"\n%s' % (name, then)


SCENARIOS = {
    # the boot, the ? prompt, an unknown key, U(ser restart without a program
    'prompt': 'TYPE "?"\nWAIT "S(hell"\nTYPE "Q"\nWAIT "Command:"\nTYPE "U"\nWAIT "Command:"\n',
    # the Filer: list the boot disk, what's the work file, the date; quit
    'filer': ('TYPE "F"\nWAIT "Filer:"\nTYPE "L"\nWAIT "?"\nTYPE "#5:\\r"\nWAIT "Filer:"\n'
              'TYPE "W"\nWAIT "Filer:"\nTYPE "V"\nWAIT "Filer:"\nTYPE "Q"\nWAIT "Command:"\n'),
    # the Filer changes the disks: transfer, change a name, remove, krunch
    'filer2': ('TYPE "F"\nWAIT "Filer:"\nTYPE "T"\nWAIT "?"\nTYPE "#5:HELLO.TEXT\\r"\nWAIT "?"\nTYPE "#9:H.TEXT\\r"\nWAIT "Filer:"\n'
               'TYPE "T"\nWAIT "?"\nTYPE "#5:CRASH.TEXT\\r"\nWAIT "?"\nTYPE "#9:C.TEXT\\r"\nWAIT "Filer:"\n'
               'TYPE "C"\nWAIT "?"\nTYPE "#9:H.TEXT\\r"\nWAIT "?"\nTYPE "HH.TEXT\\r"\nWAIT "Filer:"\n'
               'TYPE "R"\nWAIT "?"\nTYPE "#9:C.TEXT\\r"\nWAIT "Update directory"\nTYPE "Y"\nWAIT "Filer:"\n'
               'TYPE "K"\nWAIT "?"\nTYPE "#9:\\r"\nWAIT "(Y/N)"\nTYPE "Y"\nWAIT "Filer:"\n'
               'TYPE "L"\nWAIT "?"\nTYPE "#9:\\r"\nWAIT "Filer:"\nTYPE "Q"\nWAIT "Command:"\n'),
    # the Pascal compiler, then the program (strings: SCONCAT, SINSERT, SCOPY, SDELETE, SPOS)
    'compile': compile_steps('HELLO') + x('#5:HELLO'),
    # an execution error in a Pascal program: the message, Type <space>, re-initialized
    'crash': compile_steps('CRASH') + x('#5:CRASH', 'WAIT "Type <space>"\nTYPE " "\nWAIT "Command:"\n'),
    # files from a Pascal program: text, a typed file, a missing file, READLN of a number and a string
    'files': compile_steps('FILES') + x('#5:FILES', 'WAIT "number?"\nTYPE "21\\r"\nWAIT "name?"\nTYPE "Zed\\r"\nWAIT "Command:"\n'),
    # the work file: Filer G(et, then R(un compiles and runs it
    'workfile': ('TYPE "F"\nWAIT "Filer:"\nTYPE "G"\nWAIT "?"\nTYPE "#5:HELLO\\r"\nWAIT "Filer:"\nTYPE "Q"\nWAIT "Command:"\n'
                 'TYPE "R"\nWAIT "Command:"\n'),
    # the Editor: a file, then the work file updated; R(un compiles and runs it
    'editor': ('TYPE "E"\nWAIT "File?"\nTYPE "#5:HELLO\\r"\nWAIT ">Edit:"\nTYPE "Q"\nWAIT ">Quit:"\nTYPE "U"\n'
               'WAIT "Command:"\nTYPE "R"\nWAIT "Compile what text?"\nTYPE "\\r"\nWAIT "Command:"\n'),
    # a stack overflow: *STK OFLOW* and the Command: prompt again
    'stack': compile_steps('DEEP') + x('#5:DEEP'),
    # an I/O error with checking on
    'ioerr': compile_steps('IOERR') + x('#5:IOERR', 'WAIT "Type <space>"\nTYPE " "\nWAIT "Command:"\n'),
    # the console: READLN of numbers and strings with the delete keys (backspace, DEL), READ of a char,
    # GOTOXY (FGOTOXY)
    'keys': compile_steps('KEYS') + x('#5:KEYS', 'WAIT "number?"\nTYPE "123\\x08\\x0845\\r"\nWAIT "text?"\n'
                                      'TYPE "abcd\\x08\\x08xy\\r"\nWAIT "char?"\nTYPE "Q"\nWAIT "two numbers?"\n'
                                      'TYPE "7 9\\r"\nWAIT "Command:"\n'),
    # the Linker and the Assembler started from the prompt, then left
    'linker': ('TYPE "L"\nWAIT "Host file?"\nTYPE "\\r"\nWAIT "terminate"\nTYPE "\\e"\nWAIT "Command:"\n'),
    'assembler': ('TYPE "A"\nWAIT "Assemble what text?"\nTYPE "\\r"\nWAIT "Command:"\n'),
    # *SYSTEM.STARTUP on the boot disk: it runs instead of the welcome (then I(nitialize: again)
    'startup': 'TYPE "?"\nWAIT "S(hell"\nTYPE "I"\nWAIT "started up"\nWAIT "Command:"\n',
    # S(hell: the Tiny-C shell (pexec, the OS's files from C)
    'shell': ('TYPE "S"\nWAIT "> "\nTYPE "dir #5:\\r"\nWAIT "> "\nTYPE "type #5:hello.text\\r"\nWAIT "> "\n'
              'TYPE "copy #5:hello.text #9:\\r"\nWAIT "> "\nTYPE "bye\\r"\nWAIT "Command:"\n'),
    # a new NAME.BAS is a text file (1.15): written by a Pascal program and read back; the
    # Filer's T(ransfer to one; E(xtended list shows their kind
    'basfile': (compile_steps('BASFILE') + x('#5:BASFILE') +
                'TYPE "F"\nWAIT "Filer:"\nTYPE "T"\nWAIT "?"\nTYPE "#5:HELLO.TEXT\\r"\nWAIT "?"\nTYPE "#9:HELLO.BAS\\r"\nWAIT "Filer:"\n'
                'TYPE "E"\nWAIT "?"\nTYPE "#9:\\r"\nWAIT "Filer:"\nTYPE "Q"\nWAIT "Command:"\n'),
    # X(ecute of no file, of a text file; I(nitialize; H(alt
    'execute': (x('#5:NOSUCH') + x('#5:HELLO.TEXT.') + 'TYPE "?"\nWAIT "S(hell"\nTYPE "I"\nWAIT "Command:"\n'
                'TYPE "?"\nWAIT "S(hell"\nTYPE "H"\n'),
}


STARTUP = """#include <stdio.h>
int main(void)
{
    printf("started up\\n");
    return 0;
}
"""


def disks(d, code, name):
    """d/data (the boot disk with SYSTEM.PASCAL := code, None: the Pascal one), d/WORK.BLK, d/SPARE.BLK"""
    os.makedirs(os.path.join(d, 'data'), exist_ok=True)
    shutil.copy(os.path.join(TINYC, 'build', 'data', 'pascal.bin'), os.path.join(d, 'data'))
    boot = os.path.join(d, 'data', 'Big_Disk.BLK')
    shutil.copy(os.path.join(VOLUMES, 'BLK_format', 'Big_Disk.BLK'), boot)
    v = ucsdvol.Volume(boot)
    if code:
        v.write('SYSTEM.PASCAL', code, 2)
    # (BIGGY's boot would boot its SYSTEM.C, whatever SYSTEM.PASCAL is: the one under test only)
    v.remove('SYSTEM.C')
    if name == 'startup':                       # (a Tiny-C program as SYSTEM.STARTUP)
        from tcrun import compile_c
        open(os.path.join(d, 'st.c'), 'w').write(STARTUP)
        b, c = compile_c(os.path.join(d, 'st.c'), d, z80=True)
        v.write('SYSTEM.STARTUP', open(c, 'rb').read(), 2)
    v.save()
    ucsdvol.main(['new', os.path.join(d, 'WORK.BLK'), 'WORK', '1000'])
    ucsdvol.main(['new', os.path.join(d, 'SPARE.BLK'), 'SPARE', '400'])
    v = ucsdvol.Volume(os.path.join(d, 'WORK.BLK'))
    for name, text in (('HELLO.TEXT', HELLO), ('CRASH.TEXT', CRASH), ('FILES.TEXT', FILES), ('BASFILE.TEXT', BASFILE),
                       ('DEEP.TEXT', DEEP), ('IOERR.TEXT', IOERR), ('KEYS.TEXT', KEYS)):
        v.write(name, ucsdvol.text_to_ucsd(text), 3)
    v.save()


def run(d, script, mode):
    sp = os.path.join(d, 'run.script')
    open(sp, 'w').write(('WAIT "started up"\n' if 'started up' in script else '') + 'WAIT "Command:"\n' + script)
    out = os.path.join(d, 'out')
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    env = dict(os.environ)
    env.pop('VERIFY_RECLAIM', None)
    if os.environ.get('SYSTEST_LAYOUT') in ('reclaim', 'harvard'):     # (native mode only)
        env['VERIFY_RECLAIM'] = '1'
        if os.environ['SYSTEST_LAYOUT'] == 'harvard':
            env['VERIFY_HARVARD'] = '1'
    r = subprocess.run([os.path.join(TINYC, 'build', 'run_verify'), os.path.join(d, 'data'),
                        os.path.join(d, 'WORK.BLK'), os.path.join(d, 'SPARE.BLK'), sp, mode, out, '', '900'],
                       capture_output=True, text=True, env=env)
    tr = open(os.path.join(out, 'transcript.txt'), encoding='latin1').read() if os.path.exists(os.path.join(out, 'transcript.txt')) else ''
    imgs = {}
    for f in sorted(os.listdir(out)):
        if f.endswith('.BLK'):
            imgs[f] = open(os.path.join(out, f), 'rb').read()
    return 'VERIFY SCRIPT COMPLETED' in r.stdout, tr, imgs


def codebytes(data):
    """a code file without what follows each segment in its last block (whatever the
    compiler's buffer held: not code)"""
    keep = bytearray(data[:512])
    for seg in range(16):
        blk, length = struct.unpack_from('<HH', data, 4 * seg)
        if length:
            keep += data[blk * 512: blk * 512 + length]
    return bytes(keep)


def files_of(img):
    """{name: (kind, bytes, blocks, last byte, date)} of a volume image, the boot disk's
    SYSTEM.PASCAL left out"""
    fd, p = tempfile.mkstemp(suffix='.BLK')
    os.write(fd, img)
    os.close(fd)
    try:
        v = ucsdvol.Volume(p)
        res = {}
        for e in v.entries:
            if e[3] != 'SYSTEM.PASCAL':
                data = v.read(e[3])[0]
                res[e[3]] = (e[2], codebytes(data) if e[2] & 15 == 2 else data, e[1] - e[0], e[4], e[5])
        return res
    finally:
        os.remove(p)


def main(args):
    mode = os.environ.get('PSYS_MODE', 'native')
    names = args or list(SCENARIOS)
    code = open(os.path.join(ROOT, 'SYSTEM.C'), 'rb').read()
    bad = 0
    for name in names:
        res = {}
        pas = open(os.environ['SYSTEST_PASCAL'], 'rb').read() if os.environ.get('SYSTEST_PASCAL') else None
        for which, c in (('Pascal', pas), ('C', code)):
            d = tempfile.mkdtemp(prefix='systest_')
            disks(d, c, name)
            res[which] = run(d, SCENARIOS[name], mode)
            shutil.rmtree(d, ignore_errors=True)
        (okp, trp, imp), (okc, trc, imc) = res['Pascal'], res['C']
        problems = []
        if not okp:
            problems.append('the script did not finish on the Pascal OS')
        if not okc:
            problems.append('the script did not finish on the C OS')
        # the memory the Compiler has ("[ 7057 words]") is a difference to show, not to fail
        words = lambda t: re.findall(r'\[ *(\d+) words\]|space = (\d+) words', t)
        wp, wc = words(trp), words(trc)
        if wp and len(wp) == len(wc):
            print('      the Compiler\'s memory: Pascal OS %s words, C OS %s' % (' '.join(a + b for a, b in wp), ' '.join(a + b for a, b in wc)))
            trp = re.sub(r'\[ *\d+ words\]|space = \d+ words', '[words]', trp)
            trc = re.sub(r'\[ *\d+ words\]|space = \d+ words', '[words]', trc)
        # the run stops when the last WAIT is met: how much of that line is printed by then varies
        cut = lambda t: t[:t.rfind('Command:') + 8] if 'Command:' in t else t
        trp, trc = cut(trp), cut(trc)
        if not okp and os.environ.get('SYSTEST_SHOW'):
            print(trp.replace('\r', '\n')[-1500:])
        if trp != trc:
            lp, lc = trp.replace('\r', '\n').split('\n'), trc.replace('\r', '\n').split('\n')
            for i in range(max(len(lp), len(lc))):
                a = lp[i] if i < len(lp) else '(end)'
                b = lc[i] if i < len(lc) else '(end)'
                if a != b:
                    problems.append('transcript line %d:\n      Pascal %r\n      C      %r' % (i + 1, a[:150], b[:150]))
                    break
        for f in sorted(set(imp) | set(imc)):
            if f not in imp or f not in imc:
                problems.append('%s only on one' % f)
                continue
            a, b = files_of(imp[f]), files_of(imc[f])
            for n in sorted(set(a) | set(b)):
                if a.get(n) != b.get(n):
                    problems.append('%s: %s differs (Pascal %s, C %s)' % (f, n, a.get(n, ('-',))[0:1] + (len(a[n][1]),) if n in a else '-',
                                                                       b.get(n, ('-',))[0:1] + (len(b[n][1]),) if n in b else '-'))
        bad += bool(problems)
        print('%-4s  %s' % ('BAD' if problems else 'ok', name))
        for p in problems:
            print('      ' + p)
    print('system test (%s): %s' % (mode, 'FAILED' if bad else 'PASSED'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
