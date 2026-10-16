/* segs.c -- the operating system's segment procedures (SYSSEGS.A and
   SYSSEGS.B.TEXT of the Pascal one): each segment is read from SYSTEM.PASCAL
   when it is called and gives its memory back when it returns.  A segment's
   first procedure is its procedure 1, the one called; the Pascal one's nested
   procedures are the procedures after it. */

/* ---- segment 1, USERPROG: a program run (GETCMD's ASSOCIATE) takes its
   place in the segment table; this is what is left when none was ---- */
#pragma segment USERPROG
void userprogram(struct fib *input, struct fib *output)
{
    fwriteln(systerm);
    pset(pl, "No user program");
    fwritestring(systerm, pl, 0);
}

/* ---- segment 2, DEBUGGER ---- */
#pragma segment DEBUGGER
void debugger(void)
{
    fwriteln(systerm);
    pset(pl, "No debugger in system");
    fwritestring(systerm, pl, 0);
}

/* ---- segment 3, PRINTERR: an execution error's message ---- */
#pragma segment PRINTERR
void printerror(int xeqerr, int iorslt)
{
    unsigned char *s;
    s = "Unknown run-time error";
    switch (xeqerr) {
    case 1: s = "Value range error"; break;
    case 2: s = "No proc in seg-table"; break;
    case 3: s = "Exit from uncalled proc"; break;
    case 4: s = "Stack overflow"; break;
    case 5: s = "Integer overflow"; break;
    case 6: s = "Divide by zero"; break;
    case 7: s = "NIL pointer reference"; break;
    case 8: s = "Program interrupted by user"; break;
    case 9: s = "System IO error"; break;
    case 10:
        wstr("IO error: ");
        s = "unknown cause";
        switch (iorslt) {
        case 1: s = "parity (CRC)"; break;
        case 2: s = "illegal unit #"; break;
        case 3: s = "illegal IO request"; break;
        case 4: s = "data-com timeout"; break;
        case 5: s = "vol went off-line"; break;
        case 6: s = "file lost in dir"; break;
        case 7: s = "bad file name"; break;
        case 8: s = "no room on vol"; break;
        case 9: s = "vol not found"; break;
        case 10: s = "file not found"; break;
        case 11: s = "dup dir entry"; break;
        case 12: s = "file already open"; break;
        case 13: s = "file not open"; break;
        case 14: s = "bad input format"; break;
        case 15: s = "ring buffer overflow"; break;
        case 16: s = "disk write protected"; break;
        case 17: s = "illegal block #"; break;
        case 18: s = "bad byte count"; break;
        case 19: s = "bad init record"; break;
        }
        break;
    case 11: s = "Unimplemented instruction"; break;
    case 12: s = "Floating point error"; break;
    case 13: s = "String overflow"; break;
    case 14: s = "Programmed HALT"; break;
    case 15: s = "Programmed break-point"; break;
    }
    wstr(s);
    wln();
}

/* ---- segment 4, INITIALI: the boot (and I(nitialize): the heap, the
   units' volumes, the console files, SYSTEM.MISCINFO, the work files ---- */
#pragma segment INITIALI

/* INITIALIZE's variables, which its parts share */
struct ini {
    int justbooted;
    int startup;                        /* *SYSTEM.STARTUP is there: no welcome */
    unsigned char ltitle[42];           /* STRING[40] */
};
void initheap(void);
void initunitable(struct ini *v);
void initentry(int lunit, unsigned char *name);
void initfiles(struct ini *v);
void initsyscom(void);
void initfiller(void);
void initworkfile(struct ini *v);
void tryopen(struct fib *wf, unsigned char *first, unsigned char *secvol, unsigned char *secname, int *flag);
void initcharset(struct ini *v);

