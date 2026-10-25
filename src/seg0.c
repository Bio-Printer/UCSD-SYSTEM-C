/* seg0.c -- segment 0, the resident operating system: procedures 2..43 in
   their order (SYSTEM.A, SYSTEM.B and SYSTEM.C.TEXT of the Pascal one), then
   the helpers of this file.  Each says what of the Pascal it is. */

/* MOVELEFT(SRC[SI], DST[DI], N) as the Pascal compiler passes it */
#define MVL(s, si, d, di, n) __cspv(2, (s), (si), (d), (di), (n))
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')

/* ---- 2 EXECERROR: the interpreter calls it (CXP 0,2) after an execution
   error, SYSCOM^.XEQERR the reason.  It has set BOMBP where this procedure's
   mark stack will be -- SP - 14: it takes this procedure to have one word of
   locals, as the Pascal one has (the build checks it), so sc is its only
   variable; xerrmsg does the rest.  BOMBP^.MSIPC := BOMBIPC makes it return
   (the debugger, or ESC at "Type <space>") to the instruction that failed ---- */
void execerror(void)
{
    struct syscomrec *sc;
    sc = syscom;
    if (sc->xeqerr == 4) {                      /* stack overflow: nothing more fits */
        RELEASE(&emptyheap);
        pset(pl, "*STK OFLOW*");
        UNITWRITE(2, pl + 1, pl[0], 0);
        EXITPROC(0, P_COMMAND);
    }
    sc->bombp->msipc = sc->bombipc;
    if (sc->bugstate != 0) {
        debugger();
        sc->xeqerr = 0;
    } else
        xerrmsg();
}

/* ---- 3 FINIT: a file variable's FIB at the start of its block; RECWORDS
   0 a text file (INTERACTIVE: -2), < 0 an untyped file, else its record
   size in words ---- */
void finit(struct fib *f, unsigned char *window, int recwords)
{
    f->fstate = FJANDW;
    f->fisopen = 0;
    f->feof = 1;
    f->feoln = 1;
    f->fwindow = window;
    if (recwords == 0 || recwords == -2) {
        window[1] = 0;
        f->frecsize = 1;
        if (recwords == 0)
            f->fstate = FNEEDCHAR;
    } else if (recwords < 0) {
        f->fwindow = NIL;
        f->frecsize = 0;
    } else
        f->frecsize = recwords + recwords;
}

/* ---- 4 FRESET: RESET(F) ---- */
void freset(struct fib *f)
{
    syscom->iorslt = INOERROR;
    if (f->fisopen) {
        reseter(f);
        if (f->frecsize > 0) {
            if (f->fstate == FJANDW)
                fget(f);
            else
                f->fstate = FNEEDCHAR;
        }
    }
}

/* ---- 5 FOPEN: RESET(F, NAME) (FOPENOLD) and REWRITE(F, NAME) ---- */
void fopen(struct fib *f, unsigned char *ftitle, int fopenold, int junk)
{
    struct direntry *ldir;
    struct direntry *h;
    struct direntry *e;
    struct unitent *u;
    struct syscomrec *sc;
    int *oldheap;
    int lunit;
    int linx;
    int lsegs;
    int nbytes;
    int lkind;
    int swapped;
    int saverslt;
    unsigned char lvid[8];
    unsigned char ltid[16];
    sc = syscom;
    sc->iorslt = INOERROR;
    if (f->fisopen) {
        sc->iorslt = INOTCLOSED;
        return;
    }
    if (!scantitle(ftitle, lvid, ltid, &lsegs, &lkind)) {
        sc->iorslt = IBADTITLE;
        return;
    }
    if (fopenold > 1)                           /* old code's special case */
        fopenold = fopenold == 2 || fopenold == 4;
    /* the compiler's swap file open and no directory in memory: if the heap
       above EMPTYHEAP holds one's room, it goes to the swap file while the
       directory is read there (and comes back at the end) */
    swapped = 0;
    if (swapfib->fisopen && sc->gdirp == NIL) {
        MARK(&oldheap);
        nbytes = (int)sc->lastmp - (int)oldheap;
        if (nbytes > 0 && nbytes < DIRSIZE + 400) {
            nbytes = (int)oldheap - (int)emptyheap;
            if (nbytes > 0 && nbytes > DIRSIZE && peq(unitable[swapfib->funit].uvid, swapfib->fvid)) {
                UNITWRITE(swapfib->funit, emptyheap, DIRSIZE, swapfib->fheader.dfirstblk);
                RELEASE(&emptyheap);
                swapped = 1;
            }
        }
    }
    lunit = volsearch(lvid, 1, &ldir);
    if (lunit == 0)
        sc->iorslt = INOUNIT;
    else {
        u = &unitable[lunit];
        h = &f->fheader;
        f->fisopen = 1;
        f->fmodified = 0;
        f->funit = lunit;
        pcpy(f->fvid, lvid);
        f->fnxtblk = 0;
        f->fisblkd = u->uisblkd;
        f->fsoftbuf = u->uisblkd && f->frecsize != 0;
        if (ldir != NIL && ltid[0] > 0) {       /* look up or enter the file */
            linx = dirsearch(ltid, fopenold, ldir);
            if (fopenold) {
                if (linx == 0) {
                    sc->iorslt = INOFILE;
                    goto out;
                }
                *h = ldir[linx];
            } else {
                if (linx > 0) {
                    sc->iorslt = IDUPFILE;
                    goto out;
                }
                if (lkind == UNTYPEDFILE)
                    lkind = DATAFILE;
                linx = entertemp(ltid, lsegs, lkind, ldir);
                if (linx > 0 && lkind == TEXTFILE) {
                    e = &ldir[linx];
                    if ((e->dlastblk - e->dfirstblk) & 1)
                        e->dlastblk--;
                    if (e->dlastblk - e->dfirstblk < 4) {
                        delentry(linx, ldir);
                        linx = 0;
                    }
                }
                if (linx == 0) {
                    sc->iorslt = INOROOM;
                    goto out;
                }
                *h = ldir[linx];
                f->fmodified = 1;
                writedir(lunit, ldir);
            }
        } else {                                /* the unit itself: a dummy header */
            if (fopenold && ltid[0] != 0) {
                sc->iorslt = INOFILE;
                goto out;
            }
            h->dfirstblk = 0;
            h->dlastblk = MMAXINT;
            if (u->uisblkd)
                h->dlastblk = u->ueovblk;
            h->dfkind = (h->dfkind & -16) | lkind;
            h->dtid[0] = 0;
            h->dlastbyte = FBLKSIZE;
            h->daccess = 0;
        }
        if (fopenold)
            f->fmaxblk = h->dlastblk - h->dfirstblk;
        else
            f->fmaxblk = 0;
        if (f->fsoftbuf) {
            f->fnxtbyte = FBLKSIZE;
            f->fbufchngd = 0;
            if (fopenold)
                f->fmaxbyte = h->dlastbyte;
            else
                f->fmaxbyte = FBLKSIZE;
            if (DKIND(h) == TEXTFILE) {
                f->fnxtblk = 2;
                if (!fopenold) {                /* a new text file: its header page empty */
                    FILLCHAR(f->fbuffer, 514, 0);
                    UNITWRITE(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk);
                    UNITWRITE(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + 1);
                }
            }
        }
        if (fopenold)
            freset(f);
        else
            reseter(f);                         /* no GET */
out:
        if (sc->iorslt != INOERROR) {
            f->fisopen = 0;
            f->feof = 1;
            f->feoln = 1;
        }
    }
    if (swapped) {
        RELEASE(&oldheap);
        sc->gdirp = NIL;
        saverslt = sc->iorslt;
        UNITREAD(swapfib->funit, emptyheap, DIRSIZE, swapfib->fheader.dfirstblk);
        sc->iorslt = saverslt;
    }
}

