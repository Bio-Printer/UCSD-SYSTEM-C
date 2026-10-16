#!/usr/bin/env python3
"""boot.py -- the boot that chooses the operating system: SYSTEM.C, or
SYSTEM.PASCAL when a key is typed within 2 seconds.

  boot.py SYSTEM.MICRO OUT        patch an interpreter (BIGGY's SYSTEM.MICRO)
  boot.py --text OUT              the source, BOOTC.TEXT (Z80 assembly)

The boot is the interpreter's BOOT (SYSTEM.MICRO, U120_Z80_P-CODE_SOURCE
BOOT.TEXT, 1EBA..2018): it reads the directory of the boot disk, finds
SYSTEM.PASCAL (SYSTLE), reads its segment 0 and starts it.  Two of its
instructions change:

  1EE2  CALL IOINIT  ->  CALL CHOOSE   (CHOOSE calls IOINIT, then asks)
  1F27  JP   $45     ->  JP   NOTFND   (the name was not in the directory)

and CHOOSE, USE, NOTFND and PRINT go into the unused end of the file
(2A40.., above the directory buffer and RELSEG, so they are still there
when NOTFND runs; the heap covers them afterwards).  CHOOSE says what it
does, waits up to 2 seconds for a key (OUT (2): the emulator's key wait,
version 2.08; IN (0) / IN (1): CONST / CONIN), and puts the name into
SYSTLE and its length + 1 into the search's LD B,0EH.  NOTFND tries the
other name once (SYSTEM.C missing: SYSTEM.PASCAL, and the other way round),
then says that neither is there and stops, as BOOT stopped before.

The emulator's native boot (P-Code mode) does what this code does when it
finds these bytes (PSystemEngine.cpp, kChoosingBoot), so this file and
it must agree; build.py checks the bytes against it when the emulator's
source is beside this repository.
"""
import re, sys

ORG = 0x2A40                        # CHOOSE ...: in the unused end of SYSTEM.MICRO
FILE_OFFSET = 0x200                 # file offset = address + 0x200 (the file is loaded at 0400, moved to 0100)
EQU = {'IOINIT': 0x1EBA, 'SYSTLE': 0x1ECC, 'SEARCH': 0x1EFC, 'CMPLEN': 0x1F0C}
PATCHES = [(0x1EE2, 'CALL', 'CHOOSE', 'IOINIT'), (0x1F27, 'JP', 'NOTFND', 0x1F27)]
KEYWAIT = 20                        # tenths of a second

ASK = '\r\nType any key within 2 seconds to boot SYSTEM.PASCAL instead of SYSTEM.C: '
NOTFOUND = '  not found, booting '   # (no ; in a string: the assembler's comment)
NEITHER = '  not found either: no operating system on this disk\r\n'

# label, instruction, comment
CODE = [
    ('CHOOSE', 'CALL IOINIT', 'what BOOT called here: the I/O drivers, MEMTOP'),
    ('', 'LD HL,ASK', ''),
    ('', 'CALL PRINT', ''),
    ('', 'LD A,%d' % KEYWAIT, '2 seconds, in tenths'),
    ('', 'OUT (2),A', 'wait until a key is typed or the time is up (emulator 2.08)'),
    ('', 'IN A,(0)', 'CONST: a key?'),
    ('', 'OR A', ''),
    ('', 'LD HL,NAMEC', ''),
    ('', 'JR Z,USE', 'no: SYSTEM.C'),
    ('', 'IN A,(1)', 'CONIN: the key is read (not left for the system)'),
    ('', 'LD HL,NAMEP', 'SYSTEM.PASCAL'),
    ('USE', 'LD A,(HL)', 'HL = the name (a string): its length'),
    ('', 'INC A', ''),
    ('', 'LD (CMPLEN),A', 'the search compares the length byte and the characters'),
    ('', 'LD C,A', ''),
    ('', 'LD B,0', ''),
    ('', 'PUSH HL', ''),
    ('', 'LD DE,SYSTLE', 'SYSTLE := the name'),
    ('', 'LDIR', ''),
    ('', 'POP HL', ''),
    ('', 'LD B,(HL)', 'say it'),
    ('$1', 'INC HL', ''),
    ('', 'LD A,(HL)', ''),
    ('', 'OUT (1),A', ''),
    ('', 'DJNZ $1', ''),
    ('', 'LD HL,CRLF', ''),
    ('PRINT', 'LD A,(HL)', 'HL = text ending in 0'),
    ('', 'OR A', ''),
    ('', 'RET Z', ''),
    ('', 'OUT (1),A', 'CONOUT'),
    ('', 'INC HL', ''),
    ('', 'JR PRINT', ''),
    ('NOTFND', 'LD HL,TRIED', 'the name is not in the directory'),
    ('', 'LD A,(HL)', ''),
    ('', 'OR A', ''),
    ('', 'JR NZ,NONE', 'the other one was not there either'),
    ('', 'INC (HL)', ''),
    ('', 'LD HL,NOTMSG', ''),
    ('', 'CALL PRINT', ''),
    ('', 'LD A,(CMPLEN)', 'which one was it?'),
    ('', 'CP 0EH', ''),
    ('', 'LD HL,NAMEC', ''),
    ('', 'JR Z,$2', 'SYSTEM.PASCAL: try SYSTEM.C'),
    ('', 'LD HL,NAMEP', 'SYSTEM.C: try SYSTEM.PASCAL'),
    ('$2', 'CALL USE', ''),
    ('', 'JP SEARCH', 'search the directory again (it is still in its buffer)'),
    ('NONE', 'LD HL,NONMSG', ''),
    ('', 'CALL PRINT', ''),
    ('$3', 'JR $3', 'stop, as BOOT did'),
    ('NAMEP', '.BYTE 0DH', ''),
    ('', '.ASCII "SYSTEM.PASCAL"', ''),
    ('NAMEC', '.BYTE 08H', ''),
    ('', '.ASCII "SYSTEM.C"', ''),
    ('TRIED', '.BYTE 0', '1 when the other name has been tried'),
    ('ASK', '.TEXT', ASK),
    ('NOTMSG', '.TEXT', NOTFOUND),
    ('NONMSG', '.TEXT', NEITHER),
    ('CRLF', '.TEXT', '\r\n'),
]

