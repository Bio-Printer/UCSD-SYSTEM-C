# UCSD-SYSTEM-C — the UCSD Pascal II.0 operating system in C

`SYSTEM.PASCAL` is the operating system of the UCSD Pascal II.0 P-System: the
boot loads it, its segment 0 stays in memory while programs run, and every
compiled program calls it for files, strings and the console.  This
repository is a replacement for it written in C and compiled by
[Tiny-C](https://github.com/Bio-Printer/UCSD-TinyC).  The two are
interchangeable: boot `SYSTEM.C` (this one) instead of `SYSTEM.PASCAL` and the
system runs as before, with the Filer, Editor, Compiler, Linker and Assembler,
Pascal and Tiny-C programs, the Tiny-C shell, pexec and sub-volumes.

It is a port of the U134.4 OS source, revision 1.13 (UCSD-Pascal-Volumes'
`U134.4_OS_SOURCE_v1.13`: II.0 plus pexec, `S(hell` and `X(ecute` into
sub-volumes).  The text, the files it writes and the choices it makes are the
Pascal one's.  It needs no newer emulator: any that runs that one runs this.

BIGGY (UCSD-Pascal-Volumes' `Big_Disk.BLK`) holds both, and its SYSTEM.MICRO
has a boot that chooses:

    Type any key within 2 seconds to boot SYSTEM.PASCAL instead of SYSTEM.C: SYSTEM.C

SYSTEM.C unless a key is typed (the key is not passed on); when the one chosen
is not on the disk, the other.  The 2 seconds need emulator 2.08 (its key wait,
`OUT (2)`); an older one does not wait.  The volume SYS-C: (UCSD-Pascal-Volumes'
`BLK_format/SYS-C.BLK`) has the sources, SYSTEM.C, that SYSTEM.MICRO and the
boot's source BOOTC.TEXT.

## Smaller

| | Pascal | C |
|---|---:|---:|
| segment 0 (in memory all the time) | 7,756 bytes | **7,564 bytes** |
| the outer block's data (the globals) | 550 bytes | 550 bytes |
| PRINTERR, INITIALI, GETCMD (read in while they run) | 1,052 + 2,400 + 3,882 | 978 + 2,228 + 3,676 |
| free memory of a program (`memfree`), standard layout and Z80 mode | 21,961 words | **22,057 words** (+96) |
| ... P-Code mode, interpreter memory given back (Reclaim) | 25,876 words | **25,972 words** (+96) |
| ... Reclaim and code in its own I-space (Harvard) | 31,749 words | **31,753 words** (+4) |
| the Pascal Compiler's "[ N words]" compiling a program | 7,057 | **7,153** (+96) |

In the Harvard layout code does not take data memory, so what the operating
system's code saves does not show there; its frames and string constants
(copied to data memory) are no bigger than the Pascal one's.

## Installing it

* Boot BIGGY: it has SYSTEM.C, SYSTEM.PASCAL and the choosing boot.
* On another disk: copy `SYSTEM.C` and `SYSTEM.MICRO` onto it (the emulator's
  Options > Import File, or the Filer's T(ransfer to `*SYSTEM.C` and
  `*SYSTEM.MICRO`), keep its SYSTEM.PASCAL, and boot again.  Or copy `SYSTEM.C`
  over its `SYSTEM.PASCAL` (the old boot then boots it, with no question).
* Built on the P-System: X(ecute TINY-C:CC, `SYS-C:SYSC` makes SYS-C:SYSC.CODE,
  the same as SYSTEM.C.

## What it keeps (what compiled programs depend on)

* **The procedures of segment 0, by number**: 2 EXECERROR (the interpreter
  calls it after an execution error), 3..29 FINIT, FRESET, FOPEN, FCLOSE, FGET,
  FPUT, XSEEK, FEOF, FEOLN, FREADINT, FWRITEINT, XREADREAL, XWRITEREAL,
  FREADCHAR, FWRITECHAR, FREADSTRING, FWRITESTRING, FWRITEBYTES, FREADLN,
  FWRITELN, SCONCAT, SINSERT, SCOPY, SDELETE, SPOS, FBLOCKIO, FGOTOXY (what
  compiled programs call: `CXP 0,n`), 30..43 VOLSEARCH, WRITEDIR, DIRSEARCH,
  SCANTITLE, DELENTRY, INSENTRY, HOMECURSOR, CLEARSCREEN, CLEARLINE, PROMPT,
  SPACEWAIT, GETCHAR, FETCHDIR, COMMAND (what the system programs call), with
  the Pascal parameters: a function's result two words below its parameters,
  a value STRING passed by its address and copied.
* **The global variables at their words** (the system programs were compiled
  with GLOBALS.TEXT and use them directly): SYSCOM 1, GFILES 2, USERINFO 8,
  EMPTYHEAP 54, the console FIBs 55..58, DKVID 59, SYVID 63, THEDATE 67, STATE
  69, PL 70, IPOT 111, FILLER 116, DIGITS 122, UNITABLE 126, FILENAME 216
  (`src/globals.h`, with the Pascal compiler's layout: names declared
  together, `A,B: T`, are laid out last first; the FIB, the directory entry
  and the mark stack likewise).
* **The segments**: USERPROG 1 (a program takes its place), DEBUGGER 2,
  PRINTERR 3, INITIALI 4, GETCMD 5; segment 0 PASCALSY.
* **EXECERROR's frame**: the interpreter sets BOMBP to SP − 14 before it calls
  EXECERROR, where EXECERROR's mark stack is if it has exactly one word of
  locals, as the Pascal one; `BOMBP^.MSIPC := BOMBIPC` then makes it return to
  the failing instruction.  So it has one variable, and `build.py` checks it.
* NIL is the P-machine's: 1, not 0.

## Building

    python3 build.py

compiles `src/sysc.c` with UCSD-TinyC's host compiler (`../ucsd-tinyc`, or
`$TINYC`; it is built with gcc if needed) into `SYSTEM.C`, checks the
numbers, the segments, the globals and EXECERROR's frame (the build fails
without them), and compares the sizes with the Pascal SYSTEM.PASCAL (from
`../UCSD-Pascal-Volumes`).  It also makes `SYSTEM.MICRO` (BIGGY's interpreter with
the choosing boot, `tools/boot.py`; its bytes are checked against the emulator's
copy when `../ucsd-pascal_windows_emulator` is there) and the volume
`build/SYS-C.BLK`.  `tools/psbuild.py` builds it again on the
P-System, with Tiny-C's CC running on this operating system: the code file is
the same, byte for byte.

Tiny-C's `#pragma system` (UCSD-TinyC, `tinyc/tc.h`) makes a module an
operating system: its functions are lex level 0 procedures (called with CBP,
returning with RBP, as the Pascal compiler compiles the system's), its static
variables the outer block's locals (`LOD/STR/LDA 1,n` from the functions),
`main` the outer block itself (procedure 1, lex level −1: the statics its first
locals, then its own; it zeroes them and ends with XIT), a function's result
two words below its parameters, no scratch word (a value is dropped with
`FJP 0`); the linker makes segment 0 of it, numbers the other segments 1..6 and
every procedure in the order of the file (all are kept), and links nothing of
a library.  The C here uses no library at all: no plain `char` (sign
extension is a library call), `__dvi`/`__mdi` (Pascal's DIV and MOD) for `/`
and `%`, no `>>`, no constant over 32767.

## The source

| | |
|---|---|
| `src/sysc.c` | the module: the procedures' declarations (their numbers) and the segments |
| `src/globals.h` | the types (FIB, DIRENTRY, SYSCOMREC, ...), the global variables word for word, the CSPs |
| `src/seg0.c` | segment 0: EXECERROR ... COMMAND in their order (SYSTEM.A, B and C.TEXT), the body (main), then this file's helpers |
| `src/segs.c` | USERPROGRAM, DEBUGGER, PRINTERROR, INITIALIZE, GETCMD and their parts (SYSSEGS.A and B.TEXT) |
| `src/readme.txt` | SYS-C:README.TEXT |
| `build.py` | SYSTEM.C, checked; SYSTEM.MICRO; build/SYS-C.BLK |
| `tools/boot.py` | the choosing boot: assembled, put into SYSTEM.MICRO, its source BOOTC.TEXT |
| `tools/boottest.py` | the boot in every mode and layout |
| `tools/filer.py`, `tools/filertest.py` | SYSTEM.FILER whose K(runch) knows the running system; its test |
| `tools/suite.py` | UCSD-TinyC's tests on this operating system |
| `tools/systest.py` | the system programs on both operating systems: the same transcripts and disks |
| `tools/psbuild.py` | the build on the P-System |

GETCMD's nested Pascal procedures set GETCMD's result and leave it with
`EXIT(GETCMD)`; C has no nested procedures, so they set the global STATE (which
COMMAND keeps the result in anyway) and leave with the same EXIT; until then
STATE is the last command, the Pascal one's LASTST.  INITIALIZE's parts share
its variables through a record.

## Tests

* `tools/suite.py TEST ...`: UCSD-TinyC's tests (`quicktest`, `pexectest`,
  `keytest`, `xsubvoltest`, `selfcompile`, `alltest`, `shellalltest`, the vi
  tests, ...) with SYSTEM.C as the SYSTEM.PASCAL of their boot disks.  All 38 pass, in P-Code
  and in Z80 mode.
* `tools/systest.py`: the Filer (list, transfer, change, remove, krunch, the
  work file), the Compiler, a Pascal program's strings, files (text with blank
  compression, a typed file, a missing one), console input with the delete
  keys, GOTOXY, an execution error, an I/O error, a stack overflow, the Editor,
  the Linker, the Assembler, SYSTEM.STARTUP, the shell, X(ecute, I(nitialize
  and H(alt -- each run on the Pascal operating system and on this one, from the
  same disks: the console transcripts and the disks they leave are the same,
  byte for byte (only the Compiler's free memory differs).  In P-Code mode
  (standard, Reclaim and Harvard layouts) and in Z80 mode.
* `tools/psbuild.py`: built on the P-System, the same code file.
* `tools/boottest.py`: the boot, in P-Code mode (standard, Reclaim, Harvard) and
  Z80 mode: no key boots SYSTEM.C, a key SYSTEM.PASCAL, each when the other is
  missing, the old SYSTEM.MICRO as before; and BOOTC.TEXT assembled by the
  P-System's Assembler is the same code.

## Where it differs

Only where the Pascal one fails:

* FWRITEINT writes −32768 (the Pascal one wrote a wrong number).
* A file name of more than 80 characters given to FOPEN or SCANTITLE (a STRING
  value parameter) is cut to 80, and a code file name of more than 23 at
  C(ompile)'s "To what codefile?" to 23, where the Pascal one stopped with
  "String overflow".
* A new directory entry's word 2 holds only the file's kind (the Pascal one
  left whatever its local had in the bits after it).

## The Filer's K(runch)

The Filer's K(runch) stops with "Please re-boot" when it has moved the running
system: the system reads its segments by their blocks, so they must not move
under it.  BIGGY's Filer up to 1.27 looked for a file named SYSTEM.PASCAL, so
it went on after moving SYSTEM.C while SYSTEM.C ran.  `tools/filer.py` builds
revision 1.14 of the OS and Filer source (UCSD-Pascal-Volumes'
`U134.4_OS_SOURCE_v1.14`: FILER.E.TEXT finds the running system by its segment
0's unit and block, `SYSCOM^.SEGTABLE[0]`) and compiles its II.0.FILER on the
P-System into `SYSTEM.FILER` (BIGGY 1.28).  Compiled the same way, v1.13 gives
BIGGY's old SYSTEM.FILER byte for byte.  `tools/filertest.py` checks it:
SYSTEM.C or SYSTEM.PASCAL running and moved (a reboot, after which the disk boots
again), the other one moved or another disk krunched (no reboot), and the old
Filer.