/* ---- 6 FCLOSE: CLOSE(F, NORMAL / LOCK / PURGE / CRUNCH) ---- */
void fclose(struct fib *f, int ftype)
{
    struct direntry *ldir;
    struct direntry *h;
    struct direntry *e;
    int linx;
    int dupinx;
    int found;
    syscom->iorslt = INOERROR;
    if (!f->fisopen || f->fwindow == systerm->fwindow)
        return;
    h = &f->fheader;
    if (f->fisblkd && h->dtid[0] > 0) {         /* a file in a directory */
        if (ftype == CCRUNCH) {
            f->fmaxblk = f->fnxtblk;
            h->daccess = (h->daccess & 0x1FF) | YEAR100;
            ftype = CLOCK;
            if (f->fsoftbuf)
                f->fmaxbyte = f->fnxtbyte;
        }
        reseter(f);
        if (f->fmodified || ISTEMP(h->daccess) || ftype == CPURGE) {
            if (f->funit != volsearch(f->fvid, 0, &ldir)) {
                syscom->iorslt = ILOSTUNIT;
                goto out;
            }
            linx = 1;
            found = 0;
            while (linx <= D_NUMFILES(ldir) && !found) {    /* by its blocks */
                e = &ldir[linx];
                found = e->dfirstblk == h->dfirstblk && e->dlastblk == h->dlastblk;
                linx++;
            }
            if (!found) {
                syscom->iorslt = ILOSTFILE;
                goto out;
            }
            linx--;
            e = &ldir[linx];
            if ((ftype == CNORMAL && ISTEMP(e->daccess)) || ftype == CPURGE)
                delentry(linx, ldir);           /* gone */
            else {                              /* a permanent entry */
                dupinx = dirsearch(h->dtid, 1, ldir);
                if (dupinx != 0 && dupinx != linx) {    /* the old file of that name goes */
                    delentry(dupinx, ldir);
                    if (dupinx < linx)
                        linx--;
                }
                e = &ldir[linx];
                if (ISTEMP(e->daccess)) {
                    if (ISTEMP(h->daccess))
                        h->daccess = thedate;   /* (else the Filer's special case) */
                } else if (f->fmodified && (thedate & 15) != 0)
                    h->daccess = thedate;
                else
                    h->daccess = e->daccess;
                h->dlastblk = h->dfirstblk + f->fmaxblk;
                if (f->fsoftbuf)
                    h->dlastbyte = f->fmaxbyte;
                h->dfkind = h->dfkind & 15;     /* FILLER1 := 0 */
                f->fmodified = 0;
                *e = *h;
            }
            writedir(f->funit, ldir);
        }
    }
    if (ftype == CPURGE && h->dtid[0] == 0)
        unitable[f->funit].uvid[0] = 0;
out:
    f->feof = 1;
    f->feoln = 1;
    f->fisopen = 0;
}

/* ---- 7 FGET: GET(F) ---- */
void fget(struct fib *f)
{
    struct direntry *h;
    int leftoget;
    int wininx;
    int leftinbuf;
    int amount;
    syscom->iorslt = INOERROR;
    if (!f->fisopen) {
        syscom->iorslt = INOTOPEN;
        goto eof;
    }
    if (f->freptcnt > 0) {                      /* blanks of a DLE still to come */
        f->freptcnt--;
        if (f->freptcnt > 0)
            return;
    }
    h = &f->fheader;
    if (f->fsoftbuf) {
        leftoget = f->frecsize;
        wininx = 0;
        for (;;) {
            if (f->fnxtblk == f->fmaxblk) {
                if (f->fnxtbyte + leftoget > f->fmaxbyte)
                    goto eof;
                leftinbuf = h->dlastbyte - f->fnxtbyte;
            } else
                leftinbuf = FBLKSIZE - f->fnxtbyte;
            amount = leftoget;
            if (amount > leftinbuf)
                amount = leftinbuf;
            if (amount > 0) {
                MVL(f->fbuffer, f->fnxtbyte, f->fwindow, wininx, amount);
                f->fnxtbyte = f->fnxtbyte + amount;
                wininx = wininx + amount;
                leftoget = leftoget - amount;
            }
            if (leftoget == 0)
                break;
            if (f->fbufchngd) {
                f->fbufchngd = 0;
                f->fmodified = 1;
                UNITWRITE(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + f->fnxtblk - 1);
            }
            if (IORESULT() != INOERROR)
                goto eof;
            UNITREAD(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + f->fnxtblk);
            if (IORESULT() != INOERROR)
                goto eof;
            f->fnxtblk++;
            f->fnxtbyte = 0;
        }
    } else {
        UNITREAD(f->funit, f->fwindow, f->frecsize, 0);
        if (IORESULT() != INOERROR)
            goto eof;
    }
    if (f->frecsize == 1) {                     /* a file of char */
        f->feoln = 0;
        if (f->fstate != FJANDW)
            f->fstate = FGOTCHAR;
        if (f->fwindow[0] == EOL) {
            f->fwindow[0] = ' ';
            f->feoln = 1;
            return;
        }
        if (f->fwindow[0] == DLE) {
            fget(f);
            amount = f->fwindow[0] - 32;
            if (amount > 0 && amount <= 127) {
                f->fwindow[0] = ' ';
                f->freptcnt = amount;
                return;
            }
            fget(f);
        }
        if (f->fwindow[0] == 0) {               /* the end */
            if (f->fsoftbuf && DKIND(h) == TEXTFILE) {  /* of a 2-block page */
                if (f->fnxtblk & 1)
                    f->fnxtblk++;
                f->fnxtbyte = FBLKSIZE;
                fget(f);
            } else {
                f->fwindow[0] = ' ';
                goto eof;
            }
        }
    }
    return;
eof:
    f->feof = 1;
    f->feoln = 1;
}

