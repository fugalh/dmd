// Copyright (C) 1985-1998 by Symantec
// Copyright (C) 2000-2011 by Digital Mars
// All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in /dmd/src/dmd/backendlicense.txt
 * or /dm/src/dmd/backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#if !SPP

#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <time.h>
#include        "cc.h"
#include        "el.h"
#include        "oper.h"
#include        "code.h"
#include        "type.h"
#include        "global.h"
#include        "aa.h"
#include        "dt.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

/*************************************
 * Handy function to answer the question: who the heck is generating this piece of code?
 */
inline void ccheck(code *cs)
{
//    if (cs->Iop == LEA && (cs->Irm & 0x3F) == 0x34 && cs->Isib == 7) *(char*)0=0;
//    if (cs->Iop == 0x31) *(char*)0=0;
//    if (cs->Irm == 0x3D) *(char*)0=0;
}

/*****************************
 * Find last code in list.
 */

code *code_last(code *c)
{
    if (c)
    {   while (c->next)
            c = c->next;
    }
    return c;
}

/*****************************
 * Set flag bits on last code in list.
 */

void code_orflag(code *c,unsigned flag)
{
    if (flag && c)
    {   while (c->next)
            c = c->next;
        c->Iflags |= flag;
    }
}

/*****************************
 * Set rex bits on last code in list.
 */

void code_orrex(code *c,unsigned rex)
{
    if (rex && c)
    {   while (c->next)
            c = c->next;
        c->Irex |= rex;
    }
}

/**************************************
 * Set the opcode fields in cs.
 */
code *setOpcode(code *c, code *cs, unsigned op)
{
    cs->Iop = op;
    return c;
}

/*****************************
 * Concatenate two code lists together. Return pointer to result.
 */

#if TX86 && __INTSIZE == 4 && __SC__
__declspec(naked) code * __pascal cat(code *c1,code *c2)
{
    _asm
    {
        mov     EAX,c1-4[ESP]
        mov     ECX,c2-4[ESP]
        test    EAX,EAX
        jne     L6D
        mov     EAX,ECX
        ret     8

L6D:    mov     EDX,EAX
        cmp     dword ptr [EAX],0
        je      L7B
L74:    mov     EDX,[EDX]
        cmp     dword ptr [EDX],0
        jne     L74
L7B:    mov     [EDX],ECX
        ret     8
    }
}
#else
code * __pascal cat(code *c1,code *c2)
{   code **pc;

    if (!c1)
        return c2;
    for (pc = &code_next(c1); *pc; pc = &code_next(*pc))
        ;
    *pc = c2;
    return c1;
}
#endif

code * cat3(code *c1,code *c2,code *c3)
{   code **pc;

    for (pc = &c1; *pc; pc = &code_next(*pc))
        ;
    for (*pc = c2; *pc; pc = &code_next(*pc))
        ;
    *pc = c3;
    return c1;
}

code * cat4(code *c1,code *c2,code *c3,code *c4)
{   code **pc;

    for (pc = &c1; *pc; pc = &code_next(*pc))
        ;
    for (*pc = c2; *pc; pc = &code_next(*pc))
        ;
    for (*pc = c3; *pc; pc = &code_next(*pc))
        ;
    *pc = c4;
    return c1;
}

code * cat6(code *c1,code *c2,code *c3,code *c4,code *c5,code *c6)
{ return cat(cat4(c1,c2,c3,c4),cat(c5,c6)); }

/*****************************
 * Add code to end of linked list.
 * Note that unused operands are garbage.
 * gen1() and gen2() are shortcut routines.
 * Input:
 *      c ->    linked list that code is to be added to end of
 *      cs ->   data for the code
 * Returns:
 *      pointer to start of code list
 */