void initialize(void)
{
    struct ini v;
    int stkfill[1200];                  /* (the Pascal one's STKFILL: the stack it took) */
    struct syscomrec *sc;
    int d;
    sc = syscom;
    v.justbooted = emptyheap == NIL;
    v.startup = 0;
    stkfill[0] = 0;
    if (v.justbooted)
        initheap();
    else
        RELEASE(&emptyheap);
    initunitable(&v);                   /* and the date, the file names, *SYSTEM.STARTUP */
    initfiles(&v);
    initsyscom();                       /* and some globals */
    initworkfile(&v);
    clearscreen();
    if (sc->miscinfo & MI_HAS8510A)
        initcharset(&v);
    wln();
    if (!v.justbooted) {
        wstr("System re-initialized");
        wln();
        return;
    }
    if (v.startup)
        return;
    if (sc->miscinfo & MI_HASXYCRT) {
        fgotoxy(0, __dvi(sc->crtinfo.height, 3));
        wpstr(filler);
    }
    wstr("Welcome  ");
    wpstr(syvid);
    wstr(",  to");
    wln();
    wln();
    wstr("U.C.S.D.  Pascal  System  II.0");
    wln();
    wln();
    wstr("Current date is  ");
    wint(__dvi(thedate & 0x1F0, 16));                   /* DAY */
    wch('-');
    d = thedate & 15;                                   /* MONTH */
    fwritebytes(gfiles[1], "???JanFebMarAprMayJunJulAugSepOctNovDec?????????" + d * 3, 3, 3);
    wch('-');
    wint(__dvi(thedate & 0x7E00, 512) + (thedate < 0 ? 64 : 0));   /* YEAR */
    wln();
}

/* INITHEAP: the system's files on the heap, then its mark (EMPTYHEAP) */
void initheap(void)
{
    unsigned char *lwindow;
    syscom->gdirp = NIL;                /* (before the first NEW) */
    NEW(&swapfib, FIBSHORT);
    finit(swapfib, NIL, -1);
    NEW(&inputfib, FIBSHORT);
    NEW(&lwindow, 1);
    finit(inputfib, lwindow, 0);
    NEW(&outputfib, FIBSHORT);
    NEW(&lwindow, 1);
    finit(outputfib, lwindow, 0);
    NEW(&systerm, FIBSHORT);
    NEW(&lwindow, 1);
    finit(systerm, lwindow, 0);
    gfiles[0] = inputfib;
    gfiles[1] = outputfib;
    NEW(&userinfo.symfibp, FIBSHORT);
    finit(userinfo.symfibp, NIL, -1);
    NEW(&userinfo.codefibp, FIBSHORT);
    finit(userinfo.codefibp, NIL, -1);
    MARK(&emptyheap);
}

/* INITUNITABLE: every unit's volume; where the system programs are (on the
   boot disk, else the first disk with them); the date */
void initunitable(struct ini *v)
{
    struct direntry *ldir;
    struct unitent *u;
    struct fib lfib;
    unsigned char temp[5][24];
    int lunit;
    int f;
    int notfound;
    int bit;
    pset(filename[ASSMBLER], ":SYSTEM.ASSMBLER");
    pset(filename[COMPILER], ":SYSTEM.COMPILER");
    pset(filename[EDITOR], ":SYSTEM.EDITOR");
    pset(filename[FILER], ":SYSTEM.FILER");
    pset(filename[LINKER], ":SYSTEM.LINKER");
    MOVELEFT(filename, temp, sizeof(temp));
    notfound = 31;
    finit(&lfib, NIL, -1);
    for (lunit = 0; lunit <= MAXUNIT; lunit++) {
        u = &unitable[lunit];
        u->uvid[0] = 0;
        u->uisblkd = lunit == 4 || lunit == 5 || lunit >= 9;
        if (u->uisblkd) {
            u->ueovblk = MMAXINT;
            UNITCLEAR(lunit);
            if (IORESULT() == INOERROR && fetchdir(lunit)) {
                pcpy(u->uvid, syscom->gdirp->dtid);
                if (lunit == syscom->sysunit) {
                    pcpy(syvid, u->uvid);
                    pset(v->ltitle, "*SYSTEM.STARTUP");
                    fopen(&lfib, v->ltitle, 1, NIL);
                    v->startup = lfib.fisopen;
                    fclose(&lfib, CNORMAL);
                }
                bit = 1;
                for (f = ASSMBLER; f <= LINKER; f++) {
                    if (lunit == syscom->sysunit || (notfound & bit)) {
                        pcpy(v->ltitle, u->uvid);
                        sconcat(v->ltitle, temp[f], 40);
                        fopen(&lfib, v->ltitle, 1, NIL);
                        if (lfib.fisopen) {
                            pcpy(filename[f], v->ltitle);
                            notfound = notfound & ~bit;
                        }
                        fclose(&lfib, CNORMAL);
                    }
                    bit = bit + bit;
                }
            }
        }
    }
    if (v->justbooted)
        pcpy(dkvid, syvid);
    volsearch(syvid, 0, &ldir);
    if (ldir == NIL)
        HALT();
    thedate = D_LASTBOOT(ldir);
    initentry(1, "CONSOLE");
    initentry(2, "SYSTERM");
    initentry(3, "GRAPHIC");
    initentry(6, "PRINTER");
    initentry(7, "REMIN");
    initentry(8, "REMOUT");
}