R8 = {'B': 0, 'C': 1, 'D': 2, 'E': 3, 'H': 4, 'L': 5, '(HL)': 6, 'A': 7}
RP = {'BC': 0, 'DE': 1, 'HL': 2, 'SP': 3}
CC = {'NZ': 0, 'Z': 1, 'NC': 2, 'C': 3}
FIXED = {'OR A': [0xB7], 'LDIR': [0xED, 0xB0], 'INC A': [0x3C], 'INC HL': [0x23], 'INC (HL)': [0x34],
         'RET Z': [0xC8], 'RET': [0xC9], 'PUSH HL': [0xE5], 'POP HL': [0xE1]}


def num(s):
    s = s.strip()
    if s.upper().endswith('H'):
        return int(s[:-1], 16)
    return int(s)


def encode(ins, comment, pc, sym):
    """the bytes of one instruction at pc (sym: the labels; None in the first pass)"""
    def val(s):
        if s in EQU:
            return EQU[s]
        if re.match(r'^[0-9][0-9A-F]*H?$', s, re.I):
            return num(s)
        return sym[s] if sym else 0

    def rel(s):
        if not sym:
            return 0
        d = val(s) - (pc + 2)
        assert -128 <= d < 128, ins
        return d & 0xFF
    if ins in FIXED:
        return FIXED[ins]
    if ins == '.TEXT':
        return [ord(c) for c in comment] + [0]
    m = re.match(r'^\.BYTE (\w+)$', ins)
    if m:
        return [val(m.group(1))]
    m = re.match(r'^\.ASCII "(.*)"$', ins)
    if m:
        return [ord(c) for c in m.group(1)]
    op, _, args = ins.partition(' ')
    a = [x.strip() for x in args.split(',')] if args else []
    w = lambda v: [v & 0xFF, v >> 8 & 0xFF]
    if op == 'CALL':
        return [0xCD] + w(val(a[0]))
    if op == 'JP':
        return [0xC3] + w(val(a[0]))
    if op == 'JR':
        if len(a) == 2:
            return [0x20 + 8 * CC[a[0]], rel(a[1])]
        return [0x18, rel(a[0])]
    if op == 'DJNZ':
        return [0x10, rel(a[0])]
    if op == 'OUT':
        return [0xD3, num(a[0].strip('()'))]
    if op == 'IN':
        return [0xDB, num(a[1].strip('()'))]
    if op == 'CP':
        return [0xFE, num(a[0])]
    if op == 'LD':
        d, s = a
        if d in RP:
            return [0x01 + 16 * RP[d]] + w(val(s))
        if d == 'A' and s.startswith('(') and s.strip('()') not in R8 and s != '(HL)':
            return [0x3A] + w(val(s.strip('()')))
        if s == 'A' and d.startswith('(') and d != '(HL)':
            return [0x32] + w(val(d.strip('()')))
        if d in R8 and s in R8:
            return [0x40 + 8 * R8[d] + R8[s]]
        if d in R8:
            return [0x06 + 8 * R8[d], num(s)]
    raise ValueError('cannot assemble ' + ins)


def assemble():
    """(the bytes at ORG, {label: address}, [(address, bytes, label, ins, comment)])"""
    sym = {}
    for pass_ in (1, 2):
        pc, out, lines = ORG, [], []
        for label, ins, comment in CODE:
            if label and pass_ == 1:
                sym[label] = pc
            b = encode(ins, comment, pc, sym if pass_ == 2 else None)
            lines.append((pc, b, label, ins, comment))
            out += b
            pc += len(b)
    return bytes(out), sym, lines