code *gen(code *c,code *cs)
{   code *ce,*cstart;
    unsigned reg;

#ifdef DEBUG                            /* this is a high usage routine */
    assert(cs);
#endif
    assert(I64 || cs->Irex == 0);
    ce = code_calloc();
    *ce = *cs;
    //printf("ce = %p %02x\n", ce, ce->Iop);
    ccheck(ce);
    if (config.flags4 & CFG4optimized &&
        (ce->Iop == 0x81 || ce->Iop == 0x80) &&
        ce->IFL2 == FLconst &&
        reghasvalue((ce->Iop == 0x80) ? BYTEREGS : ALLREGS,I64 ? ce->IEV2.Vsize_t : ce->IEV2.Vlong,&reg) &&
        !(ce->Iflags & CFopsize && I16)
       )
    {   // See if we can replace immediate instruction with register instruction
        static unsigned char regop[8] =
                { 0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38 };

        //printf("replacing 0x%02x, val = x%lx\n",ce->Iop,ce->IEV2.Vlong);
        ce->Iop = regop[(ce->Irm & modregrm(0,7,0)) >> 3] | (ce->Iop & 1);
        code_newreg(ce, reg);
    }
    code_next(ce) = CNIL;
    if (c)
    {   cstart = c;
        while (code_next(c)) c = code_next(c);  /* find end of list     */
        code_next(c) = ce;                      /* link into list       */
        return cstart;
    }
    return ce;
}

code *gen1(code *c,unsigned op)
{ code *ce,*cstart;

  ce = code_calloc();
  ce->Iop = op;
  ccheck(ce);
  assert(op != LEA);
  if (c)
  {     cstart = c;
        while (code_next(c)) c = code_next(c);  /* find end of list     */
        code_next(c) = ce;                      /* link into list       */
        return cstart;
  }
  return ce;
}

code *gen2(code *c,unsigned op,unsigned rm)
{ code *ce,*cstart;

  cstart = ce = code_calloc();
  /*cxcalloc++;*/
  ce->Iop = op;
  ce->Iea = rm;
  ccheck(ce);
  if (c)
  {     cstart = c;
        while (code_next(c)) c = code_next(c);  /* find end of list     */
        code_next(c) = ce;                      /* link into list       */
  }
  return cstart;
}

code *gen2sib(code *c,unsigned op,unsigned rm,unsigned sib)
{ code *ce,*cstart;

  cstart = ce = code_calloc();
  /*cxcalloc++;*/
  ce->Iop = op;
  ce->Irm = rm;
  ce->Isib = sib;
  ce->Irex = (rm | (sib & (REX_B << 16))) >> 16;
  if (sib & (REX_R << 16))
        ce->Irex |= REX_X;
  ccheck(ce);
  if (c)
  {     cstart = c;
        while (code_next(c)) c = code_next(c);  /* find end of list     */
        code_next(c) = ce;                      /* link into list       */
  }
  return cstart;
}

/********************************
 * Generate an ASM sequence.
 */

code *genasm(code *c,char *s,unsigned slen)
{   code *ce;

    ce = code_calloc();
    ce->Iop = ASM;
    ce->IFL1 = FLasm;
    ce->IEV1.as.len = slen;
    ce->IEV1.as.bytes = (char *) mem_malloc(slen);
    memcpy(ce->IEV1.as.bytes,s,slen);
    return cat(c,ce);
}

code *gencs(code *c,unsigned op,unsigned ea,unsigned FL2,symbol *s)
{   code cs;

    cs.Iop = op;
    cs.Iea = ea;
    ccheck(&cs);
    cs.Iflags = 0;
    cs.IFL2 = FL2;
    cs.IEVsym2 = s;
    cs.IEVoffset2 = 0;

    return gen(c,&cs);
}

code *genc2(code *c,unsigned op,unsigned ea,targ_size_t EV2)
{   code cs;

    cs.Iop = op;
    cs.Iea = ea;
    ccheck(&cs);
    cs.Iflags = CFoff;
    cs.IFL2 = FLconst;
    cs.IEV2.Vsize_t = EV2;
    return gen(c,&cs);
}

/*****************
 * Generate code.
 */

