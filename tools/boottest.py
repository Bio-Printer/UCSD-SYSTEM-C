#!/usr/bin/env python3
"""boottest.py -- the boot that chooses the operating system (tools/boot.py):
SYSTEM.C, or SYSTEM.PASCAL when a key is typed within 2 seconds.

On a copy of the boot disk with the choosing SYSTEM.MICRO, in P-Code mode
(standard, Reclaim and Harvard layouts: the emulator's native boot) and in Z80
mode (the Z80 code itself):
  * no key: SYSTEM.C boots -- a small operating system of Tiny-C's ostest that
    says "OS mode" and halts, so it is seen to be the one that runs; then this
    repository's SYSTEM.C up to the Command: prompt;
  * a key at once (TYPENOW, with VERIFY_BOOTWAIT: the real 2 seconds): SYSTEM.PASCAL,
    up to the Command: prompt, and the key is not left for it;
  * no SYSTEM.C: "not found; booting SYSTEM.PASCAL";
  * a key and no SYSTEM.PASCAL: "not found; booting SYSTEM.C";
  * with the old SYSTEM.MICRO: SYSTEM.PASCAL as before, no question;
and BOOTC.TEXT (SYS-C:, the boot's source) assembled by the P-System's Z80
Assembler is the code tools/boot.py puts into SYSTEM.MICRO, byte for byte.

  boottest.py [--runner PATH]   (default: UCSD-TinyC's build/run_verify; needs emulator 2.08)
"""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TINYC = os.environ.get('TINYC', os.path.join(os.path.dirname(ROOT), 'ucsd-tinyc'))
sys.path.insert(0, os.path.join(TINYC, 'tools'))
sys.path.insert(0, HERE)
import ucsdvol
import boot
from ostest import OS
from tcrun import build_tc, TC, INC

ASK = boot.ASK.strip()
MODES = [('native', {}), ('native', {'VERIFY_RECLAIM': '1'}), ('native', {'VERIFY_RECLAIM': '1', 'VERIFY_HARVARD': '1'}),
         ('z80', {})]


def tiny_os(d):
    build_tc()
    open(os.path.join(d, 'os.c'), 'w').write(OS)
    subprocess.check_call([TC, '-I', INC, 'os.c', '-o', 'TINY.CODE'], cwd=d, stdout=subprocess.DEVNULL)
    return open(os.path.join(d, 'TINY.CODE'), 'rb').read()


