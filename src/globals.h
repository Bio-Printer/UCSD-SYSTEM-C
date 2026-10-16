/* globals.h -- the operating system's types and global variables: GLOBALS.TEXT
   of the U134.4 OS source (revision 1.13), laid out word for word as the II.0
   Pascal compiler lays it out, because compiled programs depend on it:

   * the system programs (the Filer, the Editor, the Compiler, the Linker ...)
     were compiled with GLOBALS.TEXT: they read and write these variables at
     their word offsets in the operating system's outer frame (LOD 1,n);
   * every Pascal program reads GFILES (its INPUT and OUTPUT), Tiny-C's library
     SYSCOM, PL, DKVID and SYVID (psys.h);
   * a FIB and a directory entry are passed to and from the system procedures
     by every program that does file I/O.

   The Pascal compiler lays out a list of names declared together ("A,B: T")
   last first: SYVID,DKVID is DKVID then SYVID.  Each such list is written
   here in that order, with the offset (in words) of the Pascal declaration.
   A packed record fills each word from bit 0.  The operating system's
   variables are the locals of its outer block (procedure 1, lex level -1):
   the boot puts the pointer to SYSCOM into word 1 (#pragma system, see
   Tiny-C's tc.h). */

#define NIL ((void *)1)         /* the P-machine's NIL (LDCN) is 1, not 0 */

#define MMAXINT  32767
#define MAXUNIT  14             /* units 1..14 */
#define MAXDIR   77             /* entries in a directory */
#define VIDLENG  7
#define TIDLENG  15
#define FBLKSIZE 512
#define DIRBLK   2              /* a directory's first block */
#define AGELIMIT 300            /* ticks a directory in memory is trusted */
#define EOL      13
#define DLE      16             /* blank compression: DLE, 32 + n blanks */
#define FILL_LEN 11

/* IORSLTWD */
#define INOERROR   0
#define IBADBLOCK  1
#define IBADUNIT   2
#define IBADMODE   3
#define ITIMEOUT   4
#define ILOSTUNIT  5
#define ILOSTFILE  6
#define IBADTITLE  7
#define INOROOM    8
#define INOUNIT    9
#define INOFILE   10
#define IDUPFILE  11
#define INOTCLOSED 12
#define INOTOPEN  13
#define IBADFORMAT 14

/* CMDSTATE (GETCMD) */
#define HALTINIT  0
#define DEBUGCALL 1
#define UPROGNOU  2
#define UPROGUOK  3
#define SYSPROG   4
#define COMPONLY  5
#define COMPANDGO 6
#define COMPDEBUG 7
#define LINKANDGO 8
#define LINKDEBUG 9

/* SYSFILE: the code files GETCMD runs */
#define ASSMBLER 0
#define COMPILER 1
#define EDITOR   2
#define FILER    3
#define LINKER   4

/* FILEKIND */
#define UNTYPEDFILE 0
#define XDSKFILE    1
#define CODEFILE    2
#define TEXTFILE    3
#define INFOFILE    4
#define DATAFILE    5
#define GRAFFILE    6
#define FOTOFILE    7
#define SECUREDIR   8

/* CLOSETYPE */
#define CNORMAL 0
#define CLOCK   1
#define CPURGE  2
#define CCRUNCH 3

/* a FIB's FSTATE */
#define FJANDW    0
#define FNEEDCHAR 1
#define FGOTCHAR  2

/* DIRENTRY, 13 words.  Word 2: DFKIND in bits 0..3, then FILLER1 (entry 0) or
   FILLER2 and STATUS (a file).  A file: DTID (STRING[15]) at byte 6,
   DLASTBYTE word 11, DACCESS word 12 (a DATEREC: MONTH bits 0..3, DAY 4..8,
   YEAR 9..15; YEAR 100 marks a temporary entry).  Entry 0, the volume: DVID
   (STRING[7]) at byte 6, then DEOVBLK, DNUMFILES, DLOADTIME and DLASTBOOT,
   words 7..10 (the D_ macros). */