/* ---- 8 FPUT: PUT(F) ---- */
void fput(struct fib *f)
{
    struct direntry *h;
    int leftoput;
    int wininx;
    int leftinbuf;
    int amount;
    syscom->iorslt = INOERROR;
    if (!f->fisopen) {
        syscom->iorslt = INOTOPEN;
        goto eof;
    }
    if (f->fsoftbuf) {
        h = &f->fheader;
        leftoput = f->frecsize;
        wininx = 0;
        for (;;) {
            if (h->dfirstblk + f->fnxtblk == h->dlastblk) {
                if (f->fnxtbyte + leftoput > h->dlastbyte) {
                    if (cantstretch(f)) {
                        syscom->iorslt = INOROOM;
                        goto eof;
                    }
                    leftinbuf = FBLKSIZE - f->fnxtbyte;
                } else
                    leftinbuf = h->dlastbyte - f->fnxtbyte;
            } else
                leftinbuf = FBLKSIZE - f->fnxtbyte;
            amount = leftoput;
            if (amount > leftinbuf)
                amount = leftinbuf;
            if (amount > 0) {
                f->fbufchngd = 1;
                MVL(f->fwindow, wininx, f->fbuffer, f->fnxtbyte, amount);
                f->fnxtbyte = f->fnxtbyte + amount;
                wininx = wininx + amount;
                leftoput = leftoput - amount;
            }
            if (leftoput == 0)
                break;
            if (f->fbufchngd) {
                f->fbufchngd = 0;
                f->fmodified = 1;
                UNITWRITE(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + f->fnxtblk - 1);
            }
            if (IORESULT() != INOERROR)
                goto eof;
            if (f->fnxtblk < f->fmaxblk)
                UNITREAD(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + f->fnxtblk);
            else
                FILLCHAR(f->fbuffer, FBLKSIZE, 0);
            if (IORESULT() != INOERROR)
                goto eof;
            f->fnxtblk++;
            f->fnxtbyte = 0;
        }
        /* a line end near the end of a text page: the page ends there */
        if (f->frecsize == 1 && f->fwindow[0] == EOL && DKIND(h) == TEXTFILE &&
            f->fnxtbyte >= FBLKSIZE - 127 && !(f->fnxtblk & 1)) {
            f->fnxtbyte = FBLKSIZE - 1;
            f->fwindow[0] = 0;
            fput(f);
        }
    } else {
        UNITWRITE(f->funit, f->fwindow, f->frecsize, 0);
        if (IORESULT() != INOERROR)
            goto eof;
    }
    return;
eof:
    f->feof = 1;
    f->feoln = 1;
}

/* ---- 9 XSEEK: not in II.0 ---- */
void xseek(void)
{
    syscom->xeqerr = 11;                        /* unimplemented instruction */
    execerror();
}

/* ---- 10 FEOF, 11 FEOLN ---- */
int feof(struct fib *f)
{
    return f->feof;
}

int feoln(struct fib *f)
{
    return f->feoln;
}

/* ---- 12 FREADINT: READ(F, I) ---- */
void freadint(struct fib *f, int *i)
{
    int ch;
    int neg;
    int ivalid;
    int sinx;
    *i = 0;
    neg = 0;
    ivalid = 0;
    if (f->fstate == FNEEDCHAR)
        fget(f);
    while (f->fwindow[0] == ' ' && !f->feof)
        fget(f);
    if (f->feof)
        return;
    ch = f->fwindow[0];
    if (ch == '+' || ch == '-') {
        neg = ch == '-';
        fget(f);
        ch = f->fwindow[0];
    }
    if (ISDIGIT(ch)) {
        ivalid = 1;
        sinx = 1;
        do {
            *i = *i * 10 + ch - '0';
            fget(f);
            ch = f->fwindow[0];
            sinx++;
            if (f->funit == 1)
                while (checkdel(ch, &sinx)) {
                    if (sinx == 1)
                        *i = 0;
                    else
                        *i = __dvi(*i, 10);
                    fget(f);
                    ch = f->fwindow[0];
                }
        } while (ISDIGIT(ch) && !f->feoln);
    }
    if (ivalid || f->feof) {
        if (neg)
            *i = -*i;
    } else
        syscom->iorslt = IBADFORMAT;
}

/* ---- 13 FWRITEINT: WRITE(F, I:RLENG) ---- */
void fwriteint(struct fib *f, int i, int rleng)
{
    unsigned char s[12];
    int pot;
    int col;
    int ch;
    int suppressing;
    col = 1;
    suppressing = 1;
    if (i < 0) {
        if (i == -32767 - 1) {                  /* (no positive of it) */
            pset(s, "-32768");
            goto put;
        }
        i = -i;
        s[1] = '-';
        col = 2;
    }
    for (pot = 4; pot >= 0; pot--) {
        ch = __dvi(i, ipot[pot]) + '0';
        if (ch != '0' || pot == 0 || !suppressing) {
            suppressing = 0;
            s[col] = ch;
            col++;
            if (ch != '0')
                i = __mdi(i, ipot[pot]);
        }
    }
    s[0] = col - 1;
put:
    if (rleng < s[0])
        rleng = s[0];
    fwritestring(f, s, rleng);
}

/* ---- 14 XREADREAL, 15 XWRITEREAL: not in this system ---- */
void xreadreal(void)
{
    syscom->xeqerr = 11;
    execerror();
}

void xwritereal(void)
{
    syscom->xeqerr = 11;
    execerror();
}

/* ---- 16 FREADCHAR: READ(F, CH) ---- */
void freadchar(struct fib *f, int *ch)
{
    syscom->iorslt = INOERROR;
    if (f->fstate == FNEEDCHAR)
        fget(f);
    *ch = f->fwindow[0];
    if (f->fstate == FJANDW)
        fget(f);
    else
        f->fstate = FNEEDCHAR;
}

/* ---- 17 FWRITECHAR: WRITE(F, CH:RLENG) ---- */
void fwritechar(struct fib *f, int ch, int rleng)
{
    if (!f->fisopen) {
        syscom->iorslt = INOTOPEN;
        return;
    }
    for (;;) {
        f->fwindow[0] = rleng > 1 ? ' ' : ch;
        if (f->fsoftbuf)
            fput(f);
        else
            UNITWRITE(f->funit, f->fwindow, 1, 0);
        if (rleng <= 1)
            return;
        rleng--;
    }
}