void initentry(int lunit, unsigned char *name)
{
    UNITCLEAR(lunit);
    if (IORESULT() == INOERROR)
        pset(unitable[lunit].uvid, name);
}

/* INITFILES: INPUT and OUTPUT the console, the others closed */
void initfiles(struct ini *v)
{
    fclose(swapfib, CNORMAL);
    fclose(userinfo.symfibp, CNORMAL);
    fclose(userinfo.codefibp, CNORMAL);
    fclose(inputfib, CNORMAL);
    fclose(outputfib, CNORMAL);
    pset(v->ltitle, "CONSOLE:");
    fopen(inputfib, v->ltitle, 1, NIL);
    fopen(outputfib, v->ltitle, 1, NIL);
    if (v->justbooted) {
        pset(v->ltitle, "SYSTERM:");
        fopen(systerm, v->ltitle, 1, NIL);
    }
    gfiles[0] = inputfib;
    gfiles[1] = outputfib;
    gfiles[2] = systerm;
    gfiles[3] = NIL;
    gfiles[4] = NIL;
    gfiles[5] = NIL;
}

/* INITSYSCOM: the terminal from *SYSTEM.MISCINFO (its first record is a
   SYSCOMREC: MISCINFO, CRTTYPE, CRTCTRL and CRTINFO are taken from it) */
void initsyscom(void)
{
    int buf[256];                       /* (the first block of the file) */
    struct syscomrec *m;
    struct fib f;
    unsigned char title[18];
    struct syscomrec *sc;
    sc = syscom;
    initfiller();
    debuginfo = NIL;
    ipot[0] = 1;
    ipot[1] = 10;
    ipot[2] = 100;
    ipot[3] = 1000;
    ipot[4] = 10000;
    digits[0] = 0;
    digits[1] = 0;
    digits[2] = 0;
    digits[3] = 0x3FF;                          /* ['0'..'9'] */
    sc->xeqerr = 0;
    sc->iorslt = INOERROR;
    sc->bugstate = 0;
    pset(title, "*SYSTEM.MISCINFO");
    finit(&f, NIL, -1);
    fopen(&f, title, 1, NIL);
    if (IORESULT() == INOERROR) {
        if (fblockio(&f, buf, 0, 1, 0, 1) == 1) {  /* (RESET read the first record) */
            m = (struct syscomrec *)buf;
            sc->miscinfo = m->miscinfo;
            sc->crttype = m->crttype;
            sc->crtctrl = m->crtctrl;
            sc->crtinfo = m->crtinfo;
            initfiller();
        }
        fclose(&f, CNORMAL);
    }
    UNITCLEAR(1);                               /* the BIOS gets the new console keys */
}

/* FILLER: FILLCOUNT NULs */
void initfiller(void)
{
    struct crtctrlrec *c;
    c = &syscom->crtctrl;
    if (c->fillcount > FILL_LEN)
        c->fillcount = FILL_LEN;
    filler[0] = c->fillcount;
    FILLCHAR(filler + 1, c->fillcount, 0);
}

/* INITWORKFILE: the work files *SYSTEM.WRK.TEXT and .CODE, or the ones named */
void initworkfile(struct ini *v)
{
    struct inforec *u;
    u = &userinfo;
    u->errnum = 0;
    u->errblk = 0;
    u->errsym = 0;
    if (v->justbooted) {
        u->symtid[0] = 0;
        u->codetid[0] = 0;
        u->worktid[0] = 0;
        pcpy(u->symvid, syvid);
        pcpy(u->codevid, syvid);
        pcpy(u->workvid, syvid);
    }
    tryopen(u->symfibp, "*SYSTEM.WRK.TEXT", u->symvid, u->symtid, &u->gotsym);
    tryopen(u->codefibp, "*SYSTEM.WRK.CODE", u->codevid, u->codetid, &u->gotcode);
    u->altmode = syscom->crtinfo.altmode;
    u->slowterm = (syscom->miscinfo & MI_SLOWTERM) != 0;
    u->stupid = (syscom->miscinfo & MI_STUPID) != 0;
}

