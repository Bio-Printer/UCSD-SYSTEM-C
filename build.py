#!/usr/bin/env python3
"""build.py -- make SYSTEM.PASCAL from src/system.c with Tiny-C, and check it.

  build.py [--tc PATH]

Tiny-C is UCSD-TinyC's host compiler: ../ucsd-tinyc/build/tc (built from
tinyc/tc.c with gcc when it is not there), or $TINYC/build/tc, or --tc.  It
needs Tiny-C's #pragma system (an operating system: SYSTEM.PASCAL's layout).

What compiled programs depend on is checked, and the build fails without it:
  * segment 0's procedures 2..43 have the numbers of the Pascal one's
    (EXECERROR 2, FINIT 3 ... COMMAND 43), the segments theirs (USERPROG 1,
    DEBUGGER 2, PRINTERR 3, INITIALI 4, GETCMD 5), each segment's procedure 1
    the one its callers call;
  * procedure 1 is main, the outer block (lex level -1), with the 275 words of
    globals first (550 bytes, as the Pascal one: the system programs address
    them) -- and the comments of globals.h, which say each one's offset, end at
    word 276;
  * EXECERROR has exactly one word of locals (the interpreter puts BOMBP where
    its mark stack will be: SP - 14);
  * a Pascal string written in C ("\\005.TEXT") has the length it says.
Then the sizes are compared with the Pascal SYSTEM.PASCAL when it is found
(UCSD-Pascal-Volumes beside this repository).
"""
import os, re, struct, subprocess, sys
ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, 'src')
OUT = os.path.join(ROOT, 'SYSTEM.PASCAL')
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
PASCAL_OS = os.path.join(os.path.dirname(ROOT), 'UCSD-Pascal-Volumes', 'Files_Extracted_Raw', 'Big_Disk', 'SYSTEM.PASCAL')

FIXED = ['execerror', 'finit', 'freset', 'fopen', 'fclose', 'fget', 'fput', 'xseek', 'feof', 'feoln',
         'freadint', 'fwriteint', 'xreadreal', 'xwritereal', 'freadchar', 'fwritechar', 'freadstring',
         'fwritestring', 'fwritebytes', 'freadln', 'fwriteln', 'sconcat', 'sinsert', 'scopy', 'sdelete',
         'spos', 'fblockio', 'fgotoxy',                                         # 2..29: compiled programs
         'volsearch', 'writedir', 'dirsearch', 'scantitle', 'delentry', 'insentry', 'homecursor',
         'clearscreen', 'clearline', 'prompt', 'spacewait', 'getchar', 'fetchdir', 'command']  # 30..43
SEGMENTS = {1: ('USERPROG', 'userprogram'), 2: ('DEBUGGER', 'debugger'), 3: ('PRINTERR', 'printerror'),
            4: ('INITIALI', 'initialize'), 5: ('GETCMD  ', 'getcmd')}


def tc_path(args):
    if '--tc' in args:
        return args[args.index('--tc') + 1]
    tc = os.path.join(TINYC, 'build', 'tc')
    srcs = [os.path.join(TINYC, 'tinyc', f) for f in os.listdir(os.path.join(TINYC, 'tinyc')) if f.endswith(('.c', '.h'))]
    if not os.path.exists(tc) or any(os.path.getmtime(s) > os.path.getmtime(tc) for s in srcs):
        os.makedirs(os.path.dirname(tc), exist_ok=True)
        subprocess.check_call(['gcc', '-O1', '-w', '-o', tc, os.path.join(TINYC, 'tinyc', 'tc.c')])
    return tc


def segments(path):
    """{segnum: (name, bytes, {procnum: (lex, parmsz, datasz)})} of a code file"""
    data = open(path, 'rb').read()
    res = {}
    for s in range(16):
        blk, length = struct.unpack_from('<HH', data, 4 * s)
        if length == 0:
            continue
        name = data[64 + 8 * s: 72 + 8 * s].decode('latin1')
        seg = data[blk * 512: blk * 512 + length]
        nprocs = seg[-1]
        procs = {}
        for p in range(1, nprocs + 1):
            ptr = len(seg) - 2 - 2 * p
            jtab = ptr - struct.unpack_from('<H', seg, ptr)[0]
            lex = seg[jtab + 1] - 256 if seg[jtab + 1] > 127 else seg[jtab + 1]
            datasz, parmsz = struct.unpack_from('<HH', seg, jtab - 8)
            procs[p] = (lex, parmsz, datasz)
        res[s] = (name, length, procs)
    return res