struct direntry {
    int dfirstblk;
    int dlastblk;
    int dfkind;
    unsigned char dtid[16];
    int dlastbyte;
    int daccess;
};
#define DKIND(e)      ((e)->dfkind & 15)
#define D_EOVBLK(d)   (((int *)(d))[7])
#define D_NUMFILES(d) (((int *)(d))[8])
#define D_LOADTIME(d) (((int *)(d))[9])
#define D_LASTBOOT(d) (((int *)(d))[10])
#define DIRSIZE  2028           /* SIZEOF(DIRECTORY): 78 entries */
#define YEAR100  (-14336)       /* DATEREC YEAR = 100, in place (0xC800; an int, not a long) */
#define ISTEMP(d) (((d) & -512) == YEAR100)     /* (-512: 0xFE00, the YEAR bits) */

/* FIB: a file (FIBP = struct fib *) */
struct fib {
    unsigned char *fwindow;     /*  0 F^ */
    int feoln;                  /*  1 (FEOF,FEOLN) */
    int feof;                   /*  2 */
    int fstate;                 /*  3 */
    int frecsize;               /*  4 bytes: 0 a block file, 1 a file of char */
    int fisopen;                /*  5 */
    int fisblkd;                /*  6 */
    int funit;                  /*  7 */
    unsigned char fvid[8];      /*  8 */
    int fmaxblk;                /* 12 (FREPTCNT,FNXTBLK,FMAXBLK) */
    int fnxtblk;                /* 13 */
    int freptcnt;               /* 14 */
    int fmodified;              /* 15 */
    struct direntry fheader;    /* 16 */
    int fsoftbuf;               /* 29 */
    int fmaxbyte;               /* 30 (FNXTBYTE,FMAXBYTE) */
    int fnxtbyte;               /* 31 */
    int fbufchngd;              /* 32 */
    unsigned char fbuffer[514]; /* 33 (FBLKSIZE+1 bytes) */
};
#define FIBSHORT 30             /* words of NEW(F,TRUE,FALSE): no buffer */

/* INFOREC: USERINFO, the work files */
struct inforec {
    struct fib *codefibp;       /*  8 (SYMFIBP,CODEFIBP) */
    struct fib *symfibp;        /*  9 */
    int errnum;                 /* 10 (ERRSYM,ERRBLK,ERRNUM) */
    int errblk;                 /* 11 */
    int errsym;                 /* 12 */
    int stupid;                 /* 13 (SLOWTERM,STUPID) */
    int slowterm;               /* 14 */
    int altmode;                /* 15 */
    int gotcode;                /* 16 (GOTSYM,GOTCODE) */
    int gotsym;                 /* 17 */
    unsigned char codevid[8];   /* 18 (WORKVID,SYMVID,CODEVID) */
    unsigned char symvid[8];    /* 22 */
    unsigned char workvid[8];   /* 26 */
    unsigned char codetid[16];  /* 30 (WORKTID,SYMTID,CODETID) */
    unsigned char symtid[16];   /* 38 */
    unsigned char worktid[16];  /* 46 */
};

/* the mark stack control word (MSCW) of a frame */
struct mscw {
    struct mscw *statlink;
    struct mscw *dynlink;
    unsigned char *msjtab;      /* the procedure's jump table (byte 0: its number) (MSSEG,MSJTAB) */
    unsigned char *msseg;       /* the segment's procedure dictionary (byte 0: its number) */
    int msipc;
};