void tryopen(struct fib *wf, unsigned char *first, unsigned char *secvol, unsigned char *secname, int *flag)
{
    unsigned char ltitle[24];
    pset(ltitle, first);
    fopen(wf, ltitle, 1, NIL);
    if (!wf->fisopen && secname[0] != 0) {
        pcpy(ltitle, secvol);
        sconcat(ltitle, "\001:", 23);
        sconcat(ltitle, secname, 23);
        fopen(wf, ltitle, 1, NIL);
    }
    *flag = wf->fisopen;
    if (*flag) {
        pcpy(secvol, wf->fvid);
        pcpy(secname, wf->fheader.dtid);
    }
    fclose(wf, CNORMAL);
}

/* INITCHARSET: a Terak 8510A's character sets from *SYSTEM.CHARSET (only
   with MISCINFO's HAS8510A) */
void initcharset(struct ini *v)
{
    struct fib lfib;
    unsigned char charbuf[2560];        /* SET1 (960), 64, SET2 (960), 64, TRITON (512) */
    int display[1600];                  /* (80 x 20) */
    unsigned char *p;
    int i;
    int dotriton;
    finit(&lfib, NIL, -1);
    pset(v->ltitle, "*SYSTEM.CHARSET");
    fopen(&lfib, v->ltitle, 1, NIL);
    if (lfib.fisopen) {
        UNITCLEAR(3);
        if (IORESULT() == INOERROR) {
            UNITWRITE(3, &p, 128, 0);
            dotriton = lfib.fheader.dlastblk - lfib.fheader.dfirstblk > 4;
            UNITREAD(lfib.funit, charbuf, sizeof(charbuf), lfib.fheader.dfirstblk);
            p = (unsigned char *)(512 - 8192);  /* (the display's character memory) */
            for (i = 0; i < 96; i++) {
                MOVERIGHT(charbuf + 10 * i, p, 10);
                p = p + 16;
            }
            p = (unsigned char *)(512 - 6144);
            for (i = 0; i < 96; i++) {
                MOVERIGHT(charbuf + 1024 + 10 * i, p, 10);
                p = p + 16;
            }
            if (v->justbooted && dotriton && !v->startup) {
                FILLCHAR(display, sizeof(display), 0);
                for (i = 0; i < 64; i++)
                    MOVELEFT(charbuf + 2048 + 8 * i, &display[20 * i + 10], 8);
                UNITWRITE(3, display - 80, 23, 0);
            } else
                UNITWRITE(3, display, 7, 0);
        }
    } else
        syscom->miscinfo = syscom->miscinfo & ~MI_HAS8510A;
    fclose(&lfib, CNORMAL);
}

/* ---- segment 5, GETCMD: the Command: prompt.  It starts with STATE what
   the last command was (the Pascal one's LASTST) and ends with STATE what to
   do next: its procedures set it and leave GETCMD (EXIT(GETCMD)) ---- */
#pragma segment GETCMD

#define ASSEMONLY LINKANDGO
#define PXRUN   25601                   /* pexec (Tiny-C): SYSCOM^.EXPANSION[0] */
#define PXCHILD 25602
#define PXBACK  25603
#define FOUND_OK  0                     /* ASSOCIATE's status */
#define FOUND_BAD 1
#define NOT_FOUND 2
#define LEAVE(st) do { state = (st); EXITPROC(S_GETCMD, 1); } while (0)

int loadsegs(int lunit, int fblk);
int associate(unsigned char *titlep, int oktolink, int runonly, int error_ok, int *ass_status);
int sys_associate(int sys_name);
void startcompile(int nextst);
void finishcompile(void);
void execute(void);
int subexec(unsigned char *title);
void runworkfile(int oktolink, int runonly);
void setsegs(int *w, int lunit, int fblk);

