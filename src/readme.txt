SYS-C:  THE OPERATING SYSTEM IN C

SYSTEM.C is the UCSD Pascal II.0 operating system written in C: a port of
U134.4_OS_SOURCE_v1.13 with v1.15's change (a new NAME.BAS, a BASIC
source, is a text file) and v1.16's (a file's entry gets the time of day it
was written, from emulator 2.13: see TIMES below), the source of BIGGY's
SYSTEM.PASCAL.  Programs and
the system programs (Filer, Editor, Compiler, Linker, Assembler, the Tiny-C
shell) cannot tell the two apart: segment 0 has the same procedures with the
same numbers, the global variables are at the same words, the segments are
the same.  Its segment 0, which stays in memory, is 142 bytes smaller, so a
program has 71 words more.

TIMES

FCLOSE (SEG0.C) gives a file it writes the time of day with its date:
seconds since midnight + 1, its low 11 bits in DFKIND's bits 4..14 and its
high 6 in DLASTBYTE's bits 10..15 (DLASTBYTE is the low 10: LASTBYTE in
GLOBALS.H).  The emulator gives the time through TIME (both variables 25611)
and today's date (both 25612), which THEDATE follows.  Do not boot an older
SYSTEM.C (BIGGY 1.27 to 1.33) with disks written by this one: it takes an
entry with a time for a bad one and removes it.

BOOTING

BIGGY holds both, SYSTEM.PASCAL and SYSTEM.C.  Its SYSTEM.MICRO asks

  Type any key within 2 seconds to boot SYSTEM.PASCAL instead of SYSTEM.C:

and boots SYSTEM.C unless a key is typed.  If the one chosen is not on the
boot disk, the other one is booted.  The wait needs the emulator 2.08 or
later; an older one does not wait, and boots SYSTEM.C.

THE FILES

  MAKEFILE.TEXT MAKE -f SYS-C:MAKEFILE makes SYSTEM.C (TINY-C:MAKEFILE too)
  SYSC.C        the module: the procedures' numbers and the segments
  GLOBALS.H     the types and the global variables (their word offsets)
  SEG0.C        segment 0: EXECERROR .. COMMAND, the outer block
  SEGS.C        USERPROG, DEBUGGER, PRINTERR, INITIALI, GETCMD
  SYSTEM.C      the operating system, compiled (= BIGGY:SYSTEM.C)
  SYSTEM.MICRO  the interpreter with the choosing boot (= BIGGY:SYSTEM.MICRO)
  BOOTC.TEXT    the source of that boot (Z80 assembly), with its bytes
  FILES.TEXT    the list of the files

BUILDING IT

  MAKE -f SYS-C:MAKEFILE    (from the shell; TINY-C: and TOOLS: on line)

compiles SYSC.C when it or one of the files it includes is newer than
SYSC.OBJ (beside it) and links SYS-C:SYSTEM.C, the same as BIGGY's byte for
byte (#pragma system lays it out as SYSTEM.PASCAL is; the dot in -L
SYS-C:SYSTEM.C.= makes that the name exactly); MAKE -B -f SYS-C:MAKEFILE
does both again.  MAKE -f TINY-C:MAKEFILE, which rebuilds the compiler, the
library and the tools, builds it too.  To
use a new one, copy it with the Filer to *SYSTEM.C (T(ransfer
SYS-C:SYSTEM.C, *SYSTEM.C) and boot again.  Keep SYSTEM.PASCAL: a key at the
boot brings it back.

On the host: https://github.com/Bio-Printer/UCSD-SYSTEM-C (build.py, and the
tests that run both systems and compare them).