code *genc1(code *c,unsigned op,unsigned ea,unsigned FL1,targ_size_t EV1)
{   code cs;

    assert(FL1 < FLMAX);
    cs.Iop = op;
    cs.Iflags = CFoff;
    cs.Iea = ea;
    ccheck(&cs);
    cs.IFL1 = FL1;
    cs.IEV1.Vsize_t = EV1;
    return gen(c,&cs);
}

/*****************
 * Generate code.
 */

code *genc(code *c,unsigned op,unsigned ea,unsigned FL1,targ_size_t EV1,unsigned FL2,targ_size_t EV2)
{   code cs;

    assert(FL1 < FLMAX);
    cs.Iop = op;
    cs.Iea = ea;
    ccheck(&cs);
    cs.Iflags = CFoff;
    cs.IFL1 = FL1;
    cs.IEV1.Vsize_t = EV1;
    assert(FL2 < FLMAX);
    cs.IFL2 = FL2;
    cs.IEV2.Vsize_t = EV2;
    return gen(c,&cs);
}

/********************************
 * Generate 'instruction' which is actually a line number.
 */

code *genlinnum(code *c,Srcpos srcpos)
{   code cs;

#if 0
    srcpos.print("genlinnum");
#endif
    cs.Iop = ESCAPE | ESClinnum;
    cs.Iflags = 0;
    cs.Irex = 0;
    cs.IFL1 = 0;
    cs.IFL2 = 0;
    cs.IEV1.Vsrcpos = srcpos;
    return gen(c,&cs);
}

/******************************
 * Append line number to existing code.
 */

void cgen_linnum(code **pc,Srcpos srcpos)
{
    *pc = genlinnum(*pc,srcpos);
}

/*****************************
 * Prepend line number to existing code.
 */

void cgen_prelinnum(code **pc,Srcpos srcpos)
{
    *pc = cat(genlinnum(NULL,srcpos),*pc);
}

/********************************
 * Generate 'instruction' which tells the address resolver that the stack has
 * changed.
 */

code *genadjesp(code *c, int offset)
{   code cs;

    if (!I16 && offset)
    {
        cs.Iop = ESCAPE | ESCadjesp;
        cs.Iflags = 0;
        cs.Irex = 0;
        cs.IEV1.Vint = offset;
        return gen(c,&cs);
    }
    else
        return c;
}

/********************************
 * Generate 'instruction' which tells the scheduler that the fpu stack has
 * changed.
 */

code *genadjfpu(code *c, int offset)
{   code cs;

    if (!I16 && offset)
    {
        cs.Iop = ESCAPE | ESCadjfpu;
        cs.Iflags = 0;
        cs.Irex = 0;
        cs.IEV1.Vint = offset;
        return gen(c,&cs);
    }
    else
        return c;
}

/********************************
 * Generate 'nop'
 */

code *gennop(code *c)
{
    return gen1(c,NOP);
}


/****************************************
 * Clean stack after call to codelem().
 */

code *gencodelem(code *c,elem *e,regm_t *pretregs,bool constflag)
{
    if (e)
    {
        unsigned stackpushsave;
        int stackcleansave;

        stackpushsave = stackpush;
        stackcleansave = cgstate.stackclean;
        cgstate.stackclean = 0;                         // defer cleaning of stack
        c = cat(c,codelem(e,pretregs,constflag));
        assert(cgstate.stackclean == 0);
        cgstate.stackclean = stackcleansave;
        c = genstackclean(c,stackpush - stackpushsave,*pretregs);       // do defered cleaning
    }
    return c;
}

/**********************************
 * Determine if one of the registers in regm has value in it.
 * If so, return !=0 and set *preg to which register it is.
 */

bool reghasvalue(regm_t regm,targ_size_t value,unsigned *preg)
{
    //printf("reghasvalue(%s, %llx)\n", regm_str(regm), (unsigned long long)value);
    /* See if another register has the right value      */
    unsigned r = 0;
    for (regm_t mreg = regcon.immed.mval; mreg; mreg >>= 1)
    {
        if (mreg & regm & 1 && regcon.immed.value[r] == value)
        {   *preg = r;
            return TRUE;
        }
        r++;
        regm >>= 1;
    }
    return FALSE;
}