void getcmd(void)
{
    unsigned char t[24];
    int *x;
    int ch;
    int badcmd;
    int dont_care;
    inputfib->feof = 0;
    outputfib->feof = 0;
    systerm->feof = 0;
    gfiles[0] = inputfib;
    gfiles[1] = outputfib;
    /* pexec: a program starts another and is started again when it ends,
       through SYSCOM^.EXPANSION: [0] the state, [1],[2] the unit and first
       block of the code file to run, [3],[4] those of the one to start again,
       [5] the exit status (-1: could not be started, -2: an execution error) */
    x = syscom->expansion;
    if (x[0] == PXBACK)
        x[0] = 0;
    if (x[0] == PXRUN) {
        x[0] = PXCHILD;
        if (loadsegs(x[1], x[2])) {
            state = SYSPROG;
            return;
        }
        x[5] = -1;
    }
    if (x[0] == PXCHILD) {
        x[0] = 0;
        if (state == HALTINIT)
            x[5] = -2;                          /* it stopped: an error */
        if (loadsegs(x[3], x[4])) {
            x[0] = PXBACK;
            state = SYSPROG;
            return;
        }
    }
    if (state == HALTINIT) {
        pset(t, "*SYSTEM.STARTUP");
        if (associate(t, 0, 0, 0, &dont_care)) {
            clearscreen();
            state = SYSPROG;
            return;
        }
    }
    if (state >= COMPONLY && state <= COMPDEBUG)
        finishcompile();
    if (state == LINKANDGO || state == LINKDEBUG)
        runworkfile(0, state == LINKANDGO);
    if ((syscom->miscinfo & MI_USERKIND) == UK_AQUIZ) {
        if (state == HALTINIT) {
            state = COMPANDGO;
            runworkfile(1, 1);
        } else {
            emptyheap = NIL;
            state = HALTINIT;
            return;
        }
    }
    userinfo.errnum = 0;
    userinfo.errblk = 0;
    userinfo.errsym = 0;
    badcmd = 0;
    for (;;) {
        pset(pl, "Command: E(dit, R(un, F(ile, C(omp, L(ink, X(ecute, A(ssem, D(ebug,? [II.0]");
        prompt();
        ch = getchar(badcmd);
        clearscreen();
        if (ch == '?') {
            pset(pl, "Command: U(ser restart, I(nitialize, H(alt, S(hell");
            prompt();
            ch = getchar(badcmd);
            clearscreen();
        }
        badcmd = SCANEQ(13, ch, "ERFCLXADUIH?S") == 13;
        if (badcmd)
            continue;
        switch (ch) {
        case 'E':
            wln();
            if (sys_associate(EDITOR)) {
                state = SYSPROG;
                return;
            }
            break;
        case 'F':
            wln();
            if (sys_associate(FILER)) {
                state = SYSPROG;
                return;
            }
            break;
        case 'S':                               /* the Tiny-C shell (pexec) */
            wln();
            pset(t, "*SYSTEM.SHELL");
            if (associate(t, 0, 0, 1, &dont_care)) {
                state = SYSPROG;
                return;
            }
            break;
        case 'L':
            wstr("Linking...");
            wln();
            if (sys_associate(LINKER)) {
                state = SYSPROG;
                return;
            }
            break;
        case 'X':
            execute();
            break;
        case 'C':
            startcompile(COMPONLY);
            break;
        case 'A':
            startcompile(ASSEMONLY);
            break;
        case 'U':
            if (state != UPROGNOU) {
                wstr("Restarting...");
                wln();
                state = SYSPROG;
                return;
            }
            wln();
            wstr("U not allowed");
            break;
        case 'R':
        case 'D':
            runworkfile(1, ch == 'R');
            break;
        case 'I':
        case 'H':
            if (ch == 'H')
                emptyheap = NIL;
            state = HALTINIT;
            return;
        }
    }
}

/* LOADSEGS: SEGTABLE 1 and 7..15 to the linked code file whose block 0 is
   block FBLK of unit LUNIT (as ASSOCIATE does after FOPEN) */
int loadsegs(int lunit, int fblk)
{
    int w[256];                         /* block 0: DISKINFO, SEGNAME, SEGKIND, FILLER */
    int lseg;
    int ok;
    int old;
    ok = 0;
    if (lunit > 0 && lunit <= MAXUNIT) {
        UNITREAD(lunit, w, 512, fblk);
        if (IORESULT() == INOERROR) {
            ok = 1;
            old = 0;
            for (lseg = 0; lseg <= 15; lseg++)
                if (w[96 + lseg] < 0 || w[96 + lseg] > 4)
                    old = 1;                    /* before I.5: all linked */
            if (!old)
                for (lseg = 0; lseg <= 15; lseg++)
                    if (w[96 + lseg] != 0)
                        ok = 0;
            if (ok)
                setsegs(w, lunit, fblk);
        }
    }
    return ok;
}

