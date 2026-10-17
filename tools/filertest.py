#!/usr/bin/env python3
"""filertest.py -- the Filer's K(runch) and the running operating system
(SYSTEM.FILER of tools/filer.py, revision 1.14 of the source).

On BIGGY with that Filer and a gap before SYSTEM.C (or before SYSTEM.PASCAL):
  * SYSTEM.C runs, K(runch of the boot disk moves SYSTEM.C: "Please re-boot";
    the disk boots again (SYSTEM.C, to the Command: prompt) and a program runs;
  * SYSTEM.PASCAL runs (a key at the boot), K(runch moves SYSTEM.C, not
    SYSTEM.PASCAL: no reboot, the Filer goes on, Q, and the system still works
    (F(iler again, which reads GETCMD's segment);
  * SYSTEM.PASCAL runs, K(runch moves SYSTEM.PASCAL: "Please re-boot";
  * SYSTEM.C runs, K(runch of another disk (#5): no reboot;
  * BIGGY's old Filer (v1.13), SYSTEM.C running and moved: no "Please re-boot"
    (what this Filer fixes).
In P-Code mode, and the first case in Z80 mode too.
"""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
VOLUMES = os.environ.get('VOLUMES', os.path.join(os.path.dirname(ROOT), 'UCSD-Pascal-Volumes'))
sys.path.insert(0, os.path.join(TINYC, 'tools'))
import ucsdvol

BIGGY = os.path.join(VOLUMES, 'BLK_format', 'Big_Disk.BLK')
KRUNCH = 'TYPE "F"\nWAIT "Filer:"\nTYPE "K"\nWAIT "?"\nTYPE "%s\\r"\nWAIT "(Y/N)"\nTYPE "Y"\n'