def main(args):
    runner = args[args.index('--runner') + 1] if '--runner' in args else os.path.join(TINYC, 'build', 'run_verify')
    d = tempfile.mkdtemp(prefix='boottest_')
    base = os.path.join(TINYC, 'build', 'img', 'Big_Disk.BLK')
    v = ucsdvol.Volume(base)
    classic_micro = v.read('SYSTEM.MICRO')[0]
    micro = boot.patch(classic_micro)
    pascal = v.read('SYSTEM.PASCAL')[0]
    c_os = open(os.path.join(ROOT, 'SYSTEM.C'), 'rb').read()
    tiny = tiny_os(d)

    def disk(name, files, remove=()):
        dd = os.path.join(d, name)
        os.makedirs(dd)
        shutil.copy(os.path.join(TINYC, 'build', 'data', 'pascal.bin'), dd)
        shutil.copy(base, os.path.join(dd, 'Big_Disk.BLK'))
        vol = ucsdvol.Volume(os.path.join(dd, 'Big_Disk.BLK'))
        for f in remove:
            vol.remove(f)
        for f, data in files.items():
            vol.write(f, data, 2)
        vol.save()
        return dd
    disks = {'tiny': disk('tiny', {'SYSTEM.MICRO': micro, 'SYSTEM.C': tiny}),
             'c': disk('c', {'SYSTEM.MICRO': micro, 'SYSTEM.C': c_os}),
             'noc': disk('noc', {'SYSTEM.MICRO': micro}),
             'nopascal': disk('nopascal', {'SYSTEM.MICRO': micro, 'SYSTEM.C': tiny}, ['SYSTEM.PASCAL']),
             'old': disk('old', {'SYSTEM.C': tiny})}
    for n in ('W', 'S'):
        ucsdvol.main(['new', os.path.join(d, n + '.BLK'), n, '100'])
    n = [0]

    def run(data, script, mode, env):
        n[0] += 1
        sc = os.path.join(d, 'run%d.script' % n[0])
        out = os.path.join(d, 'out%d' % n[0])
        open(sc, 'w').write(script)
        r = subprocess.run([runner, data, os.path.join(d, 'W.BLK'), os.path.join(d, 'S.BLK'), sc, mode, out, '', '300'],
                           capture_output=True, text=True, env=dict(os.environ, **env))
        open(os.path.join(d, 'run%d.log' % n[0]), 'w').write(r.stdout + r.stderr)
        t = os.path.join(out, 'transcript.txt')
        tr = open(t, encoding='latin1').read() if os.path.exists(t) else ''
        return re.sub(r'[\r\n]+', '\n', tr)          # (a CR is a line end there too: no empty lines)
    key = 'TYPENOW " "\n'
    command = 'WAIT "Command:"\n'
    checks = []
    for mode, env in MODES:
        what = mode + (' harvard' if 'VERIFY_HARVARD' in env else ' reclaim' if env else '')
        tr = run(disks['tiny'], 'WAIT "halt"\n', mode, env)
        checks.append((what, 'no key: SYSTEM.C (the tiny one runs)', ASK + ' SYSTEM.C\n' in tr and 'OS mode: 004' in tr))
        tr = run(disks['c'], command, mode, env)
        checks.append((what, 'no key: SYSTEM.C, this one, to the Command: prompt', ASK + ' SYSTEM.C\n' in tr and 'Command:' in tr))
        tr = run(disks['tiny'], key + command + 'TYPE "H"\nHALTED\n', mode, dict(env, VERIFY_BOOTWAIT='1'))
        checks.append((what, 'a key: SYSTEM.PASCAL (the key not left for it), H(alt', ASK + ' SYSTEM.PASCAL\n' in tr
                       and 'OS mode' not in tr and tr.count('Command:') >= 1))
        tr = run(disks['noc'], command, mode, env)
        checks.append((what, 'no SYSTEM.C: SYSTEM.PASCAL', ASK + ' SYSTEM.C\n' + boot.NOTFOUND + 'SYSTEM.PASCAL\n' in tr
                       and 'Command:' in tr))
        tr = run(disks['nopascal'], key + 'WAIT "halt"\n', mode, dict(env, VERIFY_BOOTWAIT='1'))
        checks.append((what, 'a key, no SYSTEM.PASCAL: SYSTEM.C', ASK + ' SYSTEM.PASCAL\n' + boot.NOTFOUND + 'SYSTEM.C\n' in tr
                       and 'OS mode: 004' in tr))
        tr = run(disks['old'], command, mode, env)
        checks.append((what, 'the old SYSTEM.MICRO: SYSTEM.PASCAL, no question', ASK not in tr and 'Command:' in tr and 'OS mode' not in tr))
    w = ucsdvol.Volume(os.path.join(d, 'W.BLK'))
    w.write('BOOTC.TEXT', ucsdvol.text_to_ucsd(boot.text()), 3)
    w.save()
    run(disks['old'], command + 'TYPE "A"\nWAIT "Assemble what text?"\nTYPE "#5:BOOTC\\r"\nWAIT "To what codefile?"\n'
        'TYPE "#5:BOOTC\\r"\nWAIT "(<CR> for none)"\nTYPE "\\r"\nWAIT "Command:"\n', 'native', {})
    code, sym, _ = boot.assemble()
    try:
        got = ucsdvol.Volume(os.path.join(d, 'out%d' % n[0], 'VERIFY_SOURCE.BLK')).read('BOOTC.CODE')[0]
    except (Exception, SystemExit):
        got = b''
    checks.append(('native', 'BOOTC.TEXT assembled on the P-System: the same %d bytes' % len(code), got[512:512 + len(code)] == code))
    bad = 0
    for what, check, ok in checks:
        print('%-4s %-16s %s' % ('ok' if ok else 'BAD', what, check))   # (BAD: runN.log, outN/ in the work directory)
        bad += not ok
    print('boot test: %s   (work: %s)' % ('FAILED' if bad else 'PASSED', d))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