/* SEGTABLE 1 and 7..15 from a code file's block 0 */
void setsegs(int *w, int lunit, int fblk)
{
    struct segentry *e;
    int lseg;
    for (lseg = 1; lseg <= 15; lseg++)
        if (lseg == 1 || lseg >= 7) {
            e = &syscom->segtable[lseg];
            e->codeunit = lunit;
            e->codeleng = w[2 * lseg + 1];
            e->diskaddr = w[2 * lseg] + fblk;
        }
}

/* ASSOCIATE: the code file TITLE in the segment table; it may need linking
   first (OKTOLINK: start the Linker) */
int associate(unsigned char *titlep, int oktolink, int runonly, int error_ok, int *ass_status)
{
    unsigned char title[82];
    int w[256];
    struct fib *cf;
    int rslt;
    int lseg;
    int r;
    rslt = titlep[0];                   /* (a STRING value parameter) */
    if (rslt > 80)
        rslt = 80;
    MOVELEFT(titlep + 1, title + 1, rslt);
    title[0] = rslt;
    *ass_status = NOT_FOUND;
    r = 0;
    cf = userinfo.codefibp;
    fopen(cf, title, 1, NIL);
    rslt = syscom->iorslt;
    if (rslt != INOERROR) {
        if (error_ok) {
            if (rslt == IBADTITLE)
                wstr("Illegal file name");
            else {
                wstr("No file ");
                wpstr(title);
            }
        }
        goto out;
    }
    *ass_status = FOUND_BAD;
    if (DKIND(&cf->fheader) != CODEFILE) {
        wpstr(title);
        wstr(" not code");
        goto out;
    }
    UNITREAD(cf->funit, w, 512, cf->fheader.dfirstblk);
    if (IORESULT() != INOERROR) {
        wstr("Bad block #0");
        goto out;
    }
    for (lseg = 0; lseg <= 15; lseg++)
        if (w[96 + lseg] < 0 || w[96 + lseg] > 4) {    /* before I.5: fix it up */
            FILLCHAR(w + 96, 32, 0);
            FILLCHAR(w + 112, 288, 0);
            UNITWRITE(cf->funit, w, 512, cf->fheader.dfirstblk);
        }
    for (lseg = 0; lseg <= 15; lseg++)
        if (w[96 + lseg] != 0) {                /* not linked */
            if (oktolink) {
                wstr("Linking...");
                wln();
                fclose(cf, CNORMAL);
                if (sys_associate(LINKER))
                    LEAVE(runonly ? LINKANDGO : LINKDEBUG);
            } else if (!(state == LINKANDGO || state == LINKDEBUG))
                wstr("Must L(ink first");
            goto out;
        }
    setsegs(w, cf->funit, cf->fheader.dfirstblk);
    *ass_status = FOUND_OK;
    r = 1;
out:
    fclose(cf, CNORMAL);
    return r;
}

/* SYS_ASSOCIATE: a system program (FILENAME), looked for on every disk when
   it is no longer where it was */
int sys_associate(int sys_name)
{
    unsigned char vol[8];
    unsigned char title[16];
    unsigned char ltitle[24];
    struct unitent *u;
    unsigned char *fn;
    int segs;
    int kind;
    int lunit;
    int st;
    int r;
    fn = filename[sys_name];
    r = associate(fn, 0, 0, 0, &st);
    if (st == NOT_FOUND && scantitle(fn, vol, title, &segs, &kind)) {
        lunit = 0;
        do {
            lunit++;
            u = &unitable[lunit];
            if (u->uisblkd) {
                u->uvid[0] = 0;
                if (fetchdir(lunit)) {
                    pcpy(u->uvid, syscom->gdirp->dtid);
                    pcpy(ltitle, u->uvid);
                    sconcat(ltitle, "\001:", 23);
                    sconcat(ltitle, title, 23);
                    if (!peq(ltitle, fn) && associate(ltitle, 0, 0, 0, &st))
                        pcpy(fn, ltitle);
                }
            }
        } while (lunit != MAXUNIT && st == NOT_FOUND);
        r = st == FOUND_OK;
        if (st == NOT_FOUND)
            associate(fn, 0, 0, 1, &st);        /* (for its message) */
    }
    return r;
}

/* STARTCOMPILE: C(ompile and A(ssemble: the source, the code file, then the
   Compiler (or Assembler) runs */