/* ---- 18 FREADSTRING: READ(F, S) (S a STRING[SLENG]) ---- */
void freadstring(struct fib *f, unsigned char *s, int sleng)
{
    int sinx;
    int ch;
    sinx = 1;
    if (f->fstate == FNEEDCHAR)
        fget(f);
    s[0] = sleng;
    while (sinx <= sleng && !(f->feoln || f->feof)) {
        ch = f->fwindow[0];
        if (f->funit != 1 || !checkdel(ch, &sinx)) {    /* the console: line editing */
            s[sinx] = ch;
            sinx++;
        }
        fget(f);
    }
    s[0] = sinx - 1;
    while (!f->feoln)
        fget(f);
}

/* ---- 19 FWRITESTRING: WRITE(F, S:RLENG) ---- */
void fwritestring(struct fib *f, unsigned char *s, int rleng)
{
    int sinx;
    if (!f->fisopen) {
        syscom->iorslt = INOTOPEN;
        return;
    }
    if (rleng <= 0)
        rleng = s[0];
    if (rleng > s[0]) {
        fwritechar(f, ' ', rleng - s[0]);
        rleng = s[0];
    }
    if (f->fsoftbuf) {
        sinx = 1;
        while (sinx <= rleng && !f->feof) {
            f->fwindow[0] = s[sinx];
            fput(f);
            sinx++;
        }
    } else
        UNITWRITE(f->funit, s + 1, rleng, 0);
}

/* ---- 20 FWRITEBYTES: WRITE(F, A:RLENG), A a PACKED ARRAY [1..ALENG] OF CHAR ---- */
void fwritebytes(struct fib *f, unsigned char *a, int rleng, int aleng)
{
    int ainx;
    if (!f->fisopen) {
        syscom->iorslt = INOTOPEN;
        return;
    }
    if (rleng > aleng) {
        fwritechar(f, ' ', rleng - aleng);
        rleng = aleng;
    }
    if (f->fsoftbuf) {
        ainx = 0;
        while (ainx < rleng && !f->feof) {
            f->fwindow[0] = a[ainx];
            fput(f);
            ainx++;
        }
    } else
        UNITWRITE(f->funit, a, rleng, 0);
}

/* ---- 21 FREADLN: READLN(F) ---- */
void freadln(struct fib *f)
{
    while (!f->feoln)
        fget(f);
    if (f->fstate == FJANDW)
        fget(f);
    else {
        f->fstate = FNEEDCHAR;
        f->feoln = 0;
    }
}

/* ---- 22 FWRITELN: WRITELN(F) ---- */
void fwriteln(struct fib *f)
{
    f->fwindow[0] = EOL;
    fput(f);
}

/* ---- 23 SCONCAT: DEST := CONCAT(DEST, SRC), DEST a STRING[DESTLENG] ---- */
void sconcat(unsigned char *dest, unsigned char *src, int destleng)
{
    if (src[0] + dest[0] <= destleng) {
        MVL(src, 1, dest, dest[0] + 1, src[0]);
        dest[0] = src[0] + dest[0];
    }
}

/* ---- 24 SINSERT: INSERT(SRC, DEST, INSINX) ---- */
void sinsert(unsigned char *src, unsigned char *dest, int destleng, int insinx)
{
    int onright;
    if (insinx > 0 && src[0] > 0 && src[0] + dest[0] <= destleng) {
        onright = dest[0] - insinx + 1;
        if (onright > 0) {
            __cspv(3, dest, insinx, dest, insinx + src[0], onright);    /* MOVERIGHT */
            onright = 0;
        }
        if (onright == 0) {
            MVL(src, 1, dest, insinx, src[0]);
            dest[0] = dest[0] + src[0];
        }
    }
}

/* ---- 25 SCOPY: DEST := COPY(SRC, SRCINX, COPYLENG) ---- */
void scopy(unsigned char *src, unsigned char *dest, int srcinx, int copyleng)
{
    dest[0] = 0;
    if (srcinx > 0 && copyleng > 0 && srcinx + copyleng - 1 <= src[0]) {
        MVL(src, srcinx, dest, 1, copyleng);
        dest[0] = copyleng;
    }
}

/* ---- 26 SDELETE: DELETE(DEST, DELINX, DELLENG) ---- */
void sdelete(unsigned char *dest, int delinx, int delleng)
{
    int onright;
    if (delinx > 0 && delleng > 0) {
        onright = dest[0] - delinx - delleng + 1;
        if (onright == 0)
            dest[0] = delinx - 1;
        else if (onright > 0) {
            MVL(dest, delinx + delleng, dest, delinx, onright);
            dest[0] = dest[0] - delleng;
        }
    }
}

/* ---- 27 SPOS: POS(TARGET, SRC) ---- */
int spos(unsigned char *target, unsigned char *src)
{
    int temploc;
    int dist;
    int n;
    int k;
    n = target[0];
    if (n == 0)
        return 0;
    temploc = 1;
    dist = src[0] - n + 1;
    while (temploc <= dist) {
        temploc = temploc + SCANEQ(dist - temploc, target[1], src + temploc);
        if (temploc > dist)
            return 0;
        k = n;
        while (k > 0 && src[temploc + k - 1] == target[k])
            k--;
        if (k == 0)
            return temploc;
        temploc++;
    }
    return 0;
}

/* ---- 28 FBLOCKIO: BLOCKREAD and BLOCKWRITE: NBLOCKS blocks to A[I] from
   block RBLOCK of the file (< 0: the next); the blocks done ---- */
