/* system.c -- the UCSD Pascal II.0 operating system (SYSTEM.PASCAL) in C.
 *
 * A replacement for the U134.4 OS (revision 1.13: GETCMD's pexec, the S(hell
 * and X(ecute into sub-volumes) that every program and code file of the
 * system can be run with unchanged: the same procedures with the same
 * numbers and parameters in segment 0 (what compiled programs call: CXP
 * 0,n), the same global variables at the same places (globals.h), the same
 * segments (USERPROGRAM 1, DEBUGGER 2, PRINTERROR 3, INITIALIZE 4, GETCMD 5).
 * It is compiled by Tiny-C (#pragma system: lex level 0 procedures as the
 * Pascal compiler makes for the system, results two words below the
 * parameters as Pascal callers push them, the globals as the outer block's
 * locals); build.py makes SYSTEM.PASCAL of it.
 *
 * Segment 0, procedure 1 is the outer block the boot starts (Tiny-C's
 * startup code: it calls main).  Procedures 2..29 are the run-time support
 * compiled programs call; 30..43 what the system programs (the Filer ...)
 * call; the numbers come from the order of the definitions below -- do not
 * move them.  The rest are this file's own helpers.
 *
 * Pascal's strings are a length byte and the characters (unsigned char
 * s[n+1], s[0] the length); C's string literals are used where the text
 * goes straight to the console (wstr).  The C here uses no library: no
 * plain char (its sign extension is a library call), no / or % (DVI and MODI
 * as Pascal's DIV and MOD: __dvi, __mdi), no >>.
 */
#pragma system
#include "globals.h"

/* ---- segment 0: the fixed procedures (their numbers) ---- */
#pragma segment MAIN                    /* (declared in segment 0: their calls are CBP n, 2 bytes) */
void execerror(void);                                           /*  2 */
void finit(struct fib *f, unsigned char *window, int recwords); /*  3 */
void freset(struct fib *f);                                     /*  4 */
void fopen(struct fib *f, unsigned char *ftitle, int fopenold, int junk);  /*  5 */
void fclose(struct fib *f, int ftype);                          /*  6 */
void fget(struct fib *f);                                       /*  7 */
void fput(struct fib *f);                                       /*  8 */
void xseek(void);                                               /*  9 */
int feof(struct fib *f);                                        /* 10 */
int feoln(struct fib *f);                                       /* 11 */
void freadint(struct fib *f, int *i);                           /* 12 */
void fwriteint(struct fib *f, int i, int rleng);                /* 13 */
void xreadreal(void);                                           /* 14 */
void xwritereal(void);                                          /* 15 */
void freadchar(struct fib *f, int *ch);                         /* 16 */
void fwritechar(struct fib *f, int ch, int rleng);              /* 17 */
void freadstring(struct fib *f, unsigned char *s, int sleng);   /* 18 */
void fwritestring(struct fib *f, unsigned char *s, int rleng);  /* 19 */
void fwritebytes(struct fib *f, unsigned char *a, int rleng, int aleng);  /* 20 */
void freadln(struct fib *f);                                    /* 21 */
void fwriteln(struct fib *f);                                   /* 22 */
void sconcat(unsigned char *dest, unsigned char *src, int destleng);       /* 23 */
void sinsert(unsigned char *src, unsigned char *dest, int destleng, int insinx);  /* 24 */
void scopy(unsigned char *src, unsigned char *dest, int srcinx, int copyleng);   /* 25 */
void sdelete(unsigned char *dest, int delinx, int delleng);     /* 26 */
int spos(unsigned char *target, unsigned char *src);            /* 27 */
int fblockio(struct fib *f, unsigned char *a, int i, int nblocks, int rblock, int doread);  /* 28 */
void fgotoxy(int x, int y);                                     /* 29 */
/* the ones the system programs call */
int volsearch(unsigned char *fvid, int lookhard, struct direntry **fdir);  /* 30 */
void writedir(int funit, struct direntry *fdir);                /* 31 */
int dirsearch(unsigned char *ftid, int findperm, struct direntry *fdir);   /* 32 */
int scantitle(unsigned char *ftitle, unsigned char *fvid, unsigned char *ftid,
              int *fsegs, int *fkind);                          /* 33 */
void delentry(int finx, struct direntry *fdir);                 /* 34 */
void insentry(struct direntry *fentry, int finx, struct direntry *fdir);  /* 35 */
void homecursor(void);                                          /* 36 */
void clearscreen(void);                                         /* 37 */
void clearline(void);                                           /* 38 */
void prompt(void);                                              /* 39 */
int spacewait(int flush);                                       /* 40 */
int getchar(int flush);                                         /* 41 */
int fetchdir(int funit);                                        /* 42 */
void command(void);                                             /* 43 */
/* this file's own */
void xerrmsg(void);
void reseter(struct fib *f);
int cantstretch(struct fib *f);
int checkdel(int ch, int *sinx);
void putprefixed(int which, int commandchar);
int entertemp(unsigned char *ftid, int fsegs, int fkind, struct direntry *fdir);
int peq(unsigned char *a, unsigned char *b);
void pcpy(unsigned char *d, unsigned char *s);
void pset(unsigned char *d, unsigned char *s);
void wstr(unsigned char *s);
void wpstr(unsigned char *s);
void wch(int c);
void wint(int i);
void wln(void);

/* ---- the segments ---- */
#pragma segment USERPROG
void userprogram(struct fib *input, struct fib *output);
#pragma segment DEBUGGER
void debugger(void);
#pragma segment PRINTERR
void printerror(int xeqerr, int iorslt);
#pragma segment INITIALI
void initialize(void);
#pragma segment GETCMD
void getcmd(void);
#pragma segment MAIN

#include "seg0.c"
#include "segs.c"