def words_of_globals():
    """the word after the last global of globals.h, from its offset comments"""
    last = 0
    for line in open(os.path.join(SRC, 'globals.h')):
        m = re.match(r'static .*?/\* *(\d+)', line)
        if m:
            last = int(m.group(1))
    m = re.search(r'/\* *(\d+) \*/\s*$', open(os.path.join(SRC, 'globals.h')).read().split('/* the interpreter')[0])
    return int(m.group(1)) if m else last


def check_literals():
    """each Pascal string literal written in C, "\\nnn..." (an octal length, then
    the characters), says its own length"""
    bad = []
    for f in sorted(os.listdir(SRC)):
        if not f.endswith(('.c', '.h')):
            continue
        for n, line in enumerate(open(os.path.join(SRC, f)), 1):
            for m in re.finditer(r'"\\([0-7]{3})((?:[^"\\]|\\.)*)"', line):
                text = re.sub(r'\\(.)', r'\1', m.group(2))
                if int(m.group(1), 8) != len(text):
                    bad.append('%s:%d: "%s" is %d characters, not %d' % (f, n, text, len(text), int(m.group(1), 8)))
    return bad


def main(args):
    tc = tc_path(args)
    env = dict(os.environ, TINYC_MAP='1')
    r = subprocess.run([tc, '-I', os.path.join(TINYC, 'tinyc', 'include'), 'system.c', '-o', OUT],
                       cwd=SRC, capture_output=True, text=True, env=env)
    print(''.join(l + '\n' for l in r.stdout.split('\n') if l.strip() and 'Done' not in l and 'Tiny-C' not in l), end='')
    if r.returncode != 0:
        print(r.stderr)
        return 1
    procmap = {}
    for line in r.stderr.split('\n'):
        f = line.split()
        if len(f) == 6 and f[0] == 'MAP':
            procmap[f[5]] = (int(f[1]), int(f[2]))
    errors = check_literals()
    for i, name in enumerate(FIXED):
        if procmap.get(name) != (0, i + 2):
            errors.append('%s is %s, not segment 0 procedure %d' % (name, procmap.get(name), i + 2))
    segs = segments(OUT)
    for s, (sname, proc) in SEGMENTS.items():
        if s not in segs or segs[s][0] != sname or procmap.get(proc) != (s, 1):
            errors.append('segment %d is not %s with %s its procedure 1' % (s, sname.strip(), proc))
    p1 = segs[0][2][1]
    if p1[:2] != (-1, 0) or p1[2] < 550 or procmap.get('main') != (0, 1):
        errors.append('procedure 1 is (lex, parmsz, datasz) %s, not main, (-1, 0, 550 and its own words)' % (p1,))
    if 275 + 1 != words_of_globals():
        errors.append('the globals are %d words, not 275' % (words_of_globals() - 1))
    if segs[0][2][2][2] != 2:
        errors.append('EXECERROR has %d bytes of locals, not 2 (BOMBP)' % segs[0][2][2][2])
    for e in errors:
        print('error: ' + e)
    if errors:
        os.remove(OUT)
        return 1
    print('SYSTEM.PASCAL: procedures 2..43 and segments 1..5 as the Pascal one\'s, 275 words of globals')
    if os.path.exists(PASCAL_OS):
        ref = segments(PASCAL_OS)
        print('%-10s %8s %8s' % ('segment', 'Pascal', 'C'))
        for s in sorted(set(ref) | set(segs)):
            print('%2d %-8s %7s %8s' % (s, (segs.get(s) or ref.get(s))[0].strip(),
                                      ref[s][1] if s in ref else '-', segs[s][1] if s in segs else '-'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