def compact(src, dst):
    """dst := src with its files one after another (as krunched), the boot blocks kept"""
    old = ucsdvol.Volume(src)
    new = ucsdvol.Volume.new(old.volname, len(old.data) // 512)
    new.data[0:1024] = old.data[0:1024]
    for first, last, kind, name, lastbyte, date in sorted(old.entries, key=lambda e: e[0]):
        if not new.find(name):                           # (BIGGY's second SYSTEM.LIST is a stale entry)
            new.write(name, bytes(old.data[first * 512: (last - 1) * 512 + lastbyte]), kind, date)
    open(dst, 'wb').write(new.image())


def main():
    d = tempfile.mkdtemp(prefix='filertest_')
    filer = open(os.path.join(ROOT, 'SYSTEM.FILER'), 'rb').read()

    def disk(name, gap_before, new_filer=True):
        dd = os.path.join(d, name)
        os.makedirs(dd)
        shutil.copy(os.path.join(TINYC, 'build', 'data', 'pascal.bin'), dd)
        path = os.path.join(dd, 'Big_Disk.BLK')
        compact(BIGGY, path)                             # no gap but the one made here
        v = ucsdvol.Volume(path)
        if new_filer:
            v.write('SYSTEM.FILER', filer, 2)
        es = sorted(v.entries, key=lambda e: e[0])
        names = [e[3] for e in es]
        v.remove(es[names.index(gap_before) - 1][3])      # a gap before it: K(runch moves it
        v.save()
        return dd
    disks = {'c': disk('c', 'SYSTEM.C'), 'pascal': disk('pascal', 'SYSTEM.PASCAL'),
             'old': disk('old', 'SYSTEM.C', False)}
    ucsdvol.main(['new', os.path.join(d, 'S.BLK'), 'S', '100'])
    work = ucsdvol.Volume.new('WORK', 200)
    work.write('A.TEXT', ucsdvol.text_to_ucsd('a\n' * 600), 3)
    work.write('B.TEXT', ucsdvol.text_to_ucsd('b\n'), 3)
    work.remove('A.TEXT')                                # a gap before B.TEXT
    open(os.path.join(d, 'W.BLK'), 'wb').write(work.image())
    n = [0]

    def run(data, script, mode='native', env=None):
        n[0] += 1
        sc = os.path.join(d, 'run%d.script' % n[0])
        out = os.path.join(d, 'out%d' % n[0])
        open(sc, 'w').write(script)
        r = subprocess.run([os.path.join(TINYC, 'build', 'run_verify'), data, os.path.join(d, 'W.BLK'), os.path.join(d, 'S.BLK'),
                            sc, mode, out, '', '600'], capture_output=True, text=True, env=dict(os.environ, **(env or {})))
        open(os.path.join(d, 'run%d.log' % n[0]), 'w').write(r.stdout + r.stderr)
        t = os.path.join(out, 'transcript.txt')
        tr = open(t, encoding='latin1').read() if os.path.exists(t) else ''
        return re.sub(r'[\r\n]+', '\n', tr), out, 'VERIFY SCRIPT COMPLETED' in r.stdout
    key = {'VERIFY_BOOTWAIT': '1'}
    checks = []
    for mode in ('native', 'z80'):
        tr, out, ok = run(disks['c'], 'WAIT "Command:"\n' + KRUNCH % '*' + 'WAIT "Please re-boot"\n', mode)
        checks.append((mode, 'SYSTEM.C runs and is moved: Please re-boot', ok and 'Moving forward SYSTEM.C\n' in tr))
        # the krunched disk boots again, and a program runs
        again = os.path.join(d, 'again%d' % n[0])
        os.makedirs(again)
        shutil.copy(os.path.join(TINYC, 'build', 'data', 'pascal.bin'), again)
        shutil.copy(os.path.join(out, 'VERIFY_BOOT.BLK'), os.path.join(again, 'Big_Disk.BLK'))
        tr, out, ok = run(again, 'WAIT "Command:"\nTYPE "F"\nWAIT "Filer:"\nTYPE "L"\nWAIT "?"\nTYPE "*SYSTEM.C\\r"\n'
                          'WAIT "Filer:"\nTYPE "Q"\nWAIT "Command:"\n', mode)
        checks.append((mode, 'the krunched disk boots SYSTEM.C again, the Filer lists it', ok and 'SYSTEM.C ' in tr))
        if mode == 'z80':
            break
        tr, out, ok = run(disks['c'], 'TYPENOW " "\nWAIT "Command:"\n' + KRUNCH % '*' + 'WAIT "crunched"\nWAIT "Filer:"\n'
                          'TYPE "Q"\nWAIT "Command:"\nTYPE "F"\nWAIT "Filer:"\nTYPE "Q"\nWAIT "Command:"\n', mode, key)
        checks.append((mode, 'SYSTEM.PASCAL runs, SYSTEM.C is moved: no reboot, the system goes on',
                       ok and 'SYSTEM.PASCAL\n' in tr and 'Moving forward SYSTEM.C\n' in tr and 're-boot' not in tr))
        tr, out, ok = run(disks['pascal'], 'TYPENOW " "\nWAIT "Command:"\n' + KRUNCH % '*' + 'WAIT "Please re-boot"\n', mode, key)
        checks.append((mode, 'SYSTEM.PASCAL runs and is moved: Please re-boot', ok and 'Moving forward SYSTEM.PASCAL\n' in tr))
        tr, out, ok = run(disks['c'], 'WAIT "Command:"\n' + KRUNCH % '#5' + 'WAIT "crunched"\nWAIT "Filer:"\nTYPE "Q"\n'
                          'WAIT "Command:"\n', mode)
        checks.append((mode, 'SYSTEM.C runs, another disk krunched: no reboot', ok and 'Moving forward B.TEXT' in tr and 're-boot' not in tr))
        tr, out, ok = run(disks['old'], 'WAIT "Command:"\n' + KRUNCH % '*' + 'WAIT "crunched"\nWAIT "Filer:"\n', mode)
        checks.append((mode, 'the old Filer: SYSTEM.C moved, no Please re-boot (the bug)',
                       ok and 'Moving forward SYSTEM.C\n' in tr and 're-boot' not in tr))
    bad = 0
    for mode, what, ok in checks:
        print('%-4s %-7s %s' % ('ok' if ok else 'BAD', mode, what))
        bad += not ok
    print('filer test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