/**************************************
 * Load a register from the mask regm with value.
 * Output:
 *      *preg   the register selected
 */

code *regwithvalue(code *c,regm_t regm,targ_size_t value,unsigned *preg,regm_t flags)
{   unsigned reg;

    if (!preg)
        preg = &reg;

    /* If we don't already have a register with the right value in it   */
    if (!reghasvalue(regm,value,preg))
    {   regm_t save;

        save = regcon.immed.mval;
        c = cat(c,allocreg(&regm,preg,TYint));  // allocate register
        regcon.immed.mval = save;
        c = movregconst(c,*preg,value,flags);   // store value into reg
    }
    return c;
}

/************************
 * When we don't know whether a function symbol is defined or not
 * within this module, we stuff it in this linked list of references
 * to be fixed up later.
 */

struct fixlist
{   //symbol      *Lsymbol;       // symbol we don't know about
    int         Lseg;           // where the fixup is going (CODE or DATA, never UDATA)
    int         Lflags;         // CFxxxx
    targ_size_t Loffset;        // addr of reference to symbol
    targ_size_t Lval;           // value to add into location
#if TARGET_OSX
    symbol      *Lfuncsym;      // function the symbol goes in
#endif
    fixlist *Lnext;             // next in threaded list

    static AArray *start;
    static int nodel;           // don't delete from within searchfixlist
};

AArray *fixlist::start = NULL;
int fixlist::nodel = 0;

/****************************
 * Add to the fix list.
 */

void addtofixlist(symbol *s,targ_size_t soffset,int seg,targ_size_t val,int flags)
{       fixlist *ln;
        static char zeros[8];
        int numbytes;

        //printf("addtofixlist(%p '%s')\n",s,s->Sident);
        assert(flags);
        ln = (fixlist *) mem_calloc(sizeof(fixlist));
        //ln->Lsymbol = s;
        ln->Loffset = soffset;
        ln->Lseg = seg;
        ln->Lflags = flags;
        ln->Lval = val;
#if TARGET_OSX
        ln->Lfuncsym = funcsym_p;
#endif

        if (!fixlist::start)
            fixlist::start = new AArray(&ti_pvoid, sizeof(fixlist *));
        fixlist **pv = (fixlist **)fixlist::start->get(&s);
        ln->Lnext = *pv;
        *pv = ln;

#if TARGET_SEGMENTED
        switch (flags & (CFoff | CFseg))
        {
            case CFoff:         numbytes = tysize[TYnptr];      break;
            case CFseg:         numbytes = 2;                   break;
            case CFoff | CFseg: numbytes = tysize[TYfptr];      break;
            default:            assert(0);
        }
#else
        numbytes = tysize[TYnptr];
        if (I64 && !(flags & CFoffset64))
            numbytes = 4;
        assert(!(flags & CFseg));
#endif
#ifdef DEBUG
        assert(numbytes <= sizeof(zeros));
#endif
        obj_bytes(seg,soffset,numbytes,zeros);
}

/****************************
 * Given a function symbol we've just defined the offset for,
 * search for it in the fixlist, and resolve any matches we find.
 * Input:
 *      s       function symbol just defined
 */

