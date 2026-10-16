#!/usr/bin/env python3
"""suite.py -- run UCSD-TinyC's tests on the operating system in C.

  suite.py [--pascal] TEST ... (names of UCSD-TinyC/tools/*.py, e.g. quicktest pexectest)

The boot disks the tests use (UCSD-TinyC's build/BIGGY.BLK and
build/data/Big_Disk.BLK) get this repository's SYSTEM.PASCAL for the run and
their own back afterwards (--pascal: the Pascal one, unchanged, for a
comparison).  Each test's last line is printed, and the whole output goes to
build/suite/TEST.log.  PSYS_MODE (native or z80) is passed on.  The TinyC
checkout is ../ucsd-tinyc, or $TINYC.
"""
import os, shutil, signal, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
sys.path.insert(0, os.path.join(TINYC, 'tools'))
import ucsdvol

DISKS = [os.path.join(TINYC, 'build', 'BIGGY.BLK'), os.path.join(TINYC, 'build', 'data', 'Big_Disk.BLK')]


BACKUP = os.path.join(ROOT, 'build', 'suite')


def backup_of(d):
    return os.path.join(BACKUP, 'orig.' + os.path.basename(os.path.dirname(d)) + '.' + os.path.basename(d))


def restore(saved=None):
    """the boot disks back: from saved, or from the backups an interrupted run left"""
    for d in DISKS:
        b = backup_of(d)
        if saved and d in saved:
            open(d, 'wb').write(saved[d])
        elif os.path.exists(b):
            shutil.copy(b, d)
        if os.path.exists(b):
            os.remove(b)


def install(code):
    """SYSTEM.PASCAL := code on the boot disks (a copy of each kept in build/suite until
    restore); their old bytes"""
    restore()
    os.makedirs(BACKUP, exist_ok=True)
    saved = {}
    for d in DISKS:
        saved[d] = open(d, 'rb').read()
        shutil.copy(d, backup_of(d))
        v = ucsdvol.Volume(d)
        v.write('SYSTEM.PASCAL', code, 2)
        v.save()
    return saved


def main(args):
    pascal = '--pascal' in args
    tests = [a for a in args if not a.startswith('--')]
    logs = os.path.join(ROOT, 'build', 'suite')
    os.makedirs(logs, exist_ok=True)
    saved = None
    if not pascal:
        saved = install(open(os.path.join(ROOT, 'SYSTEM.PASCAL'), 'rb').read())
    bad = 0
    try:
        for t in tests:
            t0 = time.time()
            cmd = [sys.executable, os.path.join(TINYC, 'tools', t if t.endswith('.py') else t + '.py')]
            r = subprocess.run(cmd, capture_output=True, text=True, cwd=TINYC)
            out = r.stdout + r.stderr
            open(os.path.join(logs, t.replace('.py', '') + '.log'), 'w').write(out)
            lines = [l for l in out.split('\n') if l.strip()]
            last = lines[-1] if lines else '(no output)'
            failed = r.returncode != 0 or 'FAIL' in last.upper() and 'PASS' not in last.upper()
            bad += failed
            print('%-5s %-16s %4ds  %s' % ('FAIL' if failed else 'ok', t, time.time() - t0, last[:110]))
            sys.stdout.flush()
    finally:
        if saved:
            restore(saved)
    print('%d of %d failed (%s OS, %s mode)' % (bad, len(tests), 'Pascal' if pascal else 'C',
                                                os.environ.get('PSYS_MODE', 'native')))
    return 1 if bad else 0


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, signal.default_int_handler)     # (the disks back on a kill too)
    sys.exit(main(sys.argv[1:]))