def patches(sym):
    """[(address, bytes)] of the two changed instructions in BOOT"""
    res = []
    for addr, op, new, old in PATCHES:
        res.append((addr, bytes(encode('%s %s' % (op, new), '', addr, sym))))
    return res


CLASSIC = {0x1EE2: bytes([0xCD, 0xBA, 0x1E]), 0x1F27: bytes([0xC3, 0x27, 0x1F])}


def patch(micro):
    """SYSTEM.MICRO with the choosing boot (from the classic one or an already patched one)"""
    code, sym, _ = assemble()
    m = bytearray(micro)
    end = ORG + FILE_OFFSET + len(code)
    assert end <= len(m), 'SYSTEM.MICRO is too short'
    region = m[ORG + FILE_OFFSET: end]
    assert not any(region) or bytes(region) == code, 'the end of SYSTEM.MICRO is not free'
    for addr, b in patches(sym):
        cur = bytes(m[addr + FILE_OFFSET: addr + FILE_OFFSET + len(b)])
        assert cur in (CLASSIC[addr], b), 'not the BOOT this patch is for (%04X: %s)' % (addr, cur.hex())
        m[addr + FILE_OFFSET: addr + FILE_OFFSET + len(b)] = b
    m[ORG + FILE_OFFSET: end] = code
    return bytes(m)


def text():
    """BOOTC.TEXT: the source, for the P-System's Z80 assembler's syntax, with the addresses and bytes"""
    code, sym, lines = assemble()
    out = [';BOOTC -- the boot that chooses the operating system (SYSTEM.MICRO, BIGGY)',
           ';',
           ';  SYSTEM.C is booted; a key typed within 2 seconds boots SYSTEM.PASCAL.',
           ';  The interpreter\'s BOOT (U120 BOOT.TEXT) changes in two places:',
           ';']
    for addr, b in patches(sym):
        old = [p for p in PATCHES if p[0] == addr][0]
        oldtgt = old[3] if isinstance(old[3], str) else '$45'
        out.append(';    %04X  %-8s %-8s was %s %s   (%s)' % (addr, old[1], old[2], old[1], oldtgt, b.hex(' ').upper()))
    out += [';',
            ';  and this code is put into the unused end of SYSTEM.MICRO, at %04XH' % ORG,
            ';  (file offset %04XH): above the directory buffer (NRPTOP, 2019H) and' % (ORG + FILE_OFFSET),
            ';  RELSEG (2819H), so it is still there when NOTFND runs.  OUT (2) is the',
            ';  emulator\'s key wait (version 2.08): it returns when a key is typed or',
            ';  after A tenths of a second.  An emulator\'s native P-Code boot does the',
            ';  same when it finds these bytes.',
            '']
    out.append('        .ABSOLUTE')
    out.append('        .PROC   BOOTC')
    for k, v in EQU.items():
        out.append('%-8s.EQU    %04XH' % (k, v))
    out.append('')
    out.append('        .ORG    %04XH' % ORG)
    for addr, b, label, ins, comment in lines:
        if ins == '.TEXT':
            parts = []
            for piece in re.split(r'(\r\n|\r|\n)', comment):
                if piece in ('\r\n', '\r', '\n'):
                    parts += ['0DH', '0AH'] if piece == '\r\n' else ['0DH']
                elif piece:
                    parts.append('"%s"' % piece)
            src = '.ASCII  ' + parts[0] if parts and parts[0].startswith('"') else '.BYTE   ' + parts[0]
            rest = parts[1:] + ['0']
            out.append('%-8s%-40s;%04X' % (label, src, addr))
            for p in rest:
                out.append('        %-40s' % (('.ASCII  ' if p.startswith('"') else '.BYTE   ') + p))
            continue
        op, _, args = ins.partition(' ')
        src = '%-8s%s' % (op, args)
        out.append('%-8s%-40s;%04X  %-12s %s' % (label, src, addr, b.hex(' ').upper() if isinstance(b, bytes) else bytes(b).hex(' ').upper(), comment))
    out.append('')
    out.append('        .END')
    return '\n'.join(l.rstrip() for l in out) + '\n'


def main(args):
    if args[:1] == ['--text']:
        open(args[1], 'w').write(text())
        return 0
    data = open(args[0], 'rb').read()
    open(args[1], 'wb').write(patch(data))
    code, sym, _ = assemble()
    print('%s: the choosing boot, %d bytes at %04X (%s)' % (args[1], len(code), ORG,
                                                         ', '.join('%s %04X' % kv for kv in sorted(sym.items(), key=lambda kv: kv[1]) if not kv[0].startswith('$'))))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