int fblockio(struct fib *f, unsigned char *a, int i, int nblocks, int rblock, int doread)
{
    struct direntry *h;
    syscom->iorslt = INOERROR;
    if (!f->fisopen || nblocks < 0) {
        syscom->iorslt = INOTOPEN;
        return 0;
    }
    a = a + i;
    if (f->fisblkd) {
        h = &f->fheader;
        if (rblock < 0)
            rblock = f->fnxtblk;
        rblock = h->dfirstblk + rblock;
        if (rblock + nblocks > h->dlastblk && !doread)
            cantstretch(f);
        if (rblock + nblocks > h->dlastblk)
            nblocks = h->dlastblk - rblock;
        f->feof = rblock >= h->dlastblk;
        if (f->feof)
            return 0;
        if (doread)
            UNITREAD(f->funit, a, nblocks * FBLKSIZE, rblock);
        else {
            f->fmodified = 1;
            UNITWRITE(f->funit, a, nblocks * FBLKSIZE, rblock);
        }
        rblock = rblock + nblocks;
        f->feof = rblock == h->dlastblk;
        f->fnxtblk = rblock - h->dfirstblk;
        if (f->fnxtblk > f->fmaxblk)
            f->fmaxblk = f->fnxtblk;
        return nblocks;
    }
    if (doread)
        UNITREAD(f->funit, a, nblocks * FBLKSIZE, rblock);
    else
        UNITWRITE(f->funit, a, nblocks * FBLKSIZE, rblock);
    if (IORESULT() != INOERROR)
        return 0;
    if (doread) {                               /* a unit read: the blocks up to the last non-NUL byte */
        rblock = nblocks * FBLKSIZE;
        rblock = rblock + SCANNE(-rblock, 0, a + rblock - 1);
        rblock = __dvi(rblock + FBLKSIZE - 1, FBLKSIZE);
        f->feof = rblock < nblocks;
        return rblock;
    }
    return nblocks;
}

/* ---- 29 FGOTOXY: GOTOXY(X, Y): the emulator's 132 x 45 screen, CHR(1) X Y
   (MYGOTOXY.TEXT's) ---- */
void fgotoxy(int x, int y)
{
    if (x < 0)
        x = 0;
    if (x > 131)
        x = 131;
    if (y < 0)
        y = 0;
    if (y > 44)
        y = 44;
    wch(1);
    wch(x + 32);
    wch(y + 32);
}

/* ---- 30 VOLSEARCH: the unit of volume FVID (a name, or #n), 0 if none on
   line; FDIR its directory (SYSCOM^.GDIRP) if it is a disk.  LOOKHARD: read
   every disk's directory again ---- */
int volsearch(unsigned char *fvid, int lookhard, struct direntry **fdir)
{
    struct syscomrec *sc;
    struct direntry *g;
    struct unitent *u;
    int lunit;
    int ok;
    int physunit;
    int hnow;
    int lnow;
    int n;
    sc = syscom;
    *fdir = NIL;
    ok = 0;
    physunit = 0;
    lunit = 0;
    n = fvid[0];
    if (n > 0) {
        if (fvid[1] == '#' && n > 1) {          /* #n: unit n */
            ok = 1;
            hnow = 2;
            do {
                if (ISDIGIT(fvid[hnow]))
                    lunit = lunit * 10 + fvid[hnow] - '0';
                else
                    ok = 0;
                hnow++;
            } while (hnow <= n && ok);
            physunit = ok && lunit > 0 && lunit <= MAXUNIT;
        }
        if (!physunit) {                        /* by its name */
            ok = 0;
            lunit = MAXUNIT;
            do {
                ok = peq(fvid, unitable[lunit].uvid);
                if (!ok)
                    lunit--;
            } while (!ok && lunit != 0);
        }
    }
    if (ok && unitable[lunit].uisblkd) {        /* is the directory in memory good? */
        ok = 0;
        g = sc->gdirp;
        if (g != NIL && peq(fvid, g->dtid)) {
            TIME(&hnow, &lnow);
            ok = lnow - D_LOADTIME(g) <= AGELIMIT;
        }
        if (!ok) {
            ok = physunit;
            if (fetchdir(lunit)) {
                if (!physunit)
                    ok = peq(fvid, sc->gdirp->dtid);
            } else
                ok = IORESULT() == INOERROR;
        }
    }
    if (!ok && lookhard) {                      /* every disk unit */
        lunit = MAXUNIT;
        do {
            if (unitable[lunit].uisblkd && fetchdir(lunit))
                ok = peq(fvid, unitable[lunit].uvid);
            if (!ok)
                lunit--;
        } while (!ok && lunit != 0);
    }
    if (!ok)
        return 0;
    u = &unitable[lunit];
    if (u->uvid[0] > 0)
        pcpy(fvid, u->uvid);
    if (u->uisblkd && sc->gdirp != NIL) {
        *fdir = sc->gdirp;
        TIME(&hnow, &D_LOADTIME(sc->gdirp));
    }
    return lunit;
}

/* ---- 31 WRITEDIR: directory FDIR back to unit FUNIT, if it is still that
   volume's ---- */
void writedir(int funit, struct direntry *fdir)
{
    struct direntry lde;
    struct unitent *u;
    int hnow;
    int lnow;
    int ok;
    int k;
    u = &unitable[funit];
    k = DKIND(fdir);
    ok = peq(u->uvid, fdir->dtid) && (k == UNTYPEDFILE || k == SECUREDIR);
    if (ok) {
        TIME(&hnow, &lnow);
        lnow = lnow - D_LOADTIME(fdir);
        ok = lnow <= AGELIMIT && lnow >= 0 && (syscom->miscinfo & MI_HASCLOCK);
        if (!ok) {                              /* no clock or too old: is it the volume still? */
            UNITREAD(funit, &lde, sizeof(struct direntry), DIRBLK);
            if (IORESULT() == INOERROR)
                ok = peq(fdir->dtid, lde.dtid);
        }
        if (ok) {
            fdir->dfirstblk = 0;
            k = (D_NUMFILES(fdir) + 1) * sizeof(struct direntry);
            UNITWRITE(funit, fdir, k, DIRBLK);
            ok = IORESULT() == INOERROR;
            if (fdir->dlastblk == 10)           /* a second copy */
                UNITWRITE(funit, fdir, k, 6);
            if (ok)
                TIME(&hnow, &D_LOADTIME(fdir));
        }
    }
    if (!ok) {
        u->uvid[0] = 0;
        u->ueovblk = MMAXINT;
    }
}

/* ---- 32 DIRSEARCH: FTID's entry in FDIR (FINDPERM: a permanent one, else
   a temporary one), 0 if none ---- */
int dirsearch(unsigned char *ftid, int findperm, struct direntry *fdir)
{
    struct direntry *e;
    int i;
    int n;
    n = D_NUMFILES(fdir);
    for (i = 1; i <= n; i++) {
        e = &fdir[i];
        if (peq(e->dtid, ftid) && findperm == !ISTEMP(e->daccess))
            return i;
    }
    return 0;
}

/* ---- 33 SCANTITLE: a file name: its volume, title, [size] and kind (by
   the name's end); 0 if it is not a good one ---- */