void searchfixlist(symbol *s)
{
    //printf("searchfixlist(%s)\n",s->Sident);
    if (fixlist::start)
    {
        fixlist **lp = (fixlist **)fixlist::start->in(&s);
        if (lp)
        {   fixlist *p;
            while ((p = *lp) != NULL)
            {
                //dbg_printf("Found reference at x%lx\n",p->Loffset);

                // Determine if it is a self-relative fixup we can
                // resolve directly.
                if (s->Sseg == p->Lseg &&
                    (s->Sclass == SCstatic ||
#if TARGET_LINUX || TARGET_OSX || TARGET_FREEBSD || TARGET_OPENBSD || TARGET_SOLARIS
                     (!(config.flags3 & CFG3pic) && s->Sclass == SCglobal)) &&
#else
                        s->Sclass == SCglobal) &&
#endif
                    s->Sxtrnnum == 0 && p->Lflags & CFselfrel)
                {   targ_size_t ad;

                    //printf("Soffset = x%lx, Loffset = x%lx, Lval = x%lx\n",s->Soffset,p->Loffset,p->Lval);
                    ad = s->Soffset - p->Loffset - REGSIZE + p->Lval;
                    obj_bytes(p->Lseg,p->Loffset,REGSIZE,&ad);
                }
                else
                {
#if TARGET_OSX
                    symbol *funcsymsave = funcsym_p;
                    funcsym_p = p->Lfuncsym;
                    reftoident(p->Lseg,p->Loffset,s,p->Lval,p->Lflags);
                    funcsym_p = funcsymsave;
#else
                    reftoident(p->Lseg,p->Loffset,s,p->Lval,p->Lflags);
#endif
                }
                *lp = p->Lnext;
                mem_free(p);            /* remove from list             */
            }
            if (!fixlist::nodel)
                fixlist::start->del(&s);
        }
    }
}

/****************************
 * End of module. Output remaining fixlist elements as references
 * to external symbols.
 */

STATIC int outfixlist_dg(void *parameter, void *pkey, void *pvalue)
{
    //printf("outfixlist_dg(pkey = %p, pvalue = %p)\n", pkey, pvalue);
    symbol *s = *(symbol **)pkey;

    fixlist **plnext = (fixlist **)pvalue;

    while (*plnext)
    {
        fixlist *ln = *plnext;

        symbol_debug(s);
        //printf("outfixlist '%s' offset %04x\n",s->Sident,ln->Loffset);

#if TARGET_SEGMENTED
        if (tybasic(s->ty()) == TYf16func)
        {
            obj_far16thunk(s);          /* make it into a thunk         */
            searchfixlist(s);
        }
        else
#endif
        {
            if (s->Sxtrnnum == 0)
            {   if (s->Sclass == SCstatic)
                {
#if SCPP
                    if (s->Sdt)
                    {
                        outdata(s);
                        searchfixlist(s);
                        continue;
                    }

                    synerr(EM_no_static_def,prettyident(s));    // no definition found for static
#else // MARS
                    printf("Error: no definition for static %s\n",prettyident(s));      // no definition found for static
                    err_exit();                         // BUG: do better
#endif
                }
                if (s->Sflags & SFLwasstatic)
                {
                    // Put it in BSS
                    s->Sclass = SCstatic;
                    s->Sfl = FLunde;
                    dtnzeros(&s->Sdt,type_size(s->Stype));
                    outdata(s);
                    searchfixlist(s);
                    continue;
                }
                s->Sclass = SCextern;   /* make it external             */
                objextern(s);
                if (s->Sflags & SFLweak)
                {
                    obj_wkext(s, NULL);
                }
            }
#if TARGET_OSX
            symbol *funcsymsave = funcsym_p;
            funcsym_p = ln->Lfuncsym;
            reftoident(ln->Lseg,ln->Loffset,s,ln->Lval,ln->Lflags);
            funcsym_p = funcsymsave;
#else
            reftoident(ln->Lseg,ln->Loffset,s,ln->Lval,ln->Lflags);
#endif
            *plnext = ln->Lnext;
#if TERMCODE
            mem_free(ln);
#endif
        }
    }
    return 0;
}

void outfixlist()
{
    //printf("outfixlist()\n");
    if (fixlist::start)
    {
        fixlist::nodel++;
        fixlist::start->apply(NULL, &outfixlist_dg);
        fixlist::nodel--;
#if TERMCODE
        delete fixlist::start;
        fixlist::start = NULL;
#endif
    }
}

#endif // !SPP
