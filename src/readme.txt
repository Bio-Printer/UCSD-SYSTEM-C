SYS-C:  THE OPERATING SYSTEM IN C

SYSTEM.C is the UCSD Pascal II.0 operating system written in C: a port of
U134.4_OS_SOURCE_v1.13, the source of BIGGY's SYSTEM.PASCAL.  Programs and
the system programs (Filer, Editor, Compiler, Linker, Assembler, the Tiny-C
shell) cannot tell the two apart: segment 0 has the same procedures with the
same numbers, the global variables are at the same words, the segments are
the same.  Its segment 0, which stays in memory, is 192 bytes smaller, so a
program has 96 words more.

BOOTING

BIGGY holds both, SYSTEM.PASCAL and SYSTEM.C.  Its SYSTEM.MICRO asks

  Type any key within 2 seconds to boot SYSTEM.PASCAL instead of SYSTEM.C:

and boots SYSTEM.C unless a key is typed.  If the one chosen is not on the
boot disk, the other one is booted.  The wait needs the emulator 2.08 or
later; an older one does not wait, and boots SYSTEM.C.

THE FILES

  SYSC.C        the module: the procedures' numbers and the segments
  GLOBALS.H     the types and the global variables (their word offsets)
  SEG0.C        segment 0: EXECERROR .. COMMAND, the outer block
  SEGS.C        USERPROG, DEBUGGER, PRINTERR, INITIALI, GETCMD
  SYSTEM.C      the operating system, compiled (= BIGGY:SYSTEM.C)
  SYSTEM.MICRO  the interpreter with the choosing boot (= BIGGY:SYSTEM.MICRO)
  BOOTC.TEXT    the source of that boot (Z80 assembly), with its bytes
  FILES.TEXT    the list of the files

BUILDING IT

  X(ecute TINY-C:CC    Compile what file? SYS-C:SYSC

makes SYS-C:SYSC.CODE, the same as SYSTEM.C byte for byte (#pragma system
lays it out as SYSTEM.PASCAL is).  To use a new one, copy it with the Filer
to *SYSTEM.C (T(ransfer SYS-C:SYSC.CODE, *SYSTEM.C) and boot again.  Keep
SYSTEM.PASCAL: a key at the boot brings it back.

On the host: https://github.com/Bio-Printer/UCSD-SYSTEM-C (build.py, and the
tests that run both systems and compare them).