int scantitle(unsigned char *ftitlep, unsigned char *fvid, unsigned char *ftid,
              int *fsegs, int *fkind)
{
    unsigned char ftitle[82];
    unsigned char *sx;
    int i;
    int rbrack;
    int ch;
    int ok;
    i = ftitlep[0];                             /* (a STRING value parameter: a copy) */
    if (i > 80)
        i = 80;
    MVL(ftitlep, 1, ftitle, 1, i);
    ftitle[0] = i;
    fvid[0] = 0;
    ftid[0] = 0;
    *fsegs = 0;
    *fkind = UNTYPEDFILE;
    i = 1;
    while (i <= ftitle[0]) {                    /* no blanks, upper case */
        ch = ftitle[i];
        if (ch <= ' ')
            sdelete(ftitle, i, 1);
        else {
            if (ch >= 'a' && ch <= 'z')
                ftitle[i] = ch - 32;
            i++;
        }
    }
    if (ftitle[0] == 0)
        return 0;
    if (ftitle[1] == '*') {                     /* the boot volume */
        pcpy(fvid, syvid);
        sdelete(ftitle, 1, 1);
    }
    i = spos("\001:", ftitle);
    if (i <= 1) {
        if (fvid[0] == 0)
            pcpy(fvid, dkvid);                  /* the prefix */
        if (i == 1)
            sdelete(ftitle, 1, 1);
    } else if (i - 1 <= VIDLENG) {
        scopy(ftitle, fvid, 1, i - 1);
        sdelete(ftitle, 1, i);
    }
    if (fvid[0] == 0)
        return 0;
    i = spos("\001[", ftitle);
    if (i > 0)
        i--;
    else
        i = ftitle[0];
    if (i > TIDLENG)
        return 0;
    if (i > 0) {
        scopy(ftitle, ftid, 1, i);
        sdelete(ftitle, 1, i);
    }
    if (ftitle[0] == 0)
        ok = 1;
    else {                                      /* [n] blocks, [*] half the largest area */
        ok = 0;
        rbrack = spos("\001]", ftitle);
        if (rbrack == 2)
            ok = 1;
        else if (rbrack > 2) {
            ok = 1;
            i = 2;
            do {
                ch = ftitle[i];
                if (ISDIGIT(ch))
                    *fsegs = *fsegs * 10 + ch - '0';
                else
                    ok = 0;
                i++;
            } while (i != rbrack && ok);
            if (i == 3 && rbrack == 3 && ftitle[2] == '*') {
                *fsegs = -1;
                ok = 1;
            }
        }
    }
    if (ok && ftid[0] > 5) {                    /* the kind by the name's end */
        sx = ".TEXT\003.BACK\003.CODE\002.INFO\004.GRAF\006.FOTO\007";
        for (i = 0; i < 36; i = i + 6) {
            ch = 0;
            while (ch < 5 && ftid[ftid[0] - 4 + ch] == sx[i + ch])
                ch++;
            if (ch == 5) {
                *fkind = sx[i + 5];
                break;
            }
        }
    }
    if (ok && ftid[0] > 4) {                    /* a BASIC source is text (1.15) */
        i = ftid[0] - 3;
        if (ftid[i] == '.' && ftid[i + 1] == 'B' && ftid[i + 2] == 'A' && ftid[i + 3] == 'S')
            *fkind = TEXTFILE;
    }
    return ok;
}

/* ---- 34 DELENTRY: entry FINX out of FDIR ---- */
void delentry(int finx, struct direntry *fdir)
{
    int n;
    n = D_NUMFILES(fdir);
    MVL(&fdir[finx + 1], 0, &fdir[finx], 0, (n - finx) * (int)sizeof(struct direntry));
    fdir[n].dtid[0] = 0;
    D_NUMFILES(fdir) = n - 1;
}

/* ---- 35 INSENTRY: FENTRY into FDIR as entry FINX ---- */
void insentry(struct direntry *fentry, int finx, struct direntry *fdir)
{
    int n;
    n = D_NUMFILES(fdir);
    __cspv(3, &fdir[finx], 0, &fdir[finx + 1], 0, (n - finx + 1) * (int)sizeof(struct direntry));
    fdir[finx] = *fentry;
    D_NUMFILES(fdir) = n + 1;
}

/* ---- 36 HOMECURSOR, 37 CLEARSCREEN, 38 CLEARLINE ---- */
void homecursor(void)
{
    putprefixed(16, syscom->crtctrl.home);
}

void clearscreen(void)
{
    struct crtctrlrec *c;
    homecursor();
    c = &syscom->crtctrl;
    UNITCLEAR(3);
    if (c->eraseeos != 0)
        putprefixed(8, c->eraseeos);
    else
        putprefixed(64, c->clearscreen);
}

void clearline(void)
{
    putprefixed(4, syscom->crtctrl.eraseeol);
}

/* ---- 39 PROMPT: PL on the top line ---- */
void prompt(void)
{
    int i;
    homecursor();
    clearline();
    if (syscom->miscinfo & MI_SLOWTERM) {       /* only up to the ':' */
        i = SCANEQ(pl[0], ':', pl + 1);
        if (i != pl[0])
            pl[0] = i + 1;
    }
    wpstr(pl);
}

/* ---- 40 SPACEWAIT: "Type <space>"; 1 if it was the escape key instead ---- */
int spacewait(int flush)
{
    int ch;
    do {
        wstr("Type <space>");
        if (!(syscom->miscinfo & MI_SLOWTERM))
            wstr(" to continue");
        ch = getchar(flush);
        if (!gfiles[0]->feoln)
            wln();
        clearline();
    } while (ch != ' ' && ch != syscom->crtinfo.altmode);
    return ch != ' ';
}

/* ---- 41 GETCHAR: a key, upper case (FLUSH: what was typed ahead goes) ---- */
int getchar(int flush)
{
    int ch;
    if (flush)
        UNITCLEAR(1);
    if (inputfib->feof)
        EXITPROC(0, P_COMMAND);
    inputfib->fstate = FNEEDCHAR;
    freadchar(gfiles[0], &ch);
    if (ch >= 'a' && ch <= 'z')
        ch = ch - 32;
    return ch;
}

/* ---- 42 FETCHDIR: unit FUNIT's directory into SYSCOM^.GDIRP, checked; 0 if
   it has none ---- */