void startcompile(int nextst)
{
    unsigned char text_title[42];
    unsigned char title[42];
    unsigned char code_name[24];
    struct inforec *u;
    int i;
    int sys_type;
    if (nextst == ASSEMONLY) {
        wstr("Assembling");
        sys_type = ASSMBLER;
    } else {
        wstr("Compiling");
        sys_type = COMPILER;
    }
    wstr("...");
    wln();
    if (!sys_associate(sys_type))
        return;
    u = &userinfo;
    if (u->gotsym) {
        pcpy(title, u->symvid);
        sconcat(title, "\001:", 40);
        sconcat(title, u->symtid, 40);
    } else {
        wstr(nextst == ASSEMONLY ? "Assemble" : "Compile");
        wstr(" what text? ");
        freadstring(gfiles[0], text_title, 40);
        freadln(gfiles[0]);
        if (text_title[0] == 0)
            goto out;
        pcpy(title, text_title);
        sconcat(title, "\005.TEXT", 40);
    }
    fopen(u->symfibp, title, 1, NIL);
    if (IORESULT() != INOERROR) {
        wstr("Can't find ");
        wpstr(title);
        u->gotsym = 0;
        goto out;
    }
    /* SYSTEM.SWAPDISK on the volume of the Compiler */
    scopy(filename[sys_type], title, 1, spos("\001:", filename[sys_type]));
    sconcat(title, "\017SYSTEM.SWAPDISK", 40);
    fopen(swapfib, title, 1, NIL);
    pset(code_name, "*SYSTEM.WRK.CODE[*]");
    if (!u->gotsym) {
        wstr("To what codefile? ");
        freadstring(gfiles[0], title, 40);
        freadln(gfiles[0]);
        if (title[0] != 0) {
            if (title[1] == syscom->crtinfo.altmode)
                goto out;
            i = spos("\001$", title);           /* '$': the source's name */
            while (i != 0) {
                sdelete(title, i, 1);
                sinsert(text_title, title, 40, i);
                i = spos("\001$", title);
            }
            if (title[0] > 23)                  /* (the Pascal one: String overflow) */
                title[0] = 23;
            pcpy(code_name, title);
            if (title[title[0]] != ']')
                sconcat(code_name, "\010.CODE[*]", 23);
        }
    }
    fopen(u->codefibp, code_name, 0, NIL);
    if (IORESULT() != INOERROR) {
        wstr("Can't open ");
        wpstr(code_name);
        goto out;
    }
    u->errnum = 0;
    u->errblk = 0;
    u->errsym = 0;
    if (nextst == ASSEMONLY)
        nextst = COMPONLY;
    LEAVE(nextst);
out:
    fclose(u->symfibp, CNORMAL);
    fclose(swapfib, CNORMAL);
}

/* FINISHCOMPILE: after the Compiler: its errors to the Editor, or the new
   code file the work file (and C(ompile and go: run it) */
void finishcompile(void)
{
    struct inforec *u;
    unsigned char *t;
    int n;
    u = &userinfo;
    fclose(u->symfibp, CNORMAL);
    fclose(swapfib, CNORMAL);
    if (syscom->miscinfo & MI_HAS8510A)
        UNITCLEAR(3);
    if (u->errnum > 0) {
        u->gotcode = 0;
        fclose(u->codefibp, CPURGE);
        if (u->errblk > 0) {
            clearscreen();
            wln();
            if (sys_associate(EDITOR))
                LEAVE(SYSPROG);
        }
        return;
    }
    if (!peq(u->codetid, "\017SYSTEM.WRK.CODE")) {
        pcpy(u->codevid, u->codefibp->fvid);
        pcpy(u->codetid, u->codefibp->fheader.dtid);
        if (!peq(u->codetid, "\017SYSTEM.WRK.CODE")) {
            pcpy(u->workvid, u->codevid);
            n = u->codetid[0];
            t = u->codetid + n - 4;
            if (n > 5 && t[0] == '.' && t[1] == 'C' && t[2] == 'O' && t[3] == 'D' && t[4] == 'E')
                scopy(u->codetid, u->worktid, 1, n - 5);
        }
    }
    u->gotcode = 1;                             /* (COMMAND closed the code file) */
    if (state == COMPANDGO || state == COMPDEBUG)
        runworkfile(1, state == COMPANDGO);
}