/* SYSCOMREC: the system communication area (the interpreter's) */
struct crtctrlrec {
    unsigned char escape, home, eraseeos, eraseeol, ndfs, rlf;
    unsigned char backspace;
    unsigned char fillcount;
    unsigned char clearline, clearscreen;
    unsigned prefixed;          /* bit n: code n is sent after ESCAPE */
};
struct crtinforec {
    int height, width;
    unsigned char up, down, left, right;
    unsigned char eof, flush, brk, stop, chardel, badch;
    unsigned char linedel, altmode;
    unsigned char prefix, etx, alpha_lock;
    unsigned prefixed;
};
struct segentry {
    int codeunit;
    int diskaddr;
    int codeleng;
};
struct syscomrec {
    int iorslt;
    int xeqerr;
    int sysunit;
    int bugstate;
    struct direntry *gdirp;     /* the directory last read (VOLSEARCH, FETCHDIR) */
    struct mscw *bombp, *stkbase, *lastmp;
    int jtab, seg, memtop;
    int bombipc;
    int hltline;
    int brkpts[4];
    int retries;
    int expansion[9];
    unsigned lowtime, hightime;
    unsigned miscinfo;
    int crttype;
    struct crtctrlrec crtctrl;
    struct crtinforec crtinfo;
    struct segentry segtable[16];
};
#define MI_HASCLOCK   0x0001
#define MI_HAS8510A   0x0002
#define MI_HASXYCRT   0x0008
#define MI_SLOWTERM   0x0010
#define MI_STUPID     0x0020
#define MI_USERKIND   0x0180    /* NORMAL 0, AQUIZ 1, BOOKER 2, PQUIZ 3 (bits 7, 8) */
#define UK_AQUIZ      0x0080
#define UK_BOOKER     0x0100
#define UK_PQUIZ      0x0180

/* UNITABLE[unit] */
struct unitent {
    unsigned char uvid[8];
    int uisblkd;
    int ueovblk;
};

/* ---- the global variables: the outer block's words 1..275 ---- */
static struct syscomrec *syscom;        /*   1 (the boot's ^SYSCOM) */
static struct fib *gfiles[6];           /*   2 0 INPUT, 1 OUTPUT */
static struct inforec userinfo;         /*   8 */
static int *emptyheap;                  /*  54 the heap's mark (NIL: halt) */
static struct fib *swapfib;             /*  55 (INPUTFIB,OUTPUTFIB,SYSTERM,SWAPFIB) */
static struct fib *systerm;             /*  56 */
static struct fib *outputfib;           /*  57 */
static struct fib *inputfib;            /*  58 */
static unsigned char dkvid[8];          /*  59 the prefix (SYVID,DKVID) */
static unsigned char syvid[8];          /*  63 the boot volume */
static int thedate;                     /*  67 a DATEREC */
static int *debuginfo;                  /*  68 */
static int state;                       /*  69 CMDSTATE */
static unsigned char pl[82];            /*  70 STRING: the prompt line */
static int ipot[5];                     /* 111 1, 10, 100, 1000, 10000 */
static unsigned char filler[12];        /* 116 STRING[11]: NULs after a screen code */
static int digits[4];                   /* 122 SET OF '0'..'9' */
static struct unitent unitable[15];     /* 126 UNITABLE[0..MAXUNIT] */
static unsigned char filename[5][24];   /* 216 FILE_TABLE: STRING[23] */
                                        /* 276 */

/* the interpreter's standard procedures (CSP n) */
#define MOVELEFT(s, d, n)   __cspv(2, (s), 0, (d), 0, (n))
#define MOVERIGHT(s, d, n)  __cspv(3, (s), 0, (d), 0, (n))
#define EXITPROC(seg, proc) __cspv(4, (seg), (proc))
#define UNITREAD(u, b, n, blk)  __cspv(5, (u), (b), 0, (n), (blk), 0)
#define UNITWRITE(u, b, n, blk) __cspv(6, (u), (b), 0, (n), (blk), 0)
#define TIME(hi, lo)        __cspv(9, (hi), (lo))
#define FILLCHAR(d, n, c)   __cspv(10, (d), 0, (n), (c))
#define SCANEQ(n, c, b)     __cspi(11, (n), 0, (c), (b), 0, 0)
#define SCANNE(n, c, b)     __cspi(11, (n), 1, (c), (b), 0, 0)
#define NEW(pp, words)      __cspv(1, (pp), (words))
#define MARK(pp)            __cspv(32, (pp))
#define RELEASE(pp)         __cspv(33, (pp))
#define IORESULT()          __cspi(34)
#define UNITBUSY(u)         __cspi(35, (u))
#define UNITCLEAR(u)        __cspv(38, (u))
#define HALT()              __cspv(39)

/* the segments and procedures EXIT names: COMMAND is segment 0 procedure
   43, GETCMD segment 5 procedure 1 */
#define P_COMMAND 43
#define S_GETCMD  5