int fetchdir(int funit)
{
    struct syscomrec *sc;
    struct unitent *u;
    struct direntry *g;
    struct direntry *e;
    int linx;
    int ok;
    int hnow;
    int k;
    int uk;
    sc = syscom;
    u = &unitable[funit];
    if (sc->gdirp == NIL)
        NEW(&sc->gdirp, DIRSIZE / 2);
    UNITREAD(funit, sc->gdirp, DIRSIZE, DIRBLK);
    ok = sc->iorslt == INOERROR;
    if (ok) {
        g = sc->gdirp;
        ok = 0;
        uk = sc->miscinfo & MI_USERKIND;
        k = DKIND(g);
        if (g->dfirstblk == 0 &&
            (uk == UK_BOOKER || ((uk == UK_AQUIZ || uk == UK_PQUIZ) && k == SECUREDIR) ||
             (uk == 0 && k == UNTYPEDFILE)) &&
            g->dtid[0] > 0 && g->dtid[0] <= VIDLENG && D_NUMFILES(g) >= 0 && D_NUMFILES(g) <= MAXDIR) {
            ok = 1;
            if (!peq(g->dtid, u->uvid)) {       /* a new volume in the unit: check every entry */
                linx = 1;
                while (linx <= D_NUMFILES(g)) {
                    e = &g[linx];
                    if (e->dtid[0] == 0 || e->dtid[0] > TIDLENG || e->dlastblk < e->dfirstblk ||
                        e->dlastbyte > FBLKSIZE || e->dlastbyte <= 0 || (unsigned)e->daccess >= (unsigned)YEAR100) {
                        ok = 0;
                        delentry(linx, g);
                    } else
                        linx++;
                }
                if (!ok) {                      /* it was changed: write it */
                    UNITWRITE(funit, g, (D_NUMFILES(g) + 1) * (int)sizeof(struct direntry), DIRBLK);
                    ok = sc->iorslt == INOERROR;
                }
            }
        }
        if (ok) {
            pcpy(u->uvid, g->dtid);
            u->ueovblk = D_EOVBLK(g);
            TIME(&hnow, &D_LOADTIME(g));
        }
    }
    if (!ok) {
        u->uvid[0] = 0;
        u->ueovblk = MMAXINT;
        RELEASE(&sc->gdirp);
        sc->gdirp = NIL;
    }
    return ok;
}

/* ---- 43 COMMAND: the command loop (EXIT(COMMAND) comes back here) ---- */
void command(void)
{
    int t;
    state = HALTINIT;
    do {
        RELEASE(&emptyheap);
        while (!peq(unitable[syscom->sysunit].uvid, syvid)) {   /* the boot disk back */
            pset(pl, "Put in :");
            sinsert(syvid, pl, 80, 8);
            prompt();
            for (t = 4000; t != 0; t--)
                ;
            fetchdir(syscom->sysunit);
        }
        getcmd();
        if (state == DEBUGCALL)
            debugger();
        else if (state != HALTINIT)
            userprogram(NIL, NIL);
        if (state >= COMPONLY && state <= COMPDEBUG && userinfo.errnum == 0) {
            fclose(userinfo.codefibp, CLOCK);   /* (FINISHCOMPILE goes on) */
            if (IORESULT() != INOERROR) {
                t = IORESULT();
                wln();
                clearline();
                printerror(10, t);
            }
        }
        if (state == UPROGNOU || state == UPROGUOK) {
            fclose(gfiles[0], CNORMAL);
            fclose(gfiles[1], CLOCK);
        }
        if (UNITBUSY(1) || UNITBUSY(2))
            UNITCLEAR(1);
    } while (state != HALTINIT);
}

/* ---- 1: the operating system's body, the outer block (the boot starts it):
   main is procedure 1, the globals its first locals (#pragma system) ---- */
void main(void)
{
    emptyheap = NIL;
    initialize();
    do {
        command();
        if (emptyheap != NIL)
            initialize();
    } while (emptyheap != NIL);
}

/* ---- this file's own procedures ---- */

/* the rest of EXECERROR: the message, where it was (PRINTLOCS), "Type
   <space>" (then COMMAND starts again) */
void xerrmsg(void)
{
    struct syscomrec *sc;
    struct mscw *m;
    unsigned char *jt;
    sc = syscom;
    RELEASE(&emptyheap);
    gfiles[0] = inputfib;
    gfiles[1] = outputfib;
    sc->bombipc = IORESULT();
    fwriteln(systerm);
    if (sc->sysunit == volsearch(syvid, 0, &sc->gdirp))    /* (the messages are on the boot disk) */
        printerror(sc->xeqerr, sc->bombipc);
    else {
        wstr("Exec err # ");
        wint(sc->xeqerr);
        wln();
        if (sc->xeqerr == 10) {
            wch(',');
            wint(sc->bombipc);
        }
    }
    m = sc->bombp;                              /* PRINTLOCS: segment, procedure, offset in it */
    jt = m->msjtab;
    wstr("S# ");
    wint(m->msseg[0]);
    wstr(", P# ");
    wint(jt[0]);
    wstr(", I# ");
    wint(m->msipc - ((int)jt - 2 - ((int *)jt)[-1]));
    wln();
    if (!spacewait(1))
        EXITPROC(0, P_COMMAND);
}

/* RESETER: a file back to its start (FRESET without the GET), its buffer
   written first */
void reseter(struct fib *f)
{
    struct direntry *h;
    int bigger;
    f->freptcnt = 0;
    f->feoln = 0;
    f->feof = 0;
    if (!f->fisblkd)
        return;
    h = &f->fheader;
    bigger = f->fnxtblk > f->fmaxblk;
    if (bigger)
        f->fmaxblk = f->fnxtblk;
    if (f->fsoftbuf) {
        if (bigger)
            f->fmaxbyte = f->fnxtbyte;
        else if (f->fnxtblk == f->fmaxblk && f->fnxtbyte > f->fmaxbyte) {
            bigger = 1;
            f->fmaxbyte = f->fnxtbyte;
        }
        if (f->fbufchngd) {
            f->fbufchngd = 0;
            f->fmodified = 1;
            if (bigger)
                FILLCHAR(f->fbuffer + f->fnxtbyte, FBLKSIZE - f->fnxtbyte, 0);
            UNITWRITE(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + f->fnxtblk - 1);
            if (bigger && DKIND(h) == TEXTFILE && (f->fnxtblk & 1)) {   /* a text page's second block */
                f->fmaxblk++;
                FILLCHAR(f->fbuffer, FBLKSIZE, 0);
                UNITWRITE(f->funit, f->fbuffer, FBLKSIZE, h->dfirstblk + f->fnxtblk);
            }
        }
        f->fnxtbyte = FBLKSIZE;
    }
    f->fnxtblk = 0;
    if (f->fsoftbuf && DKIND(h) == TEXTFILE)
        f->fnxtblk = 2;
}