/* EXECUTE: X(ecute */
void execute(void)
{
    unsigned char title[256];
    int dont_care;
    wstr("Execute");
    if (!(syscom->miscinfo & MI_SLOWTERM))
        wstr(" what file");
    wstr("? ");
    freadstring(gfiles[0], title, 255);
    freadln(gfiles[0]);
    if (title[0] > 0) {
        if (title[title[0]] == '.')
            sdelete(title, title[0], 1);
        else
            sinsert("\005.CODE", title, 255, title[0] + 1);
        if (spos("\001/", title) > 0 || spos("\001\\", title) > 0) {
            if (subexec(title))
                LEAVE(SYSPROG);
        } else if (associate(title, 0, 0, 1, &dont_care))
            LEAVE(SYSPROG);
    }
}

/* SUBEXEC: TITLE a path into sub-volumes (Tiny-C's MKDIR: a data file whose
   blocks 2..5 hold a directory): VOL:A/B/NAME, VOL:/A/NAME or A/NAME, a \ as
   a /.  The code file runs where it is: LOADSEGS with its block on the disk,
   as pexec does */
int subexec(unsigned char *title)
{
    unsigned char saved[82];
    unsigned char part[42];
    int d[1024];
    unsigned char *c;
    int i;
    int j;
    int k;
    int f;
    int lunit;
    int base;
    int len;
    saved[0] = 0;
    if (title[0] > 80)
        goto nofile;
    pcpy(saved, title);
    for (i = 1; i <= title[0]; i++) {
        if (title[i] == '\\')
            title[i] = '/';
        if (title[i] >= 'a' && title[i] <= 'z')
            title[i] = title[i] - 32;
    }
    i = spos("\001:", title);
    if (i < title[0] && title[i + 1] == '/')
        sdelete(title, i + 1, 1);
    i = spos("\001/", title);
    if (i < 2 || i > 41)
        goto nofile;
    scopy(title, part, 1, i - 1);
    sdelete(title, 1, i);
    fopen(userinfo.codefibp, part, 1, NIL);
    if (syscom->iorslt != INOERROR)
        goto nofile;
    lunit = userinfo.codefibp->funit;
    base = userinfo.codefibp->fheader.dfirstblk;
    len = userinfo.codefibp->fheader.dlastblk - base;
    fclose(userinfo.codefibp, CNORMAL);
    c = (unsigned char *)d;
    for (;;) {
        UNITREAD(lunit, d, 2048, base + 2);
        if (IORESULT() != INOERROR)
            goto nofile;
        if (d[0] != 0 || d[1] != 6 || c[6] < 1 || c[6] > 7 || d[8] < 0 || d[8] > 77 ||
            d[7] < 6 || d[7] > len)
            goto nofile;
        i = spos("\001/", title);
        if (i == 0) {
            if (title[0] > 40)
                goto nofile;
            pcpy(part, title);
        } else {
            if (i > 41)
                goto nofile;
            scopy(title, part, 1, i - 1);
        }
        f = 0;
        for (k = 1; k <= d[8]; k++)
            if (c[26 * k + 6] == part[0]) {
                j = 1;
                while (j <= part[0] && c[26 * k + 6 + j] == part[j])
                    j++;
                if (j > part[0])
                    f = k;
            }
        if (f == 0)
            goto nofile;
        if (i == 0) {
            if (__mdi(d[13 * f + 2], 16) != CODEFILE) {
                wpstr(saved);
                wstr(" not code");
                return 0;
            }
            if (!loadsegs(lunit, base + d[13 * f])) {
                wstr("Must L(ink first");
                return 0;
            }
            return 1;
        }
        len = d[13 * f + 1] - d[13 * f];
        base = base + d[13 * f];
        sdelete(title, 1, i);
    }
nofile:
    wstr("No file ");
    wpstr(saved);
    return 0;
}

/* RUNWORKFILE: R(un and D(ebug: the work file's code (compiled first if
   there is none) */
void runworkfile(int oktolink, int runonly)
{
    unsigned char t[82];
    struct inforec *u;
    int dont_care;
    u = &userinfo;
    if (u->gotcode) {
        clearscreen();
        wln();
        pcpy(t, u->codevid);
        sconcat(t, "\001:", 80);
        sconcat(t, u->codetid, 80);
        if (associate(t, oktolink, runonly, 1, &dont_care)) {
            wstr("Running...");
            wln();
            LEAVE(runonly ? SYSPROG : DEBUGCALL);
        }
        if (!(state == LINKANDGO || state == LINKDEBUG))
            u->gotcode = 0;
    } else
        startcompile(runonly ? COMPANDGO : COMPDEBUG);
}