/* CANTSTRETCH: give file F the free blocks after it; 1 if it cannot grow */
int cantstretch(struct fib *f)
{
    struct direntry *ldir;
    struct direntry *h;
    struct direntry *e;
    int linx;
    int found;
    int lavailblk;
    int r;
    int ok;
    r = 1;
    ok = 0;
    h = &f->fheader;
    if (h->dtid[0] > 0) {
        if (f->funit != volsearch(f->fvid, 0, &ldir)) {
            syscom->iorslt = ILOSTUNIT;
            goto out;
        }
        found = 0;
        linx = 1;
        while (linx <= D_NUMFILES(ldir) && !found) {
            e = &ldir[linx];
            found = e->dfirstblk == h->dfirstblk && e->dlastblk == h->dlastblk;
            linx++;
        }
        if (!found) {
            syscom->iorslt = ILOSTFILE;
            goto out;
        }
        if (linx > D_NUMFILES(ldir))
            lavailblk = D_EOVBLK(ldir);
        else
            lavailblk = ldir[linx].dfirstblk;
        if (h->dlastblk < lavailblk || h->dlastbyte < FBLKSIZE) {
            e = &ldir[linx - 1];
            e->dlastblk = lavailblk;
            e->dlastbyte = FBLKSIZE;
            writedir(f->funit, ldir);
            if (IORESULT() != INOERROR)
                goto out;
            f->feof = 0;
            f->feoln = 0;
            if (f->fstate != FJANDW)
                f->fstate = FNEEDCHAR;
            h->dlastblk = lavailblk;
            h->dlastbyte = FBLKSIZE;
            h->daccess = (h->daccess & 0x1FF) | YEAR100;
            r = 0;
        }
        ok = 1;
    }
out:
    if (!ok) {
        f->feof = 1;
        f->feoln = 1;
    }
    return r;
}

/* CHECKDEL: the console's line and character delete keys while a string or
   a number is read; 1 if CH was one of them */
int checkdel(int ch, int *sinx)
{
    struct crtctrlrec *c;
    struct crtinforec *ci;
    int r;
    c = &syscom->crtctrl;
    ci = &syscom->crtinfo;
    r = 0;
    if (ch == ci->linedel) {
        r = 1;
        if (c->backspace == 0 || c->eraseeol == 0) {
            *sinx = 1;
            wstr("<DEL");
            wln();
        } else {
            while (*sinx > 1) {
                *sinx = *sinx - 1;
                wch(c->backspace);
            }
            wch(c->escape);
            wch(c->eraseeol);
        }
    }
    if (ch == ci->chardel) {
        r = 1;
        if (*sinx > 1) {
            *sinx = *sinx - 1;
            if (c->backspace == 0) {
                if (ci->chardel < ' ')
                    wch('_');
            } else {
                if (ci->chardel != c->backspace)
                    wch(c->backspace);
                wch(' ');
                wch(c->backspace);
            }
        } else if (ci->chardel == c->backspace)
            wch(' ');
    }
    return r;
}

/* PUTPREFIXED: a screen code (after ESCAPE when its PREFIXED bit is set:
   the bit, not its number, here), then the fill NULs */
void putprefixed(int bit, int commandchar)
{
    if (commandchar != 0) {
        if (syscom->crtctrl.prefixed & bit)
            wch(syscom->crtctrl.escape);
        wch(commandchar);
        wpstr(filler);
    }
}

/* ENTERTEMP: a temporary entry for FTID, FSEGS blocks (0: the largest free
   area, -1: half of it or the second largest) in FDIR; its index, 0 if no
   room */
int entertemp(unsigned char *ftid, int fsegs, int fkind, struct direntry *fdir)
{
    struct direntry lde;
    int i;
    int lasti;
    int dinx;
    int sinx;
    int ssegs;
    int rt11ish;
    int freearea;
    dinx = 0;
    lasti = D_NUMFILES(fdir);
    sinx = 0;
    ssegs = 0;
    if (fsegs <= 0) {                           /* FINDMAX over the gaps and the end */
        rt11ish = fsegs < 0;
        for (i = 1; i <= lasti + 1; i++) {
            freearea = (i <= lasti ? fdir[i].dfirstblk : D_EOVBLK(fdir)) - fdir[i - 1].dlastblk;
            if (freearea > fsegs) {
                sinx = dinx;
                ssegs = fsegs;
                dinx = i;
                fsegs = freearea;
            } else if (freearea > ssegs) {
                ssegs = freearea;
                sinx = i;
            }
        }
        if (rt11ish) {
            if (__dvi(fsegs, 2) <= ssegs) {
                fsegs = ssegs;
                dinx = sinx;
            } else
                fsegs = __dvi(fsegs + 1, 2);
        }
    } else {                                    /* the first gap that holds it */
        for (i = 1; i <= lasti; i++)
            if (fdir[i].dfirstblk - fdir[i - 1].dlastblk >= fsegs) {
                dinx = i;
                break;
            }
        if (dinx == 0 && D_EOVBLK(fdir) - fdir[lasti].dlastblk >= fsegs)
            dinx = lasti + 1;
    }
    if (lasti == MAXDIR)
        dinx = 0;
    if (dinx > 0) {
        lde.dfirstblk = fdir[dinx - 1].dlastblk;
        lde.dlastblk = lde.dfirstblk + fsegs;
        lde.dfkind = fkind;
        pcpy(lde.dtid, ftid);
        lde.dlastbyte = FBLKSIZE;
        lde.daccess = YEAR100;
        insentry(&lde, dinx, fdir);
    }
    return dinx;
}

/* Pascal strings: equal, copy */
int peq(unsigned char *a, unsigned char *b)
{
    int n;
    n = a[0];
    if (n != b[0])
        return 0;
    while (n > 0) {
        if (a[n] != b[n])
            return 0;
        n--;
    }
    return 1;
}

void pcpy(unsigned char *d, unsigned char *s)
{
    MVL(s, 0, d, 0, s[0] + 1);
}

/* d := the C string s */
void pset(unsigned char *d, unsigned char *s)
{
    d[0] = SCANEQ(255, 0, s);
    MVL(s, 0, d, 1, d[0]);
}

/* to OUTPUT (GFILES[1], as the Pascal one's WRITE(OUTPUT, ...)): a C
   string, a Pascal string, a character, a number, a line end */
void wstr(unsigned char *s)
{
    int n;
    n = SCANEQ(255, 0, s);
    fwritebytes(gfiles[1], s, n, n);
}

void wpstr(unsigned char *s)
{
    fwritestring(gfiles[1], s, 0);
}

void wch(int c)
{
    fwritechar(gfiles[1], c, 0);
}

void wint(int i)
{
    fwriteint(gfiles[1], i, 0);
}

void wln(void)
{
    fwriteln(gfiles[1]);
}
