/*
 * wa.h
 * https://gitlab.com/bztsrc/wa
 *
 * Copyright (C) 2025 bzt, MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief Easily embeddable, single header WASM runtime for ANSI C/C++
 *
 */

#ifndef _WA_H_
#define _WA_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* error codes */
enum {
    WA_SUCCESS,         /*  0 no error */
    WA_ERR_MEMORY,      /*  1 memory allocation error */
    WA_ERR_MAGIC,       /*  2 bad magic, not a wasm binary */
    WA_ERR_BOUND,       /*  3 index out of bounds or buffer over- or underflow */
    WA_ERR_NOEND,       /*  4 missing 0x0B end instruction */
    WA_ERR_ELSE,        /*  5 else not matched with if */
    WA_ERR_PROTO,       /*  6 prototype mismatch */
    WA_ERR_NARGS,       /*  7 bad number of arguments */
    WA_ERR_ARITH,       /*  8 arithmetic error, integer overflow or division by zero */
    WA_ERR_UD,          /*  9 undefined or unknown instruction */
    WA_ABORT            /* 10 set this to m->err_code to stop executing */
};

/* run-time linkage */
typedef struct RTLink {
    char      *name;    /* symbol, if NULL, that terminates the list */
    void      *addr;    /* pointer to a global or a function (for host exports) */
    int        fidx;    /* wasm function index (for host imports) */
    uint32_t   type;    /* prototype, return type and arguments */
} RTLink;

/* stack element, also return value to wa_call() */
typedef union {
    uint32_t   u32;     int32_t    i32;  /* int */
    uint64_t   u64;     int64_t    i64;  /* long */
#ifndef WA_NOFLOAT
    float      f32;     double     f64;  /* float */
#endif
} StackValue;

/* type type */
typedef struct Type {
    uint8_t    form, result_count;
    uint16_t   param_count;
} Type;

/* a function or a control block */
typedef struct Block {
    uint16_t   block_type;  /* WA_FUNCTION / WA_BLOCK / loop / if / else / etc. */
    uint16_t   local_count; /* total number of local variables on stack */
    Type       type;        /* return type */
    uint32_t   start_addr;  /* block start, for external WA_FUNCTION this is 0 */
    uint32_t   end_addr;    /* block end */
    uint32_t   else_addr;   /* else address, for external WA_FUNCTION this is an RTLink array index */
    uint32_t   br_addr;     /* break address */
} Block;

/* one function frame on the call stack */
typedef struct Frame {
    uint32_t   block;       /* if most significant bit set, index to functions, otherwise to cache */
    uint32_t   ra;          /* return address */
    int        sp;          /* stack pointer */
    int        fp;          /* frame pointer */
} Frame;

/* table type, used with indirect calls */
typedef struct Table {
    uint8_t    elem_type;   /* not really used */
    uint32_t   maximum;     /* upper limit to offset table elements */
    uint32_t   size;        /* actual number of elements */
    uint32_t  *entries;     /* records */
} Table;

/* memory type */
typedef struct Memory {
    uint8_t   *bytes;       /* buffer to hold the address space */
    uint64_t   size;        /* address space size in bytes */
    uint64_t   start;       /* address space start (required size with WA_ALLOW_GROW / WA_DIRECTMEM) */
    uint64_t   limit;       /* address space end (always WA_MAXMEM with WA_ALLOW_GROW) */
} Memory;

/* data segment */
typedef struct Segment {
    uint32_t   start;       /* segment start (in wasm binary) */
    uint32_t   size;        /* segment size in bytes */
} Segment;

#ifdef WA_DEBUGGER
/* number of breakpoints */
#ifndef WA_NUMBRK
#define WA_NUMBRK 8
#endif
/* breakpoint for the debugger */
typedef struct BreakPoint {
    uint8_t     type;       /* one of the enums */
    uint64_t    addr;       /* the address to watch */
} BreakPoint;
#endif
enum { BRK_NONE, BRK_CODE, BRK_CALL, BRK_GET, BRK_SET, BRK_READ, BRK_WRITE, BRK_GROW };

/* memory restrictions */
#undef WA_NUMBUF
#ifdef WA_MAXALLOC
# if defined(WA_ALLOW_GROW) || defined(WA_DIRECTMEM)
#  error "WA_ALLOW_GROW, WA_DIRECTMEM and WA_MAXALLOC are mutually exclusive"
# endif
# define WA_NUMBUF  WA_MAXALLOC
#else
# if defined(WA_ALLOW_GROW) && defined(WA_DIRECTMEM)
#  error "WA_ALLOW_GROW and WA_DIRECTMEM are mutually exclusive"
# endif
# define WA_NUMBUF  1
#endif
enum { LIDX_MALLOC, LIDX_REALLOC, LIDX_FREE };

/* the main WA context */
typedef struct Module {
    /* raw input buffers (owner: caller) */
    uint32_t    byte_count;
    uint8_t    *bytes;          /* wasm binary, constant, not changed */
    RTLink     *link;           /* run-time linking, only fidx fields changed */
    /* parsed buffers (owner: WA) */
    uint32_t    type_count;     /* from Type(1) section */
    Type       *types;
    uint32_t    function_count; /* from Import(2) and Function(3) sections */
    Block      *functions;
    uint32_t    global_count;   /* from Import(2) and Globals(4) sections */
    StackValue *globals;
    uint64_t   *gptrs;
    Table       table;          /* from Table(4) section */
    Memory      memory[WA_NUMBUF];/* Memory(5) section + dynamically allocated */
    uint32_t    segs_count;     /* from Data(11) section */
    Segment    *segs;
    /* block reference cache */
    Block      *cache;          /* only for block type opcodes */
    uint32_t   *lookup;         /* pc to cache lookup */
    uint32_t    cache_count, lookup_first, lookup_count;
    /* machine state */
    uint32_t    pc;             /* program counter */
    int         sp, sp_count;   /* stack pointer and size */
    int         fp;             /* frame pointer on stack */
    StackValue *stack;
    Frame      *callstack;      /* call stack */
    int         csp, csp_count; /* call stack pointer and size */
    uint32_t   *br_table, br_count;
    /* error reporting */
    uint32_t    err_pc;         /* pc triggerig the error */
    int         err_code;       /* error code */
#ifdef WA_DEBUGGER
    int         single_step;    /* single stepping */
    BreakPoint  breakpoints[WA_NUMBRK];
#endif
} Module;

int wa_init(Module *m, uint8_t *bytes, uint32_t byte_count, RTLink *link);
int wa_sym(Module *m, char *name);
int wa_set(Module *m, int gidx, StackValue value);
StackValue wa_get(Module *m, int gidx);
int wa_push_i32(Module *m, int32_t value);
int wa_push_i64(Module *m, int64_t value);
int wa_push_u32(Module *m, uint32_t value);
int wa_push_u64(Module *m, uint64_t value);
#ifndef WA_NOFLOAT
int wa_push_f32(Module *m, float  value);
int wa_push_f64(Module *m, double value);
#endif
StackValue wa_call(Module *m, int fidx);
int wa_free(Module *m);

#ifdef WA_DEBUGGER
/**
 * Prototype of the debugger callback
 * @param m module instance
 * @param type one of the BRK_* enums or 255 (on start when no constructor)
 * @param addr memory address (or function index with BRK_CALL, global index with BRK_GET/SET)
 */
void WA_DEBUGGER(Module *m, uint8_t type, uint64_t addr);
#endif

/**
 * Platform native call wrapper' prototype if defined. (If not defined, then an universal, but limited dispatcher is used)
 * @param m module instance
 * @param ret the return value must be placed here
 * @param func the address of the native function to be called
 * @param args the address of the arguments array
 * @param type it can either use the prototype, or...
 * @param lidx ...the index to the run-time linkage table
 */
#ifdef WA_DISPATCH
void WA_DISPATCH(Module *m, StackValue *ret, void *func, StackValue *args, uint32_t type, uint32_t lidx);
#endif

/* return type and arguments in RTLink: v = void, i = int (i32/u32), l = long (i64/u64), f = float (f32), d = double (f64)
 * for arguments, only l (i32/u32/i64/u64) allowed, because there are way too many combinations already */
enum {
#ifndef WA_NOFLOAT
WA_v   =00000, WA_i   =00001, WA_l   =00002, WA_f   =00003, WA_d   =00004, WA_vl  =00020, WA_il  =00021, WA_ll  =00022,
WA_fl  =00023, WA_dl  =00024, WA_vf  =00030, WA_if  =00031, WA_lf  =00032, WA_ff  =00033, WA_df  =00034, WA_vd  =00040,
WA_id  =00041, WA_ld  =00042, WA_fd  =00043, WA_dd  =00044, WA_vll =00220, WA_ill =00221, WA_lll =00222, WA_fll =00223,
WA_dll =00224, WA_vfl =00230, WA_ifl =00231, WA_lfl =00232, WA_ffl =00233, WA_dfl =00234, WA_vdl =00240, WA_idl =00241,
WA_ldl =00242, WA_fdl =00243, WA_ddl =00244, WA_vlf =00320, WA_ilf =00321, WA_llf =00322, WA_flf =00323, WA_dlf =00324,
WA_vff =00330, WA_iff =00331, WA_lff =00332, WA_fff =00333, WA_dff =00334, WA_vdf =00340, WA_idf =00341, WA_ldf =00342,
WA_fdf =00343, WA_ddf =00344, WA_vld =00420, WA_ild =00421, WA_lld =00422, WA_fld =00423, WA_dld =00424, WA_vfd =00430,
WA_ifd =00431, WA_lfd =00432, WA_ffd =00433, WA_dfd =00434, WA_vdd =00440, WA_idd =00441, WA_ldd =00442, WA_fdd =00443,
WA_ddd =00444, WA_vlll=02220, WA_illl=02221, WA_llll=02222, WA_flll=02223, WA_dlll=02224, WA_vfll=02230, WA_ifll=02231,
WA_lfll=02232, WA_ffll=02233, WA_dfll=02234, WA_vdll=02240, WA_idll=02241, WA_ldll=02242, WA_fdll=02243, WA_ddll=02244,
WA_vlfl=02320, WA_ilfl=02321, WA_llfl=02322, WA_flfl=02323, WA_dlfl=02324, WA_vffl=02330, WA_iffl=02331, WA_lffl=02332,
WA_fffl=02333, WA_dffl=02334, WA_vdfl=02340, WA_idfl=02341, WA_ldfl=02342, WA_fdfl=02343, WA_ddfl=02344, WA_vldl=02420,
WA_ildl=02421, WA_lldl=02422, WA_fldl=02423, WA_dldl=02424, WA_vfdl=02430, WA_ifdl=02431, WA_lfdl=02432, WA_ffdl=02433,
WA_dfdl=02434, WA_vddl=02440, WA_iddl=02441, WA_lddl=02442, WA_fddl=02443, WA_dddl=02444, WA_vllf=03220, WA_illf=03221,
WA_lllf=03222, WA_fllf=03223, WA_dllf=03224, WA_vflf=03230, WA_iflf=03231, WA_lflf=03232, WA_fflf=03233, WA_dflf=03234,
WA_vdlf=03240, WA_idlf=03241, WA_ldlf=03242, WA_fdlf=03243, WA_ddlf=03244, WA_vlff=03320, WA_ilff=03321, WA_llff=03322,
WA_flff=03323, WA_dlff=03324, WA_vfff=03330, WA_ifff=03331, WA_lfff=03332, WA_ffff=03333, WA_dfff=03334, WA_vdff=03340,
WA_idff=03341, WA_ldff=03342, WA_fdff=03343, WA_ddff=03344, WA_vldf=03420, WA_ildf=03421, WA_lldf=03422, WA_fldf=03423,
WA_dldf=03424, WA_vfdf=03430, WA_ifdf=03431, WA_lfdf=03432, WA_ffdf=03433, WA_dfdf=03434, WA_vddf=03440, WA_iddf=03441,
WA_lddf=03442, WA_fddf=03443, WA_dddf=03444, WA_vlld=04220, WA_illd=04221, WA_llld=04222, WA_flld=04223, WA_dlld=04224,
WA_vfld=04230, WA_ifld=04231, WA_lfld=04232, WA_ffld=04233, WA_dfld=04234, WA_vdld=04240, WA_idld=04241, WA_ldld=04242,
WA_fdld=04243, WA_ddld=04244, WA_vlfd=04320, WA_ilfd=04321, WA_llfd=04322, WA_flfd=04323, WA_dlfd=04324, WA_vffd=04330,
WA_iffd=04331, WA_lffd=04332, WA_fffd=04333, WA_dffd=04334, WA_vdfd=04340, WA_idfd=04341, WA_ldfd=04342, WA_fdfd=04343,
WA_ddfd=04344, WA_vldd=04420, WA_ildd=04421, WA_lldd=04422, WA_fldd=04423, WA_dldd=04424, WA_vfdd=04430, WA_ifdd=04431,
WA_lfdd=04432, WA_ffdd=04433, WA_dfdd=04434, WA_vddd=04440, WA_iddd=04441, WA_lddd=04442, WA_fddd=04443, WA_dddd=04444
#else
WA_v         =00000000000, WA_i         =00000000001, WA_l         =00000000002, WA_vl        =00000000020, WA_il        =00000000021,
WA_ll        =00000000022, WA_vll       =00000000220, WA_ill       =00000000221, WA_lll       =00000000222, WA_vlll      =00000002220,
WA_illl      =00000002221, WA_llll      =00000002222, WA_vllll     =00000022220, WA_illll     =00000022221, WA_lllll     =00000022222,
WA_vlllll    =00000222220, WA_illlll    =00000222221, WA_llllll    =00000222222, WA_vllllll   =00002222220, WA_illllll   =00002222221,
WA_lllllll   =00002222222, WA_vlllllll  =00022222220, WA_illlllll  =00022222221, WA_llllllll  =00022222222, WA_vllllllll =00222222220,
WA_illllllll =00222222221, WA_lllllllll =00222222222, WA_vlllllllll=02222222220, WA_illlllllll=02222222221, WA_llllllllll=02222222222
#endif
};
/* Note that with WA_DISPATCH, anything is possible, use these macros to construct the missing enum prototypes */
#define WA_2(a,b) ((a)|(b<<3))
#define WA_3(a,b,c) ((a)|(b<<3)|(c<<6))
#define WA_4(a,b,c,d) ((a)|(b<<3)|(c<<6)|(d<<9))
#define WA_5(a,b,c,d,e) ((a)|(b<<3)|(c<<6)|(d<<9)|(e<<12))
#define WA_6(a,b,c,d,e,f) ((a)|(b<<3)|(c<<6)|(d<<9)|(e<<12)|(f<<15))
#define WA_7(a,b,c,d,e,f,g) ((a)|(b<<3)|(c<<6)|(d<<9)|(e<<12)|(f<<15)|(g<<18))
#define WA_8(a,b,c,d,e,f,g,h) ((a)|(b<<3)|(c<<6)|(d<<9)|(e<<12)|(f<<15)|(g<<18)|(h<<21))
#define WA_9(a,b,c,d,e,f,g,h,i) ((a)|(b<<3)|(c<<6)|(d<<9)|(e<<12)|(f<<15)|(g<<18)|(h<<21)|(i<<24))
#define WA_10(a,b,c,d,e,f,g,h,i,j) ((a)|(b<<3)|(c<<6)|(d<<9)|(e<<12)|(f<<15)|(g<<18)|(h<<21)|(i<<24)|(j<<27))

#ifdef WA_IMPLEMENTATION

/*****************************************
 *            Private stuff              *
 *****************************************/

#ifndef DBG
#ifdef DEBUG
#define DBG(a) do{(printf a);printf("\n");}while(0)
#define ERR(a) do{printf("ERROR:");(printf a);printf("\n");}while(0)
#else
#define DBG(a)
#define ERR(a)
#endif
#endif
#ifndef LL
#ifdef __WIN32__
#define LL "ll"
#else
#define LL "l"
#endif
#endif

#define WA_SYMSIZE  128
#define WA_PAGESIZE 65536

#define WA_MAGIC    0x6d736100
#define WA_VERSION  0x01
#define WA_FMSK     0x80000000
#define WA_GMSK     1
#define WA_FUNCTION 0
#define WA_TABLE    1
#define WA_MEMORY   2
#define WA_GLOBAL   3
#define WA_BLOCK    0x40

#ifndef WA_MAXMEM
# define WA_MAXMEM  (256*WA_PAGESIZE)
#endif

/* math stuff */
double sqrt(double);
static __inline__ uint32_t rotl32(uint32_t n, uint32_t c) { c = c % 32; c &= 31; return (n << c) | (n >> ((0 - c) & 31)); }
static __inline__ uint32_t rotr32(uint32_t n, uint32_t c) { c = c % 32; c &= 31; return (n >> c) | (n << ((0 - c) & 31)); }
static __inline__ uint64_t rotl64(uint64_t n, uint64_t c) { c = c % 64; c &= 63; return (n << c) | (n >> ((0 - c) & 63)); }
static __inline__ uint64_t rotr64(uint64_t n, uint64_t c) { c = c % 64; c &= 63; return (n >> c) | (n << ((0 - c) & 63)); }
static __inline__ void sext_8_32(uint32_t *val)  { if(*val & 0x80) { *val = *val | 0xffffff00; } }
static __inline__ void sext_16_32(uint32_t *val) { if(*val & 0x8000) { *val = *val | 0xffff0000; } }
static __inline__ void sext_8_64(uint64_t *val)  { if(*val & 0x80) { *val = *val | 0xffffffffffffff00; } }
static __inline__ void sext_16_64(uint64_t *val) { if(*val & 0x8000) { *val = *val | 0xffffffffffff0000; } }
static __inline__ void sext_32_64(uint64_t *val) { if(*val & 0x80000000) { *val = *val | 0xffffffff00000000; } }

#ifndef WA_NOFLOAT
static __inline__ float f32_min(float a, float b)
{
    if (__builtin_isnan(a) || __builtin_isnan(b)) return (0.0f / 0.0f);
    else if (a == 0 && a == b) return __builtin_signbit(a) ? a : b;
    return a > b ? b : a;
}

static __inline__ float f32_max(float a, float b)
{
    if (__builtin_isnan(a) || __builtin_isnan(b)) return (0.0f / 0.0f);
    else if (a == 0 && a == b) return __builtin_signbit(a) ? b : a;
    return a > b ? a : b;
}

static __inline__ double f64_min(double a, double b)
{
    if (__builtin_isnan(a) || __builtin_isnan(b)) return (0.0f / 0.0f);
    else if (a == 0 && a == b) return __builtin_signbit(a) ? a : b;
    return a > b ? b : a;
}

static __inline__ double f64_max(double a, double b)
{
    if (__builtin_isnan(a) || __builtin_isnan(b)) return (0.0f / 0.0f);
    else if (a == 0 && a == b) return __builtin_signbit(a) ? b : a;
    return a > b ? a : b;
}
#endif

/* memory allocation */
static void *wa_recalloc(Module *m, void *ptr, size_t old_nmemb, size_t nmemb, size_t size, uint32_t line)
{
    void *res = realloc(ptr, nmemb * size);
    (void)line;
    if(res == NULL) {
        ERR(("wa_recalloc: could not allocate %u bytes in line %u", (uint32_t)(nmemb * size), line));
        m->err_code = WA_ERR_MEMORY; return NULL;
    }
    memset((uint8_t*)res + old_nmemb * size, 0, (nmemb - old_nmemb) * size);
    return res;
}

/* check memory (address space) size and grow if necessary */
static int wa_check_mem(Module *m)
{
#if defined(WA_ALLOW_GROW) || defined(WA_DIRECTMEM)
    uint64_t limit = (m->memory[0].start + 4095) & ~4095;
    if(limit > m->memory[0].limit) limit = m->memory[0].limit;
    if(m->memory[0].start > m->memory[0].limit) {
        m->memory[0].start = m->memory[0].limit;
        DBG(("  memory cropped at limit to %"LL"u bytes", m->memory[0].limit));
        return WA_NUMBUF;
    }
#else
    uint64_t limit = m->memory[0].limit;
#endif
    if(m->memory[0].size != limit) {
        DBG(("  memory size: %"LL"u pages (%"LL"u bytes, was %"LL"u)", (limit + WA_PAGESIZE - 1) / WA_PAGESIZE, limit, m->memory[0].size));
        if(!(m->memory[0].bytes = wa_recalloc(m, m->memory[0].bytes, m->memory[0].size, limit, sizeof(uint8_t), __LINE__))) return 0;
        m->memory[0].size = limit;
    }
    return 0;
}

/* check stack size (and grow it in smallish steps if necessary) */
static int wa_check_stack(Module *m, int deltasp) {
    int i;

    if(m->csp + 1 >= m->csp_count) {
        i = m->csp_count; m->csp_count += 128;
        m->callstack = wa_recalloc(m, m->callstack, i, m->csp_count, sizeof(Frame), __LINE__);
    }
    if(m->sp + deltasp + 1 >= m->sp_count) {
        i = m->sp_count; m->sp_count += deltasp + 128;
        m->stack = wa_recalloc(m, m->stack, i, m->sp_count, sizeof(StackValue), __LINE__);
    }
    return !m->err_code;
}

/* read various values from buffer with bound check and adjust current position */
static uint64_t wa_read_LEB_(Module *m, uint32_t *pos, uint32_t maxbits, int sign) {
    uint8_t *bytes = m->bytes;
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t bcnt = 0;
    uint32_t startpos = *pos;
    uint64_t byte;

    (void)startpos;
    if(m->err_code) return 0;
    while(1) {
        byte = bytes[*pos];
        *pos += 1;
        result |= ((byte & 0x7f)<<shift);
        shift += 7;
        if((byte & 0x80) == 0) break;
        bcnt += 1;
        if(bcnt > (maxbits + 7 - 1) / 7 || *pos > m->byte_count) {
            ERR(("wa_read_LEB: at byte %d overflow", startpos));
            m->err_code = WA_ERR_BOUND; return 0;
        }
    }
    if(sign && (shift < maxbits) && (byte & 0x40))
        result |= - (1 << shift);
    return result;
}

static __inline__ uint64_t wa_read_LEB(Module *m, uint32_t *pos, uint32_t maxbits) {
    return wa_read_LEB_(m, pos, maxbits, 0);
}

static __inline__ uint64_t wa_read_LEB_signed(Module *m, uint32_t *pos, uint32_t maxbits) {
    return wa_read_LEB_(m, pos, maxbits, 1);
}

static uint8_t wa_read_u8(Module *m, uint32_t *pos) {
    if(m->err_code) return 0;
    *pos += 1;
    if(*pos > m->byte_count) {
        ERR(("wa_read_u8: out of bounds"));
        m->err_code = WA_ERR_BOUND; return 0;
    }
    return ((uint8_t *)(m->bytes + *pos - 1))[0];
}

static uint32_t wa_read_u32(Module *m, uint32_t *pos) {
    if(m->err_code) return 0;
    *pos += 4;
    if(*pos > m->byte_count) {
        ERR(("wa_read_u32: out of bounds"));
        m->err_code = WA_ERR_BOUND; return 0;
    }
    return ((uint32_t *)(m->bytes + *pos - 4))[0];
}

static uint64_t wa_read_u64(Module *m, uint32_t *pos) {
    if(m->err_code) return 0;
    *pos += 8;
    if(*pos > m->byte_count) {
        ERR(("wa_read_u64: out of bounds"));
        m->err_code = WA_ERR_BOUND; return 0;
    }
    return ((uint64_t *)(m->bytes + *pos - 8))[0];
}

static void wa_read_string(Module *m, uint32_t *pos, char *str) {
    uint32_t str_len = wa_read_LEB(m, pos, 32);
    uint32_t len = str_len > WA_SYMSIZE - 1 ? WA_SYMSIZE - 1 : str_len;
    if(*pos + str_len > m->byte_count) {
        ERR(("wa_read_string: out of bounds"));
        m->err_code = WA_ERR_BOUND;
    } else {
        memcpy(str, m->bytes + *pos, len);
        str[len] = 0;
        *pos += str_len;
    }
}

static void wa_read_table_type(Module *m, uint32_t *pos) {
    uint32_t flags, tsize;
    m->table.elem_type = wa_read_LEB(m, pos, 7);
    flags = wa_read_LEB(m, pos, 32);
    tsize = wa_read_LEB(m, pos, 32);
    m->table.size = tsize;
    if(flags & 0x1) {
        tsize = wa_read_LEB(m, pos, 32);
        m->table.maximum = 0x10000 < tsize ? 0x10000 : tsize;
    } else
        m->table.maximum = 0x10000;
    DBG(("  table size: %d", tsize));
}

static void wa_read_memory_type(Module *m, uint32_t *pos) {
    uint32_t flags = wa_read_LEB(m, pos, 32);
    uint64_t pages = wa_read_LEB(m, pos, 32) * WA_PAGESIZE;
    m->memory[0].limit = pages;
    if(flags & 0x1) {
        pages = wa_read_LEB(m, pos, 32) * WA_PAGESIZE;
        if(m->memory[0].limit < pages) m->memory[0].limit = pages;
    }
#ifdef WA_ALLOW_GROW
    if(m->memory[0].limit < WA_MAXMEM) m->memory[0].limit = WA_MAXMEM;
#endif
    if(m->memory[0].limit > WA_MAXMEM) {
        m->memory[0].limit = WA_MAXMEM;
        DBG(("  memory cropped at user provided WA_MAXMEM to %"LL"u bytes", m->memory[0].limit));
    }
}

static uint32_t wa_read_opcode(Module *m, uint32_t *pos) {
    uint32_t opcode = wa_read_u8(m, pos);
    if(opcode >= 0xfb && opcode <= 0xfe) { opcode <<= 8; opcode |= wa_read_u8(m, pos); }
    return opcode;
}

static uint8_t *wa_read_addr(Module *m, int dir, uint64_t offs, uint32_t size)
{
    uint8_t *maddr = NULL;
    uint64_t end = offs + size;
    int n = 0;

    (void)dir;
#ifdef WA_DEBUGGER
    for(n = 0; n < WA_NUMBRK && (m->breakpoints[n].type != dir ||
        m->breakpoints[n].addr < offs || m->breakpoints[n].addr >= end); n++);
    if(n < WA_NUMBRK) WA_DEBUGGER(m, dir, offs);
#endif
#if defined(WA_ALLOW_GROW) || defined(WA_DIRECTMEM)
    if(m->memory[0].limit > end && m->memory[0].start < end) m->memory[0].start = end;
    n = wa_check_mem(m);
    maddr =
#ifdef WA_DIRECTMEM
            offs >= m->memory[0].limit ? (uint8_t*)(uintptr_t)offs :
#endif
            m->memory[0].bytes + offs;
#else
    for(n = 0; n < WA_NUMBUF; n++)
        if(offs >= m->memory[n].start && end < m->memory[n].limit) {
            maddr = m->memory[n].bytes - m->memory[n].start + offs;
            break;
        }
#endif
    if(n == WA_NUMBUF || !maddr) {
        ERR(("wa_interpret: out of bound access %s 0x%"LL"x", dir == BRK_READ ? "read" : "write", offs));
        m->err_code = WA_ERR_BOUND; return NULL;
    }
    return maddr;
}

static uint64_t wa_read_init_value(Module *m, uint32_t *pos) {
    StackValue ret = { 0 };
    uint32_t opcode = wa_read_opcode(m, pos);

    switch(opcode) {
        case 0x41: /* i32.const */ ret.u32 = wa_read_LEB_signed(m, pos, 32); break;
        case 0x42: /* i64.const */ ret.u64 = wa_read_LEB_signed(m, pos, 64); break;
#ifndef WA_NOFLOAT
        case 0x43: /* f32.const */ ret.u32 = wa_read_u32(m, pos); break;
        case 0x44: /* f64.const */ ret.u64 = wa_read_u64(m, pos); break;
#endif
        case 0x23: /* global.get */ ret = wa_get(m, wa_read_LEB(m, pos, 32)); break;
        default: ERR(("wa_read_init_value: unrecognized opcode 0x%x", opcode)); m->err_code = WA_ERR_UD; break;
    }
    /* this must be 0x0B end */
    if(!m->err_code && wa_read_u8(m, pos) != 0x0B) {
        ERR(("wa_read_init_value: missing END"));
        m->err_code = WA_ERR_NOEND; m->err_pc = (*pos) - 1; ret.u64 = 0;
    }
    return ret.u64;
}

/* build up block lookup cache for a function */
static int wa_find_blocks(Module *m, Block *func)
{
    Block    *block;
    int      top = -1;
    uint32_t pos = func->start_addr, i, cnt, opcode;
    uint32_t *blockstack = wa_recalloc(m, NULL, 0, func->end_addr - pos, sizeof(uint32_t), __LINE__);

    while(!m->err_code && pos <= func->end_addr) {
        m->err_pc = pos;
        opcode = wa_read_opcode(m, &pos);
        switch (opcode) {
        case 0x02: /* block */
        case 0x03: /* loop */
        case 0x04: /* if */
            i = m->cache_count++;
            if(!(m->cache = wa_recalloc(m, m->cache, i, m->cache_count, sizeof(Block), __LINE__))) return 0;
            block = &m->cache[i];
            block->block_type = opcode;
            block->type.form = WA_BLOCK;
            block->type.result_count = m->bytes[pos] != 0x40;
            block->start_addr = pos - 1;
            blockstack[++top] = i;
            m->lookup[pos - 1 - m->lookup_first] = i;
            break;
        case 0x05: /* else */
            if(m->cache[blockstack[top]].block_type != 0x04) {
                ERR(("wa_find_blocks: else not matched with if"));
                m->err_code = WA_ERR_ELSE; return 0;
            }
            m->cache[blockstack[top]].else_addr = pos;
            break;
        case 0x0b: /* end */
            if(pos - 1 >= func->end_addr) break;
            if(top < 0) { ERR(("wa_find_blocks: blockstack underflow")); m->err_code = WA_ERR_BOUND; return 0; }
            block = &m->cache[blockstack[top--]];
            block->end_addr = pos - 1;
            /* loop: label after start / block, if: label at end */
            block->br_addr = block->block_type == 0x03 ? block->start_addr + 2 : pos - 1;
/*          DBG(("      block start: 0x%x, end: 0x%x, br_addr: 0x%x, else_addr: 0x%x",
                block->start_addr, block->end_addr, block->br_addr, block->else_addr)); */
            break;
        }
        /* skip immediates */
        switch (opcode) {
        case 0x3f: case 0x40:                                                       /* current_memory, grow_memory */
        case 0x0c: case 0x0d: case 0x10: case 0x12:                                 /* br, br_if, call, return_call */
        case 0x1d: case 0x1e: case 0x20: case 0x21: case 0x22: case 0x23: case 0x24:/* get/local.set, local.tee, get/global.set */
        case 0x41: case 0xfc09: case 0xfc0b: wa_read_LEB(m, &pos, 32); break;       /* i32.const, data.drop, memory.fill */
        case 0x11: case 0x13: wa_read_LEB(m, &pos, 32); wa_read_LEB(m, &pos, 1); break; /* call_indirect, return_call_indirect */
        case 0x42: wa_read_LEB(m, &pos, 64); break;                                 /* i64.const */
        case 0x43: wa_read_u32(m, &pos); break;                                     /* f32.const */
        case 0x44: wa_read_u64(m, &pos); break;                                     /* f64.const */
        case 0x02: case 0x03: case 0x04: wa_read_LEB(m, &pos, 7); break;            /* block, loop, if */
        case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2f:/* *.load*, *.store*, memory.init, memory.copy */
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36:
        case 0x37: case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d:
        case 0x3e: case 0xfc08: case 0xfc0a: wa_read_LEB(m, &pos, 32); wa_read_LEB(m, &pos, 32); break;
        case 0xc7: case 0xc9: case 0xca: wa_read_u8(m, &pos); break;                /* local.get/set/tee fast */
        case 0x0e: cnt = wa_read_LEB(m, &pos, 32);                                  /* br_table */
            for(i = 0; i < cnt; i++) wa_read_LEB(m, &pos, 32);
            wa_read_LEB(m, &pos, 32);
            break;
        }
    }
    if(blockstack) free(blockstack);
    return !m->err_code;
}

/* push a block to the call stack */
static void wa_push_block(Module *m, int block, int sp) {
    if(m->err_code) return;
    if(sp < -1 || sp >= m->sp_count) { ERR(("wa_push_block: invalid stack pointer %d", sp)); m->err_code = WA_ERR_BOUND; return; }
    wa_check_stack(m, 0);
    m->csp++;
    m->callstack[m->csp].block = block;
    m->callstack[m->csp].sp = sp;
    m->callstack[m->csp].fp = m->fp;
    m->callstack[m->csp].ra = m->pc;
}

/* pop a block from the top of the call stack */
static Block *wa_pop_block(Module *m) {
    Block *block = NULL;
    Frame *frame = &m->callstack[m->csp--];
    uint32_t fidx;

    if(m->err_code) return NULL;
    if(m->csp < -1 || (fidx = frame->block & ~WA_FMSK) >= (frame->block & WA_FMSK ? m->function_count : m->cache_count)) {
        ERR(("wa_pop_block: function/block index out of bounds"));
        m->err_code = WA_ERR_BOUND; return NULL;
    }
    m->fp = frame->fp;
    if(frame->block & WA_FMSK) {
        m->pc = frame->ra;
        block = &m->functions[fidx];
    } else
        block = &m->cache[frame->block];
    if(block->type.result_count == 1) {
        if(frame->sp < m->sp && frame->sp + 1 < m->sp_count) {
            m->stack[frame->sp + 1] = m->stack[m->sp];
            m->sp = frame->sp + 1;
        }
    } else {
        if(frame->sp < m->sp)
            m->sp = frame->sp;
    }
    if(m->sp < -1 || m->sp >= m->sp_count) { ERR(("wa_pop_block: invalid stack pointer")); m->err_code = WA_ERR_BOUND; block = NULL; }
    return block;
}

/* do an external (non-wasm) function call */
static int wa_external_call(Module *m, uint32_t fidx) {
    Block  *func = &m->functions[fidx];
    StackValue ret = { 0 }, *args;
    void *addr;
    uint32_t lidx;
#ifdef WA_MAXALLOC
    uint64_t lmax = 0;
    uint32_t midx = WA_NUMBUF, zidx = WA_NUMBUF;
#endif
#if defined(WA_MAXALLOC) || defined(WA_DEBUGGER)
    uint32_t i;
#endif

    if(m->err_code) return 0;
    if(fidx >= m->function_count || m->functions[fidx].start_addr || !m->link || m->functions[fidx].else_addr == -1U ||
      (uint32_t)m->link[m->functions[fidx].else_addr].fidx != fidx || !(addr = m->link[m->functions[fidx].else_addr].addr)) {
        ERR(("wa_external_call: function index out of bounds"));
        m->err_code = WA_ERR_BOUND;
    } else {
#ifdef WA_DEBUGGER
        for(i = 0; i < WA_NUMBRK && (m->breakpoints[i].type != BRK_CALL || m->breakpoints[i].addr != fidx); i++);
        if(i < WA_NUMBRK) WA_DEBUGGER(m, BRK_CALL, fidx);
#endif
        func = &m->functions[fidx];
        lidx = func->else_addr;
        if(m->sp + 1 < func->type.param_count) { ERR(("wa_external_call: stack underflow")); m->err_code = WA_ERR_BOUND; return 0; }
        m->sp -= func->type.param_count;
        args = &m->stack[m->sp + 1];
        ret.u64 = 0;
#ifdef WA_MAXALLOC
        for(i = 0; i < WA_NUMBUF; i++) {
            if(lmax < m->memory[i].limit) lmax = m->memory[i].limit;
            if(i && zidx == WA_NUMBUF && !m->memory[i].limit) zidx = i;
            if(m->memory[i].start == args[0].u64) midx = i;
        }
        if(midx < WA_NUMBUF) {
            if((lidx == LIDX_REALLOC && !args[1].u64) || lidx == LIDX_FREE) {
                args[0].u64 = m->memory[midx].bytes;
                m->memory[midx].bytes = NULL;
                m->memory[midx].start = m->memory[midx].limit = m->memory[midx].size = 0;
            }
            if(lidx == LIDX_REALLOC && args[0].u64 && args[1].u64) {
                args[0].u64 = m->memory[midx].bytes;
                zidx = midx;
            }
        }
#endif
#ifdef WA_DISPATCH
        /* this wrapper should be implemented with a custom switch-case, or in Assembly natively to the host platform */
        WA_DISPATCH(m, &ret, addr, args, m->link[lidx].type, lidx);
#else
        /* hush little gcc, we deliberately cast data pointer to a function pointer */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
        /* this is a portable universal version, with one integer argument kind and only up to 3 arguments */
        switch(m->link[lidx].type) {
#ifndef WA_NOFLOAT
        case WA_v   :           ((void(*)())addr) (); break;
        case WA_i   : ret.u32 = ((uint32_t(*)())addr) (); break;
        case WA_l   : ret.u64 = ((uint64_t(*)())addr) (); break;
        case WA_f   : ret.f32 = ((float(*)())addr) (); break;
        case WA_d   : ret.f64 = ((double(*)())addr) (); break;
        case WA_vl  :           ((void(*)(uint64_t))addr) (args[0].u64); break;
        case WA_il  : ret.u32 = ((uint32_t(*)(uint64_t))addr) (args[0].u64); break;
        case WA_ll  : ret.u64 = ((uint64_t(*)(uint64_t))addr) (args[0].u64); break;
        case WA_fl  : ret.f32 = ((float(*)(uint64_t))addr) (args[0].u64); break;
        case WA_dl  : ret.f64 = ((double(*)(uint64_t))addr) (args[0].u64); break;
        case WA_vf  :           ((void(*)(float))addr) (args[0].f32); break;
        case WA_if  : ret.u32 = ((uint32_t(*)(float))addr) (args[0].f32); break;
        case WA_lf  : ret.u64 = ((uint64_t(*)(float))addr) (args[0].f32); break;
        case WA_ff  : ret.f32 = ((float(*)(float))addr) (args[0].f32); break;
        case WA_df  : ret.f64 = ((double(*)(float))addr) (args[0].f32); break;
        case WA_vd  :           ((void(*)(double))addr) (args[0].f64); break;
        case WA_id  : ret.u32 = ((uint32_t(*)(double))addr) (args[0].f64); break;
        case WA_ld  : ret.u64 = ((uint64_t(*)(double))addr) (args[0].f64); break;
        case WA_fd  : ret.f32 = ((float(*)(double))addr) (args[0].f64); break;
        case WA_dd  : ret.f64 = ((double(*)(double))addr) (args[0].f64); break;
        case WA_vll :           ((void(*)(uint64_t,uint64_t))addr) (args[0].u64, args[1].u64); break;
        case WA_ill : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t))addr) (args[0].u64, args[1].u64); break;
        case WA_lll : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t))addr) (args[0].u64, args[1].u64); break;
        case WA_fll : ret.f32 = ((float(*)(uint64_t,uint64_t))addr) (args[0].u64, args[1].u64); break;
        case WA_dll : ret.f64 = ((double(*)(uint64_t,uint64_t))addr) (args[0].u64, args[1].u64); break;
        case WA_vfl :           ((void(*)(float,uint64_t))addr) (args[0].f32, args[1].u64); break;
        case WA_ifl : ret.u32 = ((uint32_t(*)(float,uint64_t))addr) (args[0].f32, args[1].u64); break;
        case WA_lfl : ret.u64 = ((uint64_t(*)(float,uint64_t))addr) (args[0].f32, args[1].u64); break;
        case WA_ffl : ret.f32 = ((float(*)(float,uint64_t))addr) (args[0].f32, args[1].u64); break;
        case WA_dfl : ret.f64 = ((double(*)(float,uint64_t))addr) (args[0].f32, args[1].u64); break;
        case WA_vdl :           ((void(*)(double,uint64_t))addr) (args[0].f64, args[1].u64); break;
        case WA_idl : ret.u32 = ((uint32_t(*)(double,uint64_t))addr) (args[0].f64, args[1].u64); break;
        case WA_ldl : ret.u64 = ((uint64_t(*)(double,uint64_t))addr) (args[0].f64, args[1].u64); break;
        case WA_fdl : ret.f32 = ((float(*)(double,uint64_t))addr) (args[0].f64, args[1].u64); break;
        case WA_ddl : ret.f64 = ((double(*)(double,uint64_t))addr) (args[0].f64, args[1].u64); break;
        case WA_vlf :           ((void(*)(uint64_t,float))addr) (args[0].u64, args[1].f32); break;
        case WA_ilf : ret.u32 = ((uint32_t(*)(uint64_t,float))addr) (args[0].u64, args[1].f32); break;
        case WA_llf : ret.u64 = ((uint64_t(*)(uint64_t,float))addr) (args[0].u64, args[1].f32); break;
        case WA_flf : ret.f32 = ((float(*)(uint64_t,float))addr) (args[0].u64, args[1].f32); break;
        case WA_dlf : ret.f64 = ((double(*)(uint64_t,float))addr) (args[0].u64, args[1].f32); break;
        case WA_vff :           ((void(*)(float,float))addr) (args[0].f32, args[1].f32); break;
        case WA_iff : ret.u32 = ((uint32_t(*)(float,float))addr) (args[0].f32, args[1].f32); break;
        case WA_lff : ret.u64 = ((uint64_t(*)(float,float))addr) (args[0].f32, args[1].f32); break;
        case WA_fff : ret.f32 = ((float(*)(float,float))addr) (args[0].f32, args[1].f32); break;
        case WA_dff : ret.f64 = ((double(*)(float,float))addr) (args[0].f32, args[1].f32); break;
        case WA_vdf :           ((void(*)(double,float))addr) (args[0].f64, args[1].f32); break;
        case WA_idf : ret.u32 = ((uint32_t(*)(double,float))addr) (args[0].f64, args[1].f32); break;
        case WA_ldf : ret.u64 = ((uint64_t(*)(double,float))addr) (args[0].f64, args[1].f32); break;
        case WA_fdf : ret.f32 = ((float(*)(double,float))addr) (args[0].f64, args[1].f32); break;
        case WA_ddf : ret.f64 = ((double(*)(double,float))addr) (args[0].f64, args[1].f32); break;
        case WA_vld :           ((void(*)(uint64_t,double))addr) (args[0].u64, args[1].f64); break;
        case WA_ild : ret.u32 = ((uint32_t(*)(uint64_t,double))addr) (args[0].u64, args[1].f64); break;
        case WA_lld : ret.u64 = ((uint64_t(*)(uint64_t,double))addr) (args[0].u64, args[1].f64); break;
        case WA_fld : ret.f32 = ((float(*)(uint64_t,double))addr) (args[0].u64, args[1].f64); break;
        case WA_dld : ret.f64 = ((double(*)(uint64_t,double))addr) (args[0].u64, args[1].f64); break;
        case WA_vfd :           ((void(*)(float,double))addr) (args[0].f32, args[1].f64); break;
        case WA_ifd : ret.u32 = ((uint32_t(*)(float,double))addr) (args[0].f32, args[1].f64); break;
        case WA_lfd : ret.u64 = ((uint64_t(*)(float,double))addr) (args[0].f32, args[1].f64); break;
        case WA_ffd : ret.f32 = ((float(*)(float,double))addr) (args[0].f32, args[1].f64); break;
        case WA_dfd : ret.f64 = ((double(*)(float,double))addr) (args[0].f32, args[1].f64); break;
        case WA_vdd :           ((void(*)(double,double))addr) (args[0].f64, args[1].f64); break;
        case WA_idd : ret.u32 = ((uint32_t(*)(double,double))addr) (args[0].f64, args[1].f64); break;
        case WA_ldd : ret.u64 = ((uint64_t(*)(double,double))addr) (args[0].f64, args[1].f64); break;
        case WA_fdd : ret.f32 = ((float(*)(double,double))addr) (args[0].f64, args[1].f64); break;
        case WA_ddd : ret.f64 = ((double(*)(double,double))addr) (args[0].f64, args[1].f64); break;
        case WA_vlll:           ((void(*)(uint64_t,uint64_t,uint64_t))addr) (args[0].u64, args[1].u64, args[2].u64); break;
        case WA_illl: ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t))addr) (args[0].u64, args[1].u64, args[2].u64); break;
        case WA_llll: ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t))addr) (args[0].u64, args[1].u64, args[2].u64); break;
        case WA_flll: ret.f32 = ((float(*)(uint64_t,uint64_t,uint64_t))addr) (args[0].u64, args[1].u64, args[2].u64); break;
        case WA_dlll: ret.f64 = ((double(*)(uint64_t,uint64_t,uint64_t))addr) (args[0].u64, args[1].u64, args[2].u64); break;
        case WA_vfll:           ((void(*)(float,uint64_t,uint64_t))addr) (args[0].f32, args[1].u64, args[2].u64); break;
        case WA_ifll: ret.u32 = ((uint32_t(*)(float,uint64_t,uint64_t))addr) (args[0].f32, args[1].u64, args[2].u64); break;
        case WA_lfll: ret.u64 = ((uint64_t(*)(float,uint64_t,uint64_t))addr) (args[0].f32, args[1].u64, args[2].u64); break;
        case WA_ffll: ret.f32 = ((float(*)(float,uint64_t,uint64_t))addr) (args[0].f32, args[1].u64, args[2].u64); break;
        case WA_dfll: ret.f64 = ((double(*)(float,uint64_t,uint64_t))addr) (args[0].f32, args[1].u64, args[2].u64); break;
        case WA_vdll:           ((void(*)(double,uint64_t,uint64_t))addr) (args[0].f64, args[1].u64, args[2].u64); break;
        case WA_idll: ret.u32 = ((uint32_t(*)(double,uint64_t,uint64_t))addr) (args[0].f64, args[1].u64, args[2].u64); break;
        case WA_ldll: ret.u64 = ((uint64_t(*)(double,uint64_t,uint64_t))addr) (args[0].f64, args[1].u64, args[2].u64); break;
        case WA_fdll: ret.f32 = ((float(*)(double,uint64_t,uint64_t))addr) (args[0].f64, args[1].u64, args[2].u64); break;
        case WA_ddll: ret.f64 = ((double(*)(double,uint64_t,uint64_t))addr) (args[0].f64, args[1].u64, args[2].u64); break;
        case WA_vlfl:           ((void(*)(uint64_t,float,uint64_t))addr) (args[0].u64, args[1].f32, args[2].u64); break;
        case WA_ilfl: ret.u32 = ((uint32_t(*)(uint64_t,float,uint64_t))addr) (args[0].u64, args[1].f32, args[2].u64); break;
        case WA_llfl: ret.u64 = ((uint64_t(*)(uint64_t,float,uint64_t))addr) (args[0].u64, args[1].f32, args[2].u64); break;
        case WA_flfl: ret.f32 = ((float(*)(uint64_t,float,uint64_t))addr) (args[0].u64, args[1].f32, args[2].u64); break;
        case WA_dlfl: ret.f64 = ((double(*)(uint64_t,float,uint64_t))addr) (args[0].u64, args[1].f32, args[2].u64); break;
        case WA_vffl:           ((void(*)(float,float,uint64_t))addr) (args[0].f32, args[1].f32, args[2].u64); break;
        case WA_iffl: ret.u32 = ((uint32_t(*)(float,float,uint64_t))addr) (args[0].f32, args[1].f32, args[2].u64); break;
        case WA_lffl: ret.u64 = ((uint64_t(*)(float,float,uint64_t))addr) (args[0].f32, args[1].f32, args[2].u64); break;
        case WA_fffl: ret.f32 = ((float(*)(float,float,uint64_t))addr) (args[0].f32, args[1].f32, args[2].u64); break;
        case WA_dffl: ret.f64 = ((double(*)(float,float,uint64_t))addr) (args[0].f32, args[1].f32, args[2].u64); break;
        case WA_vdfl:           ((void(*)(double,float,uint64_t))addr) (args[0].f64, args[1].f32, args[2].u64); break;
        case WA_idfl: ret.u32 = ((uint32_t(*)(double,float,uint64_t))addr) (args[0].f64, args[1].f32, args[2].u64); break;
        case WA_ldfl: ret.u64 = ((uint64_t(*)(double,float,uint64_t))addr) (args[0].f64, args[1].f32, args[2].u64); break;
        case WA_fdfl: ret.f32 = ((float(*)(double,float,uint64_t))addr) (args[0].f64, args[1].f32, args[2].u64); break;
        case WA_ddfl: ret.f64 = ((double(*)(double,float,uint64_t))addr) (args[0].f64, args[1].f32, args[2].u64); break;
        case WA_vldl:           ((void(*)(uint64_t,double,uint64_t))addr) (args[0].u64, args[1].f64, args[2].u64); break;
        case WA_ildl: ret.u32 = ((uint32_t(*)(uint64_t,double,uint64_t))addr) (args[0].u64, args[1].f64, args[2].u64); break;
        case WA_lldl: ret.u64 = ((uint64_t(*)(uint64_t,double,uint64_t))addr) (args[0].u64, args[1].f64, args[2].u64); break;
        case WA_fldl: ret.f32 = ((float(*)(uint64_t,double,uint64_t))addr) (args[0].u64, args[1].f64, args[2].u64); break;
        case WA_dldl: ret.f64 = ((double(*)(uint64_t,double,uint64_t))addr) (args[0].u64, args[1].f64, args[2].u64); break;
        case WA_vfdl:           ((void(*)(float,double,uint64_t))addr) (args[0].f32, args[1].f64, args[2].u64); break;
        case WA_ifdl: ret.u32 = ((uint32_t(*)(float,double,uint64_t))addr) (args[0].f32, args[1].f64, args[2].u64); break;
        case WA_lfdl: ret.u64 = ((uint64_t(*)(float,double,uint64_t))addr) (args[0].f32, args[1].f64, args[2].u64); break;
        case WA_ffdl: ret.f32 = ((float(*)(float,double,uint64_t))addr) (args[0].f32, args[1].f64, args[2].u64); break;
        case WA_dfdl: ret.f64 = ((double(*)(float,double,uint64_t))addr) (args[0].f32, args[1].f64, args[2].u64); break;
        case WA_vddl:           ((void(*)(double,double,uint64_t))addr) (args[0].f64, args[1].f64, args[2].u64); break;
        case WA_iddl: ret.u32 = ((uint32_t(*)(double,double,uint64_t))addr) (args[0].f64, args[1].f64, args[2].u64); break;
        case WA_lddl: ret.u64 = ((uint64_t(*)(double,double,uint64_t))addr) (args[0].f64, args[1].f64, args[2].u64); break;
        case WA_fddl: ret.f32 = ((float(*)(double,double,uint64_t))addr) (args[0].f64, args[1].f64, args[2].u64); break;
        case WA_dddl: ret.f64 = ((double(*)(double,double,uint64_t))addr) (args[0].f64, args[1].f64, args[2].u64); break;
        case WA_vllf:           ((void(*)(uint64_t,uint64_t,float))addr) (args[0].u64, args[1].u64, args[2].f32); break;
        case WA_illf: ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,float))addr) (args[0].u64, args[1].u64, args[2].f32); break;
        case WA_lllf: ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,float))addr) (args[0].u64, args[1].u64, args[2].f32); break;
        case WA_fllf: ret.f32 = ((float(*)(uint64_t,uint64_t,float))addr) (args[0].u64, args[1].u64, args[2].f32); break;
        case WA_dllf: ret.f64 = ((double(*)(uint64_t,uint64_t,float))addr) (args[0].u64, args[1].u64, args[2].f32); break;
        case WA_vflf:           ((void(*)(float,uint64_t,float))addr) (args[0].f32, args[1].u64, args[2].f32); break;
        case WA_iflf: ret.u32 = ((uint32_t(*)(float,uint64_t,float))addr) (args[0].f32, args[1].u64, args[2].f32); break;
        case WA_lflf: ret.u64 = ((uint64_t(*)(float,uint64_t,float))addr) (args[0].f32, args[1].u64, args[2].f32); break;
        case WA_fflf: ret.f32 = ((float(*)(float,uint64_t,float))addr) (args[0].f32, args[1].u64, args[2].f32); break;
        case WA_dflf: ret.f64 = ((double(*)(float,uint64_t,float))addr) (args[0].f32, args[1].u64, args[2].f32); break;
        case WA_vdlf:           ((void(*)(double,uint64_t,float))addr) (args[0].f64, args[1].u64, args[2].f32); break;
        case WA_idlf: ret.u32 = ((uint32_t(*)(double,uint64_t,float))addr) (args[0].f64, args[1].u64, args[2].f32); break;
        case WA_ldlf: ret.u64 = ((uint64_t(*)(double,uint64_t,float))addr) (args[0].f64, args[1].u64, args[2].f32); break;
        case WA_fdlf: ret.f32 = ((float(*)(double,uint64_t,float))addr) (args[0].f64, args[1].u64, args[2].f32); break;
        case WA_ddlf: ret.f64 = ((double(*)(double,uint64_t,float))addr) (args[0].f64, args[1].u64, args[2].f32); break;
        case WA_vlff:           ((void(*)(uint64_t,float,float))addr) (args[0].u64, args[1].f32, args[2].f32); break;
        case WA_ilff: ret.u32 = ((uint32_t(*)(uint64_t,float,float))addr) (args[0].u64, args[1].f32, args[2].f32); break;
        case WA_llff: ret.u64 = ((uint64_t(*)(uint64_t,float,float))addr) (args[0].u64, args[1].f32, args[2].f32); break;
        case WA_flff: ret.f32 = ((float(*)(uint64_t,float,float))addr) (args[0].u64, args[1].f32, args[2].f32); break;
        case WA_dlff: ret.f64 = ((double(*)(uint64_t,float,float))addr) (args[0].u64, args[1].f32, args[2].f32); break;
        case WA_vfff:           ((void(*)(float,float,float))addr) (args[0].f32, args[1].f32, args[2].f32); break;
        case WA_ifff: ret.u32 = ((uint32_t(*)(float,float,float))addr) (args[0].f32, args[1].f32, args[2].f32); break;
        case WA_lfff: ret.u64 = ((uint64_t(*)(float,float,float))addr) (args[0].f32, args[1].f32, args[2].f32); break;
        case WA_ffff: ret.f32 = ((float(*)(float,float,float))addr) (args[0].f32, args[1].f32, args[2].f32); break;
        case WA_dfff: ret.f64 = ((double(*)(float,float,float))addr) (args[0].f32, args[1].f32, args[2].f32); break;
        case WA_vdff:           ((void(*)(double,float,float))addr) (args[0].f64, args[1].f32, args[2].f32); break;
        case WA_idff: ret.u32 = ((uint32_t(*)(double,float,float))addr) (args[0].f64, args[1].f32, args[2].f32); break;
        case WA_ldff: ret.u64 = ((uint64_t(*)(double,float,float))addr) (args[0].f64, args[1].f32, args[2].f32); break;
        case WA_fdff: ret.f32 = ((float(*)(double,float,float))addr) (args[0].f64, args[1].f32, args[2].f32); break;
        case WA_ddff: ret.f64 = ((double(*)(double,float,float))addr) (args[0].f64, args[1].f32, args[2].f32); break;
        case WA_vldf:           ((void(*)(uint64_t,double,float))addr) (args[0].u64, args[1].f64, args[2].f32); break;
        case WA_ildf: ret.u32 = ((uint32_t(*)(uint64_t,double,float))addr) (args[0].u64, args[1].f64, args[2].f32); break;
        case WA_lldf: ret.u64 = ((uint64_t(*)(uint64_t,double,float))addr) (args[0].u64, args[1].f64, args[2].f32); break;
        case WA_fldf: ret.f32 = ((float(*)(uint64_t,double,float))addr) (args[0].u64, args[1].f64, args[2].f32); break;
        case WA_dldf: ret.f64 = ((double(*)(uint64_t,double,float))addr) (args[0].u64, args[1].f64, args[2].f32); break;
        case WA_vfdf:           ((void(*)(float,double,float))addr) (args[0].f32, args[1].f64, args[2].f32); break;
        case WA_ifdf: ret.u32 = ((uint32_t(*)(float,double,float))addr) (args[0].f32, args[1].f64, args[2].f32); break;
        case WA_lfdf: ret.u64 = ((uint64_t(*)(float,double,float))addr) (args[0].f32, args[1].f64, args[2].f32); break;
        case WA_ffdf: ret.f32 = ((float(*)(float,double,float))addr) (args[0].f32, args[1].f64, args[2].f32); break;
        case WA_dfdf: ret.f64 = ((double(*)(float,double,float))addr) (args[0].f32, args[1].f64, args[2].f32); break;
        case WA_vddf:           ((void(*)(double,double,float))addr) (args[0].f64, args[1].f64, args[2].f32); break;
        case WA_iddf: ret.u32 = ((uint32_t(*)(double,double,float))addr) (args[0].f64, args[1].f64, args[2].f32); break;
        case WA_lddf: ret.u64 = ((uint64_t(*)(double,double,float))addr) (args[0].f64, args[1].f64, args[2].f32); break;
        case WA_fddf: ret.f32 = ((float(*)(double,double,float))addr) (args[0].f64, args[1].f64, args[2].f32); break;
        case WA_dddf: ret.f64 = ((double(*)(double,double,float))addr) (args[0].f64, args[1].f64, args[2].f32); break;
        case WA_vlld:           ((void(*)(uint64_t,uint64_t,double))addr) (args[0].u64, args[1].u64, args[2].f64); break;
        case WA_illd: ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,double))addr) (args[0].u64, args[1].u64, args[2].f64); break;
        case WA_llld: ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,double))addr) (args[0].u64, args[1].u64, args[2].f64); break;
        case WA_flld: ret.f32 = ((float(*)(uint64_t,uint64_t,double))addr) (args[0].u64, args[1].u64, args[2].f64); break;
        case WA_dlld: ret.f64 = ((double(*)(uint64_t,uint64_t,double))addr) (args[0].u64, args[1].u64, args[2].f64); break;
        case WA_vfld:           ((void(*)(float,uint64_t,double))addr) (args[0].f32, args[1].u64, args[2].f64); break;
        case WA_ifld: ret.u32 = ((uint32_t(*)(float,uint64_t,double))addr) (args[0].f32, args[1].u64, args[2].f64); break;
        case WA_lfld: ret.u64 = ((uint64_t(*)(float,uint64_t,double))addr) (args[0].f32, args[1].u64, args[2].f64); break;
        case WA_ffld: ret.f32 = ((float(*)(float,uint64_t,double))addr) (args[0].f32, args[1].u64, args[2].f64); break;
        case WA_dfld: ret.f64 = ((double(*)(float,uint64_t,double))addr) (args[0].f32, args[1].u64, args[2].f64); break;
        case WA_vdld:           ((void(*)(double,uint64_t,double))addr) (args[0].f64, args[1].u64, args[2].f64); break;
        case WA_idld: ret.u32 = ((uint32_t(*)(double,uint64_t,double))addr) (args[0].f64, args[1].u64, args[2].f64); break;
        case WA_ldld: ret.u64 = ((uint64_t(*)(double,uint64_t,double))addr) (args[0].f64, args[1].u64, args[2].f64); break;
        case WA_fdld: ret.f32 = ((float(*)(double,uint64_t,double))addr) (args[0].f64, args[1].u64, args[2].f64); break;
        case WA_ddld: ret.f64 = ((double(*)(double,uint64_t,double))addr) (args[0].f64, args[1].u64, args[2].f64); break;
        case WA_vlfd:           ((void(*)(uint64_t,float,double))addr) (args[0].u64, args[1].f32, args[2].f64); break;
        case WA_ilfd: ret.u32 = ((uint32_t(*)(uint64_t,float,double))addr) (args[0].u64, args[1].f32, args[2].f64); break;
        case WA_llfd: ret.u64 = ((uint64_t(*)(uint64_t,float,double))addr) (args[0].u64, args[1].f32, args[2].f64); break;
        case WA_flfd: ret.f32 = ((float(*)(uint64_t,float,double))addr) (args[0].u64, args[1].f32, args[2].f64); break;
        case WA_dlfd: ret.f64 = ((double(*)(uint64_t,float,double))addr) (args[0].u64, args[1].f32, args[2].f64); break;
        case WA_vffd:           ((void(*)(float,float,double))addr) (args[0].f32, args[1].f32, args[2].f64); break;
        case WA_iffd: ret.u32 = ((uint32_t(*)(float,float,double))addr) (args[0].f32, args[1].f32, args[2].f64); break;
        case WA_lffd: ret.u64 = ((uint64_t(*)(float,float,double))addr) (args[0].f32, args[1].f32, args[2].f64); break;
        case WA_fffd: ret.f32 = ((float(*)(float,float,double))addr) (args[0].f32, args[1].f32, args[2].f64); break;
        case WA_dffd: ret.f64 = ((double(*)(float,float,double))addr) (args[0].f32, args[1].f32, args[2].f64); break;
        case WA_vdfd:           ((void(*)(double,float,double))addr) (args[0].f64, args[1].f32, args[2].f64); break;
        case WA_idfd: ret.u32 = ((uint32_t(*)(double,float,double))addr) (args[0].f64, args[1].f32, args[2].f64); break;
        case WA_ldfd: ret.u64 = ((uint64_t(*)(double,float,double))addr) (args[0].f64, args[1].f32, args[2].f64); break;
        case WA_fdfd: ret.f32 = ((float(*)(double,float,double))addr) (args[0].f64, args[1].f32, args[2].f64); break;
        case WA_ddfd: ret.f64 = ((double(*)(double,float,double))addr) (args[0].f64, args[1].f32, args[2].f64); break;
        case WA_vldd:           ((void(*)(uint64_t,double,double))addr) (args[0].u64, args[1].f64, args[2].f64); break;
        case WA_ildd: ret.u32 = ((uint32_t(*)(uint64_t,double,double))addr) (args[0].u64, args[1].f64, args[2].f64); break;
        case WA_lldd: ret.u64 = ((uint64_t(*)(uint64_t,double,double))addr) (args[0].u64, args[1].f64, args[2].f64); break;
        case WA_fldd: ret.f32 = ((float(*)(uint64_t,double,double))addr) (args[0].u64, args[1].f64, args[2].f64); break;
        case WA_dldd: ret.f64 = ((double(*)(uint64_t,double,double))addr) (args[0].u64, args[1].f64, args[2].f64); break;
        case WA_vfdd:           ((void(*)(float,double,double))addr) (args[0].f32, args[1].f64, args[2].f64); break;
        case WA_ifdd: ret.u32 = ((uint32_t(*)(float,double,double))addr) (args[0].f32, args[1].f64, args[2].f64); break;
        case WA_lfdd: ret.u64 = ((uint64_t(*)(float,double,double))addr) (args[0].f32, args[1].f64, args[2].f64); break;
        case WA_ffdd: ret.f32 = ((float(*)(float,double,double))addr) (args[0].f32, args[1].f64, args[2].f64); break;
        case WA_dfdd: ret.f64 = ((double(*)(float,double,double))addr) (args[0].f32, args[1].f64, args[2].f64); break;
        case WA_vddd:           ((void(*)(double,double,double))addr) (args[0].f64, args[1].f64, args[2].f64); break;
        case WA_iddd: ret.u32 = ((uint32_t(*)(double,double,double))addr) (args[0].f64, args[1].f64, args[2].f64); break;
        case WA_lddd: ret.u64 = ((uint64_t(*)(double,double,double))addr) (args[0].f64, args[1].f64, args[2].f64); break;
        case WA_fddd: ret.f32 = ((float(*)(double,double,double))addr) (args[0].f64, args[1].f64, args[2].f64); break;
        case WA_dddd: ret.f64 = ((double(*)(double,double,double))addr) (args[0].f64, args[1].f64, args[2].f64); break;
#else
        /* this is a portable universal version, without floats, one integer argument kind and up to 9 arguments */
        case WA_v         :           ((void(*)(void))addr) (); break;
        case WA_i         : ret.u32 = ((uint32_t(*)(void))addr) (); break;
        case WA_l         : ret.u64 = ((uint64_t(*)(void))addr) (); break;
        case WA_vl        :           ((void(*)(uint64_t))addr) (args[1].u64); break;
        case WA_il        : ret.u32 = ((uint32_t(*)(uint64_t))addr) (args[1].u64); break;
        case WA_ll        : ret.u64 = ((uint64_t(*)(uint64_t))addr) (args[1].u64); break;
        case WA_vll       :           ((void(*)(uint64_t,uint64_t))addr) (args[1].u64,args[2].u64); break;
        case WA_ill       : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t))addr) (args[1].u64,args[2].u64); break;
        case WA_lll       : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t))addr) (args[1].u64,args[2].u64); break;
        case WA_vlll      :           ((void(*)(uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64); break;
        case WA_illl      : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64); break;
        case WA_llll      : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64); break;
        case WA_vllll     :           ((void(*)(uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64); break;
        case WA_illll     : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64); break;
        case WA_lllll     : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64); break;
        case WA_vlllll    :           ((void(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64); break;
        case WA_illlll    : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64); break;
        case WA_llllll    : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64); break;
        case WA_vllllll   :           ((void(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64); break;
        case WA_illllll   : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64); break;
        case WA_lllllll   : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64); break;
        case WA_vlllllll  :           ((void(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64); break;
        case WA_illlllll  : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64); break;
        case WA_llllllll  : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64); break;
        case WA_vllllllll :           ((void(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64,args[8].u64); break;
        case WA_illllllll : ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64,args[8].u64); break;
        case WA_lllllllll : ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64,args[8].u64); break;
        case WA_vlllllllll:           ((void(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64,args[8].u64,args[9].u64); break;
        case WA_illlllllll: ret.u32 = ((uint32_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64,args[8].u64,args[9].u64); break;
        case WA_llllllllll: ret.u64 = ((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))addr) (args[1].u64,args[2].u64,args[3].u64,args[4].u64,args[5].u64,args[6].u64,args[7].u64,args[8].u64,args[9].u64); break;
#endif
        default: ERR(("wa_external_call: unknown function prototype")); m->err_code = WA_ERR_BOUND; break;
        }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif /* WA_DISPATCH */
#ifdef WA_MAXALLOC
        if(zidx < WA_NUMBUF && ((lidx == LIDX_REALLOC && args[1].u64) || lidx == LIDX_MALLOC)) {
            m->memory[zidx].bytes = ret.u64;
            m->memory[zidx].start = ret.u64 = lmax;
            m->memory[zidx].size = args[lidx != LIDX_MALLOC].u64;
            m->memory[zidx].limit = lmax + m->memory[i].size;
        }
#endif
        if(func->type.result_count) {
            wa_check_stack(m, func->type.result_count);
            m->sp += func->type.result_count;
            m->stack[m->sp] = ret;
        }
        return 1;
    }
    return 0;
}

/* prepare to do an internal (implemented in wasm) function call (need to call wa_interpret() after) */
static void wa_internal_call(Module *m, uint32_t fidx) {
    Block  *func = &m->functions[fidx];
#ifdef WA_DEBUGGER
    int i;
#endif
    if(m->err_code) return;
    if(fidx >= m->function_count || !m->functions[fidx].start_addr) {
        ERR(("wa_internal_call: function index out of bounds"));
        m->err_code = WA_ERR_BOUND;
    } else {
#ifdef WA_DEBUGGER
        for(i = 0; i < WA_NUMBRK && (m->breakpoints[i].type != BRK_CALL || m->breakpoints[i].addr != fidx); i++);
        if(i < WA_NUMBRK) WA_DEBUGGER(m, BRK_CALL, fidx);
#endif
        func = &m->functions[fidx];
        if(m->sp + 1 < func->type.param_count) { ERR(("wa_internal_call: stack underflow")); m->err_code = WA_ERR_BOUND; return; }
        wa_check_stack(m, func->type.param_count + func->local_count + 1);
        wa_push_block(m, fidx | WA_FMSK, m->sp - func->type.param_count);
        memset(&m->stack[m->sp + 1], 0, func->local_count * sizeof(StackValue));
        m->fp = m->sp - func->type.param_count + 1;
        m->sp += func->local_count;
        m->pc = func->start_addr;
    }
}

/* interpret wasm bytecodes */
static int wa_interpret(Module *m) {
    const uint32_t IMM_SIZE[] = {
        4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4, /* loads  0x28 .. 0x35 */
        4, 8, 4, 8, 1, 2, 1, 2, 4 };              /* stores 0x36 .. 0x3e */
    StackValue  *stack = m->stack;
    Block       *block;
    uint32_t     cur_pc, arg, val, fidx, depth, count, opcode;
    uint8_t     *msrc, *mdst;
    uint32_t     a, b, c; /* I32 math */
    uint64_t     d, e, f; /* I64 math */
#ifndef WA_NOFLOAT
    float        g, h, i; /* F32 math */
    double       j, k, l; /* F64 math */
#endif
    int          n;

    while(!m->err_code && m->pc < m->byte_count) {
        cur_pc = m->err_pc = m->pc;
        opcode = wa_read_opcode(m, &m->pc);
#ifdef WA_DEBUGGER
        if(opcode == 0xdc) { m->single_step = 1; continue; }
        for(n = 0; n < WA_NUMBRK && (m->breakpoints[n].type != BRK_CODE || m->breakpoints[n].addr != cur_pc); n++);
        if(m->single_step || n < WA_NUMBRK) WA_DEBUGGER(m, BRK_CODE, cur_pc);
#endif
        wa_check_stack(m, 0);
        if(m->sp < -1) { ERR(("wa_interpret: stack underflow")); m->err_code = WA_ERR_BOUND; return 0; }

        switch (opcode) {

        /*** Control flow operators ***/
        case 0x00:  /* unreachable */
            ERR(("wa_interpret: unreachable"));
            m->err_code = WA_ERR_UD; return 0;
        case 0x01:  /* nop */
        case 0xdc:  /* breakpoint */
            continue;
        case 0x02:  /* block */
        case 0x03:  /* loop */
        case 0x04:  /* if */
            wa_read_LEB(m, &m->pc, 32);  /* ignore block type */
            n = m->lookup[cur_pc - m->lookup_first];
            wa_push_block(m, n, m->sp);
            if(opcode == 0x04) {
                if(!stack[m->sp--].u32) {
                    /* branch to else block or after end of if */
                    block = &m->cache[n];
                    if(!block->else_addr) {
                        /* no else block, pop if block and skip end */
                        m->csp--;
                        m->pc = block->br_addr + 1;
                    } else
                        m->pc = block->else_addr;
                }
            }
            continue;
        case 0x05:  /* else */
            m->pc = m->cache[m->callstack[m->csp].block].br_addr;
            continue;
        case 0x0b:  /* end */
            if(!(block = wa_pop_block(m))) return 0;
            /* Function with empty callstack, return to top level */
            if(block->block_type == WA_FUNCTION && m->csp == -1)
                return 1;
            continue;
        case 0x0c:  /* br */
            if((m->csp -= wa_read_LEB(m, &m->pc, 32)) < 0) {
                ERR(("wa_interpret: callstack underflow"));
                m->err_code = WA_ERR_BOUND; return 0;
            }
            /* set to end for pop_block */
            m->pc = m->cache[m->callstack[m->csp].block].br_addr;
            continue;
        case 0x0d:  /* br_if */
            count = wa_read_LEB(m, &m->pc, 32);
            if(stack[m->sp--].u32) { /* if true */
                if((m->csp -= count) < 0) return 0;
                m->pc = m->cache[m->callstack[m->csp].block].br_addr;
            }
            continue;
        case 0x0e:  /* br_table */
            if((count = wa_read_LEB(m, &m->pc, 32)) > m->br_count) {
                if(!(m->br_table = wa_recalloc(m, m->br_table, m->br_count, count, sizeof(uint32_t), __LINE__))) return 0;
                m->br_count = count;
            }
            for(arg = 0; !m->err_code && arg < count; arg++)
                m->br_table[arg] = wa_read_LEB(m, &m->pc, 32);
            depth = wa_read_LEB(m, &m->pc, 32);
            n = stack[m->sp--].i32;
            if(n >= 0 && n < (int)count)
                depth = m->br_table[n];
            if((m->csp -= depth) < 0) { ERR(("wa_interpret: callstack underflow")); m->err_code = WA_ERR_BOUND; return 0; }
            m->pc = m->cache[m->callstack[m->csp].block].br_addr;
            continue;
        case 0x0f:  /* return */
            /* Set the program count to the end of the function
               The actual pop_block and return is handled by the end opcode. */
            for(a = 0; m->csp >= 0; m->csp--)
                if(m->callstack[m->csp].block & WA_FMSK) {
                    m->pc = m->functions[m->callstack[m->csp].block & ~WA_FMSK].end_addr;
                    a = 1; break;
                } else
                if(m->cache[m->callstack[m->csp].block].block_type == WA_FUNCTION) {
                    m->pc = m->cache[m->callstack[0].block].end_addr;
                    a = 1; break;
                }
            if(a) continue;
            /* there was no function entry on the call stack? Should never happen! */
            ERR(("wa_interpret: callstack underflow"));
            m->err_code = WA_ERR_BOUND; return 0;

        /*** Call operators ***/
        case 0x10:  /* call */
        case 0x12:  /* return_call */
            fidx = wa_read_LEB(m, &m->pc, 32);
            if(fidx >= m->function_count) { ERR(("wa_interpret: bad function index")); m->err_code = WA_ERR_BOUND; return 0; }
            if(m->functions[fidx].start_addr) {
                wa_internal_call(m, fidx);
                if(m->functions[fidx].type.param_count + m->functions[fidx].local_count != (uint32_t)(m->sp - m->fp + 1)) {
                    ERR(("wa_interpret: call type mismatch (param counts differ)"));
                    m->err_code = WA_ERR_PROTO; return 0;
                }
            } else
                wa_external_call(m, fidx);
            continue;
        case 0x11:  /* call_indirect */
        case 0x13:  /* return_call_indirect */
            wa_read_LEB(m, &m->pc, 32);    /* skip tidx */
            wa_read_LEB(m, &m->pc, 1);     /* reserved immediate */
            val = stack[m->sp--].u32;
            if(val >= m->table.maximum) {
                ERR(("wa_interpret: undefined element 0x%x (max: 0x%x) in table", val, m->table.maximum));
                m->err_code = WA_ERR_BOUND; return 0;
            }
            fidx = m->table.entries[val];
            if(fidx >= m->function_count) { ERR(("wa_interpret: bad function index")); m->err_code = WA_ERR_BOUND; return 0; }
            if(m->functions[fidx].start_addr) {
                wa_internal_call(m, fidx);
                if(m->functions[fidx].type.param_count + m->functions[fidx].local_count != (uint32_t)(m->sp - m->fp + 1)) {
                    ERR(("wa_interpret: call type mismatch (param counts differ)"));
                    m->err_code = WA_ERR_PROTO; return 0;
                }
            } else
                wa_external_call(m, fidx);
            continue;

        /*** Parametric operators ***/
        case 0x1a:  /* drop */
        case 0xc5:  /* drop64 */
            m->sp--;
            continue;
        case 0x1b:  /* select */
        case 0xc6:  /* select64 */
            a = stack[m->sp].u32;
            m->sp -= 2;
            if(!a)  /* use a instead of b */
                stack[m->sp] = stack[m->sp+1];
            continue;


        /*** Variable access ***/
        case 0x20:  /* local.get */
            arg = wa_read_LEB(m, &m->pc, 32);
            stack[++m->sp] = stack[m->fp + arg];
            continue;
        case 0xc7:  /* local.get_fast */
            arg = wa_read_u8(m, &m->pc) & 0x7f;
            stack[++m->sp] = stack[m->fp + arg];
            continue;
        case 0x21:  /* local.set */
            arg = wa_read_LEB(m, &m->pc, 32);
            stack[m->fp + arg] = stack[m->sp--];
            continue;
        case 0xc9:  /* local.set_fast */
            arg = wa_read_u8(m, &m->pc) & 0x7f;
            stack[m->fp + arg] = stack[m->sp--];
            continue;
        case 0x22:  /* local.tee */
            arg = wa_read_LEB(m, &m->pc, 32);
            stack[m->fp + arg] = stack[m->sp];
            continue;
        case 0xca:  /* local.tee_fast */
            arg = wa_read_u8(m, &m->pc) & 0x7f;
            stack[m->fp + arg] = stack[m->sp];
            continue;
        case 0x1d:  /* global64.get */
        case 0x23:  /* global.get */
            arg = wa_read_LEB(m, &m->pc, 32) | (opcode == 0x1d ? 0x80000000 : 0);
            stack[++m->sp] = wa_get(m, arg);
            continue;
        case 0x1e:  /* global64.set */
        case 0x24:  /* global.set */
            arg = wa_read_LEB(m, &m->pc, 32) | (opcode == 0x1e ? 0x80000000 : 0);
            wa_set(m, arg, stack[m->sp--]);
            continue;

        /*** Memory-related operators ***/
        case 0x3f:  /* memory.size */
            wa_read_LEB(m, &m->pc, 32); /* ignore memory index */
            stack[++m->sp].u32 = (m->memory[0].size + WA_PAGESIZE - 1) / WA_PAGESIZE;
            continue;
        case 0x40:  /* memory.grow */
            wa_read_LEB(m, &m->pc, 32); /* ignore memory index */
            arg = stack[m->sp].u32 * WA_PAGESIZE;
            stack[m->sp].u32 = (m->memory[0].size + WA_PAGESIZE - 1) / WA_PAGESIZE;
            d = m->memory[0].size + arg;
#ifdef WA_DEBUGGER
            for(n = 0; n < WA_NUMBRK && (m->breakpoints[n].type != BRK_GROW); n++);
            if(n < WA_NUMBRK) WA_DEBUGGER(m, BRK_GROW, d);
#endif
            if(arg == 0) {
                continue; /* No change */
            } else
#ifdef WA_ALLOW_GROW
            if(d < m->memory[0].limit && d < WA_MAXMEM) {
                m->memory[0].bytes = wa_recalloc(m, m->memory[0].bytes, m->memory[0].size, d, sizeof(uint8_t), __LINE__);
                m->memory[0].start = m->memory[0].size = d;
            } else
#endif
                stack[m->sp].u32 = -1;
            continue;
        case 0xfc08:  /* memory.init */
            c = wa_read_LEB(m, &m->pc, 32);
            wa_read_LEB(m, &m->pc, 32); /* ignore memory index */
            a = stack[m->sp--].u32; /* len */
            b = stack[m->sp--].u32; /* src */
            d = stack[m->sp--].u64; /* dst */
            if(!(mdst = wa_read_addr(m, BRK_WRITE, d, a))) return 0;
            if(c < (uint32_t)m->segs_count && m->segs && b + a < m->segs[c].size)
                memcpy(mdst, m->bytes + m->segs[c].start + b, a);
            else {
                ERR(("wa_interpret: out of bound data segment %u", c));
                m->err_code = WA_ERR_BOUND; return 0;
            }
            continue;
        case 0xfc09:  /* data.drop */
            c = wa_read_LEB(m, &m->pc, 32);
            if(c < (uint32_t)m->segs_count && m->segs)
                m->segs[c].size = 0;
            continue;
        case 0xfc0a:  /* memory.copy */
            wa_read_LEB(m, &m->pc, 32); /* ignore memory index */
            wa_read_LEB(m, &m->pc, 32); /* ignore memory index */
            a = stack[m->sp--].u32; /* len */
            e = stack[m->sp--].u64; /* src */
            d = stack[m->sp--].u64; /* dst */
            if(!(msrc = wa_read_addr(m, BRK_READ,  e, a))) return 0;
            if(!(mdst = wa_read_addr(m, BRK_WRITE, d, a))) return 0;
            if(a > 0 && mdst != msrc) memmove(mdst, msrc, a);
            continue;
        case 0xfc0b:  /* memory.fill */
            wa_read_LEB(m, &m->pc, 32); /* ignore memory index */
            a = stack[m->sp--].u32; /* len */
            b = stack[m->sp--].u32; /* val */
            d = stack[m->sp--].u64; /* dst */
            if(!(mdst = wa_read_addr(m, BRK_WRITE, d, a))) return 0;
            if(a > 0) memset(mdst, b, a);
            continue;

        /* Memory load operators */
        case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2f:
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35:
            wa_read_LEB(m, &m->pc, 32); /* flags */
            d = wa_read_LEB(m, &m->pc, 64);
            e = stack[m->sp].u32;
            a = IMM_SIZE[opcode-0x28];
            if(!(msrc = wa_read_addr(m, BRK_READ, d + e, a))) return 0;
            stack[m->sp].u64 = 0;       /* initialize to 0 */
            memcpy(&stack[m->sp], msrc, a);
            switch(opcode) {            /* sign extend */
                case 0x2c: sext_8_32(&stack[m->sp].u32); break;
                case 0x2e: sext_16_32(&stack[m->sp].u32); break;
                case 0x30: sext_8_64(&stack[m->sp].u64); break;
                case 0x32: sext_16_64(&stack[m->sp].u64); break;
                case 0x34: sext_32_64(&stack[m->sp].u64); break;
            }
            continue;

        /* Memory store operators */
        case 0x36: case 0x37: case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e:
            wa_read_LEB(m, &m->pc, 32); /* flags */
            d = wa_read_LEB(m, &m->pc, 64);
            b = m->sp--;
            e = stack[m->sp--].u32;
            a = IMM_SIZE[opcode-0x28];
            if(!(mdst = wa_read_addr(m, BRK_WRITE, d + e, a))) return 0;
            memcpy(mdst, &stack[b].u64, a);
            continue;

        /*** Constants ***/
        case 0x41: stack[++m->sp].u32 = wa_read_LEB_signed(m, &m->pc, 32); continue;/* i32.const */
        case 0x42: stack[++m->sp].i64 = wa_read_LEB_signed(m, &m->pc, 64); continue;/* i64.const */
#ifndef WA_NOFLOAT
        case 0x43: stack[++m->sp].u32 = wa_read_u32(m, &m->pc); continue;           /* f32.const */
        case 0x44: stack[++m->sp].u64 = wa_read_u64(m, &m->pc); continue;           /* f64.const */
#endif

        /*** Comparison operators ***/

        /* unary */
        case 0x45: stack[m->sp].u32 = stack[m->sp].u32 == 0; continue;     /* i32.eqz */
        case 0x50: stack[m->sp].u32 = stack[m->sp].u64 == 0; continue;     /* i64.eqz */

        /* i32 binary */
        case 0x46: case 0x47: case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
            a = stack[m->sp-1].u32;
            b = stack[m->sp].u32;
            m->sp--;
            switch (opcode) {
            case 0x46: stack[m->sp].u64 = a == b; break;                   /* i32.eq */
            case 0x47: stack[m->sp].u64 = a != b; break;                   /* i32.ne */
            case 0x48: stack[m->sp].u64 = (int32_t)a <  (int32_t)b; break; /* i32.lt_s */
            case 0x49: stack[m->sp].u64 = a <  b; break;                   /* i32.lt_u */
            case 0x4a: stack[m->sp].u64 = (int32_t)a >  (int32_t)b; break; /* i32.gt_s */
            case 0x4b: stack[m->sp].u64 = a >  b; break;                   /* i32.gt_u */
            case 0x4c: stack[m->sp].u64 = (int32_t)a <= (int32_t)b; break; /* i32.le_s */
            case 0x4d: stack[m->sp].u64 = a <= b; break;                   /* i32.le_u */
            case 0x4e: stack[m->sp].u64 = (int32_t)a >= (int32_t)b; break; /* i32.ge_s */
            case 0x4f: stack[m->sp].u64 = a >= b; break;                   /* i32.ge_u */
            }
            continue;
        case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x58: case 0x59: case 0x5a:
            d = stack[m->sp-1].u64;
            e = stack[m->sp].u64;
            m->sp--;
            switch (opcode) {
            case 0x51: stack[m->sp].u64 = d == e; break;                   /* i64.eq */
            case 0x52: stack[m->sp].u64 = d != e; break;                   /* i64.ne */
            case 0x53: stack[m->sp].u64 = (int64_t)d <  (int64_t)e; break; /* i64.lt_s */
            case 0x54: stack[m->sp].u64 = d <  e; break;                   /* i64.lt_u */
            case 0x55: stack[m->sp].u64 = (int64_t)d >  (int64_t)e; break; /* i64.gt_s */
            case 0x56: stack[m->sp].u64 = d >  e; break;                   /* i64.gt_u */
            case 0x57: stack[m->sp].u64 = (int64_t)d <= (int64_t)e; break; /* i64.le_s */
            case 0x58: stack[m->sp].u64 = d <= e; break;                   /* i64.le_u */
            case 0x59: stack[m->sp].u64 = (int64_t)d >= (int64_t)e; break; /* i64.ge_s */
            case 0x5a: stack[m->sp].u64 = d >= e; break;                   /* i64.ge_u */
            }
            continue;
#ifndef WA_NOFLOAT
        case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60:
            g = stack[m->sp-1].f32;
            h = stack[m->sp].f32;
            m->sp--;
            switch (opcode) {
            case 0x5b: stack[m->sp].u64 = g == h; break;                   /* f32.eq */
            case 0x5c: stack[m->sp].u64 = g != h; break;                   /* f32.ne */
            case 0x5d: stack[m->sp].u64 = g <  h; break;                   /* f32.lt */
            case 0x5e: stack[m->sp].u64 = g >  h; break;                   /* f32.gt */
            case 0x5f: stack[m->sp].u64 = g <= h; break;                   /* f32.le */
            case 0x60: stack[m->sp].u64 = g >= h; break;                   /* f32.ge */
            }
            continue;
        case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66:
            j = stack[m->sp-1].f64;
            k = stack[m->sp].f64;
            m->sp--;
            switch (opcode) {
            case 0x61: stack[m->sp].u64 = j == k; break;                   /* f64.eq */
            case 0x62: stack[m->sp].u64 = j != k; break;                   /* f64.ne */
            case 0x63: stack[m->sp].u64 = j <  k; break;                   /* f64.lt */
            case 0x64: stack[m->sp].u64 = j >  k; break;                   /* f64.gt */
            case 0x65: stack[m->sp].u64 = j <= k; break;                   /* f64.le */
            case 0x66: stack[m->sp].u64 = j >= k; break;                   /* f64.ge */
            }
            continue;
#endif

        /*** Numeric operators ***/

        /* unary i32 */
        case 0x67: case 0x68: case 0x69:
            a = stack[m->sp].u32;
            switch (opcode) {
            case 0x67: stack[m->sp].u32 = a==0 ? 32 : __builtin_clz(a); break; /* i32.clz */
            case 0x68: stack[m->sp].u32 = a==0 ? 32 : __builtin_ctz(a); break; /* i32.ctz */
            case 0x69: stack[m->sp].u32 = __builtin_popcount(a); break;        /* i32.popcnt */
            }
            continue;

        /* unary i64 */
        case 0x79: case 0x7a: case 0x7b:
            d = stack[m->sp].u64;
            switch (opcode) {
            case 0x79: stack[m->sp].u64 = d==0 ? 64 : __builtin_clzll(d); break; /* i64.clz */
            case 0x7a: stack[m->sp].u64 = d==0 ? 64 : __builtin_ctzll(d); break; /* i64.ctz */
            case 0x7b: stack[m->sp].u64 = __builtin_popcountll(d); break;        /* i64.popcnt */
            }
            continue;

#ifndef WA_NOFLOAT
        /* unary f32 */
        case 0x8b: stack[m->sp].f32 = __builtin_fabs(stack[m->sp].f32); break;  /* f32.abs */
        case 0x8c: stack[m->sp].f32 = -stack[m->sp].f32; break;                 /* f32.neg */
        case 0x8d: stack[m->sp].f32 = __builtin_ceil(stack[m->sp].f32); break;  /* f32.ceil */
        case 0x8e: stack[m->sp].f32 = __builtin_floor(stack[m->sp].f32); break; /* f32.floor */
        case 0x8f: stack[m->sp].f32 = __builtin_trunc(stack[m->sp].f32); break; /* f32.trunc */
        case 0x90: stack[m->sp].f32 = __builtin_rint(stack[m->sp].f32); break;  /* f32.nearest */

        /* unary f64 */
        case 0x99: stack[m->sp].f64 = __builtin_fabs(stack[m->sp].f64); break;  /* f64.abs */
        case 0x9a: stack[m->sp].f64 = -stack[m->sp].f64; break;                 /* f64.neg */
        case 0x9b: stack[m->sp].f64 = __builtin_ceil(stack[m->sp].f64); break;  /* f64.ceil */
        case 0x9c: stack[m->sp].f64 = __builtin_floor(stack[m->sp].f64); break; /* f64.floor */
        case 0x9d: stack[m->sp].f64 = __builtin_trunc(stack[m->sp].f64); break; /* f64.trunc */
        case 0x9e: stack[m->sp].f64 = __builtin_rint(stack[m->sp].f64); break;  /* f64.nearest */
#ifndef WA_NOLIBM
        case 0x91: stack[m->sp].f32 = (float)sqrt((double)stack[m->sp].f32); break; /* f32.sqrt */
        case 0x9f: stack[m->sp].f64 = sqrt(stack[m->sp].f64); break;                /* f64.sqrt */
#endif
#endif

        /* i32 binary */
        case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f: case 0x70:
        case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: case 0x78:
            a = stack[m->sp-1].u32;
            b = stack[m->sp].u32;
            c = 0;
            m->sp--;
            if(opcode >= 0x6d && opcode <= 0x70 && b == 0) { ERR(("wa_interpret: divide by zero")); m->err_code = WA_ERR_ARITH; return 0; }
            switch (opcode) {
            case 0x6a: c = a + b; break;        /* i32.add */
            case 0x6b: c = a - b; break;        /* i32.sub */
            case 0x6c: c = a * b; break;        /* i32.mul */
            case 0x6d: if (a == 0x80000000 && b == -1U) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                       c = (int32_t)a / (int32_t)b; break;  /* i32.div_s */
            case 0x6e: c = a / b; break;        /* i32.div_u */
            case 0x6f: c = (a == 0x80000000 && b == -1U) ? 0 : (int32_t)a % (int32_t)b; break;  /* i32.rem_s */
            case 0x70: c = a % b; break;        /* i32.rem_u */
            case 0x71: c = a & b; break;        /* i32.and */
            case 0x72: c = a | b; break;        /* i32.or */
            case 0x73: c = a ^ b; break;        /* i32.xor */
            case 0x74: c = a << b; break;       /* i32.shl */
            case 0x75: c = (int32_t)a >> b; break; /* i32.shr_s */
            case 0x76: c = a >> b; break;       /* i32.shr_u */
            case 0x77: c = rotl32(a, b); break; /* i32.rotl */
            case 0x78: c = rotr32(a, b); break; /* i32.rotr */
            }
            stack[m->sp].u32 = c;
            continue;

        /* i64 binary */
        case 0x7c: case 0x7d: case 0x7e: case 0x7f: case 0x80: case 0x81: case 0x82:
        case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: case 0x88: case 0x89: case 0x8a:
            d = stack[m->sp-1].u64;
            e = stack[m->sp].u64;
            f = 0;
            m->sp--;
            if(opcode >= 0x7f && opcode <= 0x82 && e == 0) { ERR(("wa_interpret: divide by zero")); m->err_code = WA_ERR_ARITH; return 0; }
            switch (opcode) {
            case 0x7c: f = d + e; break;        /* i64.add */
            case 0x7d: f = d - e; break;        /* i64.sub */
            case 0x7e: f = d * e; break;        /* i64.mul */
            case 0x7f: if (d == 0x8000000000000000 && e == -1U) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                       f = (int64_t)d / (int64_t)e; break;  /* i64.div_s */
            case 0x80: f = d / e; break;        /* i64.div_u */
            case 0x81: f = d == 0x8000000000000000 && e == -1U ? 0 : (int64_t)d % (int64_t)e; break;  /* i64.rem_s */
            case 0x82: f = d % e; break;        /* i64.rem_u */
            case 0x83: f = d & e; break;        /* i64.and */
            case 0x84: f = d | e; break;        /* i64.or */
            case 0x85: f = d ^ e; break;        /* i64.xor */
            case 0x86: f = d << e; break;       /* i64.shl */
            case 0x87: f = ((int64_t)d) >> e; break; /* i64.shr_s */
            case 0x88: f = d >> e; break;       /* i64.shr_u */
            case 0x89: f = rotl64(d, e); break; /* i64.rotl */
            case 0x8a: f = rotr64(d, e); break; /* i64.rotr */
            }
            stack[m->sp].u64 = f;
            continue;

#ifndef WA_NOFLOAT
        /* f32 binary */
        case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: case 0x98:
            g = stack[m->sp-1].f32;
            h = stack[m->sp].f32;
            i = 0;
            m->sp--;
            switch (opcode) {
            case 0x92: i = g + h; break;        /* f32.add */
            case 0x93: i = g - h; break;        /* f32.sub */
            case 0x94: i = g * h; break;        /* f32.mul */
            case 0x95: i = g / h; break;        /* f32.div */
            case 0x96: i = f32_min(g, h); break;/* f32.min */
            case 0x97: i = f32_max(g, h); break;/* f32.max */
            case 0x98: i = __builtin_signbit(h) ? -__builtin_fabs(g) : __builtin_fabs(g); break; /* f32.copysign */
            }
            stack[m->sp].f32 = i;
            continue;

        /* f64 binary */
        case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6:
            j = stack[m->sp-1].f64;
            k = stack[m->sp].f64;
            l = 0;
            m->sp--;
            switch (opcode) {
            case 0xa0: l = j + k; break;        /* f64.add */
            case 0xa1: l = j - k; break;        /* f64.sub */
            case 0xa2: l = j * k; break;        /* f64.mul */
            case 0xa3: l = j / k; break;        /* f64.div */
            case 0xa4: l = f64_min(j, k); break;/* f64.min */
            case 0xa5: l = f64_max(j, k); break;/* f64.max */
            case 0xa6: l = __builtin_signbit(k) ? -__builtin_fabs(j) : __builtin_fabs(j); break; /* f64.copysign */
            }
            stack[m->sp].f64 = l;
            continue;
#endif

        /*** conversion operations ***/
        case 0xa7: stack[m->sp].u64 &= 0x00000000ffffffff; break;   /* i32.wrap_i64 */
        case 0xac: stack[m->sp].u64 = stack[m->sp].u32; sext_32_64(&stack[m->sp].u64); break;  /* i64.extend_i32_s */
        case 0xad: stack[m->sp].u64 = stack[m->sp].u32; break;      /* i64.extend_i32_u */
#ifndef WA_NOFLOAT
        case 0xa8: if(__builtin_isnan(stack[m->sp].f32)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f32 >= INT32_MAX || stack[m->sp].f32 < INT32_MIN) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].i32 = stack[m->sp].f32; break;      /* i32.trunc_f32_s */
        case 0xa9: if(__builtin_isnan(stack[m->sp].f32)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f32 >= UINT32_MAX || stack[m->sp].f32 <= -1) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].u32 = stack[m->sp].f32; break;      /* i32.trunc_f32_u */
        case 0xaa: if(__builtin_isnan(stack[m->sp].f64)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f64 > INT32_MAX || stack[m->sp].f64 < INT32_MIN) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].i32 = stack[m->sp].f64; break;      /* i32.trunc_f64_s */
        case 0xab: if(__builtin_isnan(stack[m->sp].f64)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f64 > UINT32_MAX || stack[m->sp].f64 <= -1) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].u32 = stack[m->sp].f64; break;      /* i32.trunc_f64_u */
        case 0xae: if(__builtin_isnan(stack[m->sp].f32)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f32 >= INT64_MAX || stack[m->sp].f32 < INT64_MIN) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].i64 = stack[m->sp].f32; break;      /* i64.trunc_f32_s */
        case 0xaf: if(__builtin_isnan(stack[m->sp].f32)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f32 >= UINT64_MAX || stack[m->sp].f32 <= -1) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].u64 = stack[m->sp].f32; break;      /* i64.trunc_f32_u */
        case 0xb0: if(__builtin_isnan(stack[m->sp].f64)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f64 >= INT64_MAX || stack[m->sp].f64 < INT64_MIN) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].i64 = stack[m->sp].f64; break;      /* i64.trunc_f64_s */
        case 0xb1: if(__builtin_isnan(stack[m->sp].f64)) { ERR(("wa_interpret: invalid int conversion")); m->err_code = WA_ERR_ARITH; return 0; }
            else if(stack[m->sp].f64 >= UINT64_MAX || stack[m->sp].f64 <= -1) { ERR(("wa_interpret: int overflow")); m->err_code = WA_ERR_ARITH; return 0; }
                   stack[m->sp].u64 = stack[m->sp].f64; break;      /* i64.trunc_f64_u */
        case 0xb2: stack[m->sp].f32 = stack[m->sp].i32; break;      /* f32.convert_i32_s */
        case 0xb3: stack[m->sp].f32 = stack[m->sp].u32; break;      /* f32.convert_i32_u */
        case 0xb4: stack[m->sp].f32 = stack[m->sp].i64; break;      /* f32.convert_i64_s */
        case 0xb5: stack[m->sp].f32 = stack[m->sp].u64; break;      /* f32.convert_i64_u */
        case 0xb6: stack[m->sp].f32 = stack[m->sp].f64; break;      /* f32.demote_f64 */
        case 0xb7: stack[m->sp].f64 = stack[m->sp].i32; break;      /* f64.convert_i32_s */
        case 0xb8: stack[m->sp].f64 = stack[m->sp].u32; break;      /* f64.convert_i32_u */
        case 0xb9: stack[m->sp].f64 = stack[m->sp].i64; break;      /* f64.convert_i64_s */
        case 0xba: stack[m->sp].f64 = stack[m->sp].u64; break;      /* f64.convert_i64_u */
        case 0xbb: stack[m->sp].f64 = stack[m->sp].f32; break;      /* f64.promote_f32 */
#endif

        /* reinterpretations: i32.reinterpret_f32, i64.reinterpret_f64, f32.reinterpret_i32, f64.reinterpret_i64 */
        case 0xbc: case 0xbd: case 0xbe: case 0xbf: /* nothing to do */ break;

        /* sign extend */
        case 0xc0: stack[m->sp].u64 &= 0xff; sext_8_32(&stack[m->sp].u32); break;       /* i32.extend8_s */
        case 0xc2: stack[m->sp].u64 &= 0xff; sext_8_64(&stack[m->sp].u64); break;       /* i64.extend8_s */
        case 0xc1: stack[m->sp].u64 &= 0xffff; sext_16_32(&stack[m->sp].u32); break;    /* i32.extend16_s */
        case 0xc3: stack[m->sp].u64 &= 0xffff; sext_16_64(&stack[m->sp].u64); break;    /* i64.extend16_s */
        case 0xc4: stack[m->sp].u64 &= 0xffffffff; sext_32_64(&stack[m->sp].u64); break;/* i64.extend32_s */

        default:
            ERR(("wa_interpret: unrecognized opcode 0x%x", opcode));
            m->err_code = WA_ERR_UD; return 0;
        }
    }
    m->err_code = WA_ERR_BOUND;
    return 0; /* We shouldn't reach here */
}

/*****************************************
 *              Public API               *
 *****************************************/

/**
 * Look up an exported symbol, returns fidx / gidx
 * @param m module instance
 * @param name symbol name zero terminated UTF-8
 * @return wasm exported function / global index
 */
int wa_sym(Module *m, char *name) {
    int i;
    DBG(("wa_sym '%s'", name));
    if(m->link && name && *name)
        for(i = 0; m->link[i].name; i++)
            if(!strcmp(name, m->link[i].name))
                return m->link[i].fidx >= 0 ? m->link[i].fidx : -1;
    return -1;
}

/**
 * Set a global variable value (either in host or WASM memory)
 * @param m module instance
 * @param gidx global variable index
 * @param value the new value
 * @return 1 on success, 0 on error
 */
int wa_set(Module *m, int gidx, StackValue value)
{
    int g64 = gidx & 0x80000000, gi = gidx & 0x7fffffff;
#ifdef WA_DEBUGGER
    int n;
    for(n = 0; n < WA_NUMBRK && (m->breakpoints[n].type != BRK_SET || m->breakpoints[n].addr != (uint64_t)gi); n++);
    if(n < WA_NUMBRK) WA_DEBUGGER(m, BRK_SET, gi);
#endif
    if((uint32_t)gi < m->global_count && m->gptrs[gi]) {
/*#ifdef DEBUG
        if(g64 || (m->gptrs[gi] & WA_GMSK)) { DBG(("wa_set %d %016"LL"x", gi, value.u64)); }
        else                                { DBG(("wa_set %d %08x", gi, value.u32)); }
#endif*/
        memcpy((void*)(uintptr_t)(m->gptrs[gi] & ~WA_GMSK), &value, g64 || (m->gptrs[gi] & WA_GMSK) ? 8 : 4);
        return 1;
    } else { ERR(("wa_set: bad global index %d", gi)); m->err_code = WA_ERR_BOUND; }
    return 0;
}

/**
 * Get a global variable value (either from host or WASM memory)
 * @param m module instance
 * @param gidx global variable index
 * @return the variable's value
 */
StackValue wa_get(Module *m, int gidx)
{
    int g64 = gidx & 0x80000000, gi = gidx & 0x7fffffff;
    StackValue value = { 0 };
#ifdef WA_DEBUGGER
    int n;
    for(n = 0; n < WA_NUMBRK && (m->breakpoints[n].type != BRK_GET || m->breakpoints[n].addr != (uint64_t)gi); n++);
    if(n < WA_NUMBRK) WA_DEBUGGER(m, BRK_GET, gi);
#endif
    if((uint32_t)gi < m->global_count && m->gptrs[gi]) {
/*        DBG(("wa_get %d", gi));*/
        memcpy(&value, (void*)(uintptr_t)(m->gptrs[gi] & ~WA_GMSK), g64 || (m->gptrs[gidx] & WA_GMSK) ? 8 : 4);
    } else { ERR(("wa_get: bad global index %d", gi)); m->err_code = WA_ERR_BOUND; }
    return value;
}

/**
 * Pass arguments to wa_call(), call these in argument order, and NOT in reverse order as usual with push.
 * @param m module instance
 * @param value next argument's value
 * @return 1 on success, 0 on error and error code in m->err_code
 */
int wa_push_i32(Module *m, int32_t  value) { DBG(("wa_push_i32 %d", value)); if(!wa_check_stack(m, 1)) return 0; m->stack[++m->sp].i32 = value; return 1; }
int wa_push_i64(Module *m, int64_t  value) { DBG(("wa_push_i64 %"LL"x",value)); if(!wa_check_stack(m, 1)) return 0; m->stack[++m->sp].i64 = value; return 1; }
int wa_push_u32(Module *m, uint32_t value) { DBG(("wa_push_u32 %u", value)); if(!wa_check_stack(m, 1)) return 0; m->stack[++m->sp].u32 = value; return 1; }
int wa_push_u64(Module *m, uint64_t value) { DBG(("wa_push_u64 %"LL"x",value)); if(!wa_check_stack(m, 1)) return 0; m->stack[++m->sp].u64 = value; return 1; }
#ifndef WA_NOFLOAT
int wa_push_f32(Module *m, float    value) { DBG(("wa_push_f32 %g", value)); if(!wa_check_stack(m, 1)) return 0; m->stack[++m->sp].f32 = value; return 1; }
int wa_push_f64(Module *m, double   value) { DBG(("wa_push_f64 %g", value)); if(!wa_check_stack(m, 1)) return 0; m->stack[++m->sp].f64 = value; return 1; }
#endif

/**
 * Do a function call into the module
 * @param m module instance
 * @param fidx function index
 * @return function's return value and error code in m->err_code
 */
StackValue wa_call(Module *m, int fidx) {
    StackValue zero = { 0 };
    if(fidx < 0 || (uint32_t)fidx >= m->function_count || !m->functions[fidx].start_addr) {
        ERR(("wa_call: bad function index %d", fidx));
        m->err_code = WA_ERR_BOUND; return zero;
    }
    DBG(("wa_call: %d (pc 0x%x)", fidx, m->functions[fidx].start_addr));
    wa_internal_call(m, fidx);
    return wa_interpret(m) && !m->err_code && m->sp >= 0 ? m->stack[m->sp] : zero;
}

/**
 * Load a wasm binary into the context
 * @param m module instance
 * @param bytes buffer with the wasm binary
 * @param byte_count length of the wasm binary
 * @param link struct array, run-time linkage table
 * @return 1 on success, 0 on error and error code in m->err_code
 */
int wa_init(Module *m, uint8_t *bytes, uint32_t byte_count, RTLink *link)
{
    Type *type;
    Block *func;
    uint32_t i, j, t, num, pos = 0, start_pos, func_pos, local_count, id, slen, count, idx, kind;
    uint32_t type_index, start_function = -1U, import_fcount = 0, import_gcount = 0;
#ifdef WA_DEBUGGER
    uint32_t first_code = 0;
#endif
    void *addr;
    char name[WA_SYMSIZE], field[WA_SYMSIZE], tmp[2 * WA_SYMSIZE + 3];
#ifdef DBG
    const char *kinds[] = { "function", "table", "memory", "global" };
#endif

    DBG(("wa_init bytes %p byte_count %u", (void*)bytes, byte_count));
    pos = 0;
    m->bytes = bytes;
    m->byte_count = byte_count;
    m->err_code = WA_SUCCESS;
    if(wa_read_u32(m, &pos) != WA_MAGIC || wa_read_u32(m, &pos) != WA_VERSION) {
        ERR(("wa_init: bad magic"));
        m->err_code = WA_ERR_MAGIC; return 0;
    }
#ifdef WA_MAXALLOC
    if(!link ||
      !link[0].name || strcmp(link[0].name, "malloc") ||
      !link[1].name || strcmp(link[1].name, "realloc") ||
      !link[2].name || strcmp(link[2].name, "free")) {
        ERR(("wa_init: WA_MAXALLOC defined but RTLink first 3 function isn't malloc, realloc, free"));
        m->err_code = WA_ERR_PROTO; return 0;
    }
#endif
    if(link) {
        DBG((" Run-Time Linkage table"));
        m->link = link;
        for(i = 0; link[i].name; i++) {
            link[i].fidx = -1U;
#ifdef DEBUG
            for(t = link[i].type >> 3, num = 0; t & 7; t >>= 3, num++);
            DBG(("  link lidx %u: %s, addr %p, type 0x%x (ret %u, args %u)", i, link[i].name, link[i].addr, link[i].type,
                !!(link[i].type & 7), num));
#endif
        }
    }

    m->sp  = -1;
    m->fp  = -1;
    m->csp = -1;
    m->sp_count = 0;  m->stack = NULL;
    m->csp_count = 0; m->callstack = NULL;
    m->function_count = m->global_count = 0;
    memset(m->memory, 0, sizeof(m->memory));

    while(!m->err_code && pos < byte_count) {
        /* read in section header */
        id = wa_read_LEB(m, &pos, 7);
        slen = wa_read_LEB(m, &pos, 32);
        start_pos = pos;
        /* parse section */
        switch(id) {
        case 0:
#ifdef DEBUG
            DBG((" Parsing Custom(0) section (at: 0x%x length: 0x%x)", pos, slen));
            wa_read_string(m, &pos, name);
            DBG(("  Section name '%s'", name));
#endif
            break;
        case 1:
            DBG((" Parsing Type(1) section (at: 0x%x length: 0x%x)", pos, slen));
            m->type_count = wa_read_LEB(m, &pos, 32);
            if(!(m->types = wa_recalloc(m, NULL, 0, m->type_count, sizeof(Type), __LINE__))) return 0;

            for(i = 0; !m->err_code && i < m->type_count; i++) {
                type = &m->types[i];
                type->form = wa_read_LEB(m, &pos, 7);
                type->param_count = wa_read_LEB(m, &pos, 16);
                for(j = 0; !m->err_code && j < type->param_count; j++)
                    wa_read_LEB(m, &pos, 32);
                type->result_count = wa_read_LEB(m, &pos, 7);
                for(j = 0; !m->err_code && j < type->result_count; j++)
                    wa_read_LEB(m, &pos, 32);
                DBG(("  type tidx %d: form 0x%x, ret %d, args %d", i, type->form, type->result_count, type->param_count));
            }
            break;
        case 2:
            DBG((" Parsing Import(2) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  import count: 0x%x", count));
            /* we iterate this twice, first we count how many elements we'll have to allocate */
            for(idx = i = j = 0, num = pos; !m->err_code && idx < count; idx++) {
                wa_read_string(m, &pos, name);
                wa_read_string(m, &pos, field);
                switch(wa_read_u8(m, &pos)) {
                case WA_FUNCTION: wa_read_LEB(m, &pos, 32); i++; break;
                case WA_TABLE:    wa_read_LEB(m, &pos, 7); t = wa_read_LEB(m, &pos, 32); wa_read_LEB(m, &pos, 32); if(t & 0x1) wa_read_LEB(m, &pos, 32); break;
                case WA_MEMORY:   t = wa_read_LEB(m, &pos, 32); wa_read_LEB(m, &pos, 32); if(t & 0x1) wa_read_LEB(m, &pos, 32); break;
                case WA_GLOBAL:   wa_read_LEB(m, &pos, 7); wa_read_LEB(m, &pos, 1); j++; break;
                }
            }
            if(m->err_code) return 0;
            /* allocate memory if needed */
            if(i && !(m->functions = wa_recalloc(m, m->functions, m->function_count, m->function_count + i, sizeof(Block), __LINE__))) return 0;
            if(j && (!(m->globals = wa_recalloc(m, m->globals, m->global_count, m->global_count + j, sizeof(StackValue), __LINE__)) ||
              !(m->gptrs = wa_recalloc(m, m->gptrs, m->global_count, m->global_count + j, sizeof(uint64_t), __LINE__)))) return 0;
            /* go second time */
            for(idx = 0, pos = num; !m->err_code && idx < count; idx++) {
                /* get type */
                wa_read_string(m, &pos, name);
                wa_read_string(m, &pos, field);
                kind = wa_read_u8(m, &pos);
                DBG(("  import: %s.%s, kind %d (%s)", name, field, kind, kind < 4 ? kinds[kind] : "?"));
                type_index = 0;
                switch(kind) {
                case WA_FUNCTION: type_index = wa_read_LEB(m, &pos, 32); break;
                case WA_TABLE:    wa_read_table_type(m, &pos); break;
                case WA_MEMORY:   wa_read_memory_type(m, &pos); break;
                case WA_GLOBAL:   type_index = wa_read_LEB(m, &pos, 7); wa_read_LEB(m, &pos, 1); /* mutability */ break;
                }
                /* lookup symbol */
                addr = NULL; j = 0;
                if(link) {
                    strcpy(tmp, name); strcat(tmp, "."); strcat(tmp, field);
                    for(j = 0; link[j].name; j++)
                        if(!strcmp(tmp, link[j].name)) { addr = link[j].addr; break; }
                    if(!addr)
                        for(j = 0; link[j].name; j++)
                            if(!strcmp(field, link[j].name)) { addr = link[j].addr; break; }
                }
                /* add to the appropriate list */
                switch(kind) {
                case WA_FUNCTION:
                    i = m->function_count++;
                    if(addr) {
                        for(t = link[j].type >> 3, num = 0; t & 7; t >>= 3, num++);
                        if(!!(link[j].type & 7) != m->types[type_index].result_count || num != m->types[type_index].param_count) {
                            ERR(("wa_init: imported function prototype mismatch, ret %u != %u args %u != %u (at %u), %s.%s",
                                !!(link[j].type & 7), m->types[type_index].result_count,
                                num, m->types[type_index].param_count, type_index, name, field));
                            m->err_code = num != m->types[type_index].param_count ? WA_ERR_NARGS : WA_ERR_PROTO;
                            return 0;
                        }
                        link[j].fidx = i;
                    } else j = -1U;
                    func = &m->functions[i];
                    func->block_type = WA_FUNCTION;
                    func->start_addr = 0;
                    func->type = m->types[type_index];
                    func->else_addr = j;
                    DBG(("  function fidx %u: tidx %u (ret %d, args %d), lidx %d, %s.%s%s", i, type_index,
                        m->types[type_index].result_count, m->types[type_index].param_count, j, name, field,
                        j == -1U ? " (symbol not found)" : ""));
                    break;
                case WA_TABLE:
                    if(m->table.entries) ERR(("wa_init: more than 1 table not supported in MVP"));
                    else ERR(("wa_init: imported external table not supported"));
                    m->err_code = WA_ERR_UD; return 0;
                case WA_MEMORY:
                    if(m->table.entries) ERR(("wa_init: more than 1 memory not supported in MVP"));
                    else ERR(("wa_init: imported external memory not supported"));
                    m->err_code = WA_ERR_UD; return 0;
                case WA_GLOBAL:
                    i = m->global_count++;
                    if(addr) {
                        switch(type_index) {
                            case 0x7f: t = WA_i; break; case 0x7e: t = WA_l; break; /* I32, I64 */
                            case 0x7d: t = WA_f; break; case 0x7c: t = WA_d; break; /* F32, F64 */
                            default: t = WA_v; break;
                        }
                        if((link[j].type & 7) != t || t == WA_v) {
                            ERR(("wa_init: imported global type mismatch, type %u != %u (0x%02x), %s.%s",
                                link[j].type & 7, t, type_index, name, field));
                            m->err_code = WA_ERR_PROTO; return 0;
                        }
                        if((uint64_t)(uintptr_t)addr & WA_GMSK) {
                            ERR(("wa_init: misaligned host address, addr %"LL"x != %"LL"x, lidx %d, %s.%s",
                                (uint64_t)(uintptr_t)addr, (uint64_t)(uintptr_t)addr & ~WA_GMSK, j, name, field));
                            m->err_code = WA_ERR_BOUND; return 0;
                        }
                        m->gptrs[i] = (uint64_t)(uintptr_t)addr | (type_index != WA_i && type_index != WA_f ? WA_GMSK : 0);
                        link[j].fidx = i;
                        DBG(("  global gidx %u: %s.%s, type 0x%02x, size %u, lidx %d, from host %p", i, name, field,
                            type_index, type_index == WA_i && type_index == WA_f ? 4 : 8, j, addr));
                    } else {
                        DBG(("  global gidx %u: %s.%s, type 0x%02x, size %u, lidx -1 (symbol not found)", i, name, field,
                            type_index, type_index == 0x7f || type_index == 0x7d ? 4 : 8));
                    }
                    break;
                default: ERR(("wa_init: import of kind %d not supported", kind)); break;
                }
            }
            import_fcount = m->function_count;
            import_gcount = m->global_count;
            break;
        case 3:
            DBG((" Parsing Function(3) section (at: 0x%x length: 0x%x)", pos, slen));
            m->function_count += wa_read_LEB(m, &pos, 32);
            DBG(("  function count: old %d, new %d", import_fcount, m->function_count));
            if(!(m->functions = wa_recalloc(m, m->functions, import_fcount, m->function_count, sizeof(Block), __LINE__))) return 0;
            for (i = import_fcount; !m->err_code && i < m->function_count; i++) {
                idx = wa_read_LEB(m, &pos, 32);
                m->functions[i].block_type = WA_FUNCTION;
                m->functions[i].else_addr = -1U;
                m->functions[i].type = m->types[idx];
                DBG(("  function fidx %u: tidx %u (ret %d, args %d)", i, idx, m->types[idx].result_count, m->types[idx].param_count));
            }
            break;
        case 4:
            DBG((" Parsing Table(4) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  table count: 0x%x", count));
            if(count != 1) { ERR(("wa_init: more than 1 table not supported in MVP")); m->err_code = WA_ERR_BOUND; return 0; }
            wa_read_table_type(m, &pos);
            if(!(m->table.entries = wa_recalloc(m, NULL, 0, m->table.size, sizeof(uint32_t), __LINE__))) return 0;
            break;
        case 5:
            DBG((" Parsing Memory(5) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  memory count: 0x%x", count));
            if(count != 1) { ERR(("wa_init: more than 1 memory not supported in MVP")); m->err_code = WA_ERR_BOUND; return 0; }
            wa_read_memory_type(m, &pos);
#if !defined(WA_ALLOW_GROW) && !defined(WA_DIRECTMEM)
            /* in normal mode we allocate the entire address space in advance with a single malloc call */
            wa_check_mem(m);
#endif
            break;
        case 6:
            DBG((" Parsing Global(6) section (at: 0x%x length: 0x%x)", pos, slen));
            m->global_count += wa_read_LEB(m, &pos, 32);
            DBG(("  global count: old %d, new %d", import_gcount, m->global_count));
            if(!(m->globals = wa_recalloc(m, m->globals, import_gcount, m->global_count, sizeof(StackValue), __LINE__)) ||
              !(m->gptrs = wa_recalloc(m, m->gptrs, import_gcount, m->global_count, sizeof(uint64_t), __LINE__))) return 0;
            for(i = import_gcount; !m->err_code && i < m->global_count; i++) {
                kind = wa_read_LEB(m, &pos, 7); /* content_type */
                wa_read_LEB(m, &pos, 1);        /* mutability */
                m->globals[i].u64 = wa_read_init_value(m, &pos);
                addr = &m->globals[i];
                if((uint64_t)(uintptr_t)addr & WA_GMSK) {
                    ERR(("wa_init: misaligned host address, addr %"LL"x != %"LL"x, gidx %d",
                        (uint64_t)(uintptr_t)addr, (uint64_t)(uintptr_t)addr & ~WA_GMSK, i));
                    m->err_code = WA_ERR_BOUND; return 0;
                }
                m->gptrs[i] = (uint64_t)(uintptr_t)addr | (kind != 0x7f && kind != 0x7d ? WA_GMSK : 0);
#ifdef DEBUG
                switch(kind) {
                    case 0x7f: sprintf(tmp, "%d", m->globals[i].i32); break;
                    case 0x7e: sprintf(tmp, "%"LL"d", m->globals[i].i64); break;
#ifndef WA_NOFLOAT
                    case 0x7d: sprintf(tmp, "%f", m->globals[i].f32); break;
                    case 0x7c: sprintf(tmp, "%g", m->globals[i].f64); break;
#endif
                    default:   sprintf(tmp, "%016"LL"x", m->globals[i].u64); break;
                }
                DBG(("  global gidx %u: type 0x%02x, size %u, %s", i, kind, kind == 0x7f || kind == 0x7d ? 4 : 8, tmp));
#endif
            }
            break;
        case 7:
            DBG((" Parsing Export(7) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  export count: 0x%x", count));
            for(i = 0; !m->err_code && i < count; i++) {
                wa_read_string(m, &pos, name);
                kind = wa_read_u8(m, &pos);
                idx = wa_read_LEB(m, &pos, 32);
                t = -1;
                if(link) {
                    for(j = 0; link[j].name; j++) {
                        if(!strcmp(name, link[j].name)) {
                            for(t = link[j].type >> 3, num = 0; t & 7; t >>= 3, num++);
                            link[j].fidx = idx;
                            if(kind == WA_FUNCTION) {
                                link[j].addr = NULL;
                                if(!!(link[j].type & 7) != m->functions[idx].type.result_count ||
                                   num != m->functions[idx].type.param_count) {
                                    ERR(("wa_init: exported function prototype mismatch, ret %u != %u args %u != %u (at %u), lidx %d, %s",
                                        !!(link[j].type & 7), m->functions[idx].type.result_count,
                                        num, m->functions[idx].type.param_count, idx, j, name));
                                    m->err_code = num != m->functions[idx].type.param_count ? WA_ERR_NARGS : WA_ERR_PROTO;
                                    return 0;
                                }
                            } else
                            if(kind == WA_GLOBAL && link[j].addr && (link[j].type & 7)) {
                                t = link[j].type & 7; num = t == WA_i || t == WA_f ? 4 : 8;
                                if(idx >= m->global_count || (num == 4 && (m->gptrs[idx] & WA_GMSK))) {
                                    ERR(("wa_init: exported global type mismatch, type %u, size %u != %u, lidx %d, %s", t, num,
                                        idx >= m->global_count || (m->gptrs[idx] & WA_GMSK) ? 8 : 4, j, name));
                                    m->err_code = WA_ERR_PROTO; return 0;
                                }
                                addr = link[j].addr;
                                if((uint64_t)(uintptr_t)addr & WA_GMSK) {
                                    ERR(("wa_init: misaligned host address, addr %"LL"x != %"LL"x, lidx %d, %s",
                                        (uint64_t)(uintptr_t)addr, (uint64_t)(uintptr_t)addr & ~WA_GMSK, j, name));
                                    m->err_code = WA_ERR_BOUND; return 0;
                                }
                                DBG(("    storing global %d (type %u size %u) to host %p\n", i, t, num, addr));
                                memcpy(addr, &m->globals[idx], num);
                                m->gptrs[idx] = (uint64_t)(uintptr_t)addr | (num == 8 ? WA_GMSK : 0);
                            }
                            t = j;
                            break;
                        }
                    }
                }
                DBG(("  export: %s, kind %d (%s), %cidx %u, lidx %d", name, kind, kind < 4 ? kinds[kind] : "?",
                    kind < 4 ? kinds[kind][0] : ' ', idx, t));
            }
            break;
        case 8:
            DBG((" Parsing Start(8) section (at: 0x%x length: 0x%x)", pos, slen));
            start_function = wa_read_LEB(m, &pos, 32);
            DBG(("  entry point fidx: %d", start_function));
            break;
        case 9:
            DBG((" Parsing Element(9) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  element count: 0x%x", count));
            for(i = 0; !m->err_code && i < count; i++) {
                idx = wa_read_LEB(m, &pos, 32);
                if(idx) { ERR(("wa_init: more than 1 table not supported in MVP")); m->err_code = WA_ERR_BOUND; return 0; }
                idx = wa_read_init_value(m, &pos);
                num = wa_read_LEB(m, &pos, 32);
                DBG(("  table.entries: offset 0x%x, num %u", idx, num));
                if(idx + num >= m->table.size) {
                    ERR(("wa_init: table overflow %u+%u > %u", idx, num, m->table.size));
                    m->err_code = WA_ERR_BOUND; return 0;
                }
                for(j = 0; !m->err_code && j < num; j++)
                    m->table.entries[idx + j] = wa_read_LEB(m, &pos, 32);
            }
            break;
        case 10:
            DBG((" Parsing Code(10) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  code count: 0x%x", count));
            if(count != m->function_count - import_fcount) {
                ERR(("wa_init: bad number of code sections")); m->err_code = WA_ERR_BOUND; return 0;
            }
            m->cache_count = 0;
            m->lookup_first = pos;
            m->lookup_count = start_pos + slen - pos;
            if(!(m->lookup = wa_recalloc(m, NULL, 0, m->lookup_count, sizeof(uint32_t), __LINE__))) return 0;
            for(i = 0; !m->err_code && i < count; i++) {
                func = &m->functions[import_fcount + i];
                num = wa_read_LEB(m, &pos, 32);
                func_pos = pos;
                local_count = wa_read_LEB(m, &pos, 32);
                func->local_count = 0;
                for(j = 0; !m->err_code && j < local_count; j++) {
                    func->local_count += wa_read_LEB(m, &pos, 32);
                    wa_read_LEB(m, &pos, 7);
                }
                func->start_addr = pos;
                func->end_addr = func_pos + num - 1;
                func->br_addr = func->end_addr;
                func->else_addr = -1U;
                if(func->end_addr >= byte_count || bytes[func->end_addr] != 0x0B) {
                    ERR(("wa_init: code section %u does not end with 0x0B", i));
                    m->err_code = WA_ERR_NOEND; return 0;
                }
                wa_find_blocks(m, func);
                pos = func->end_addr + 1;
                DBG(("  code: at 0x%x, size 0x%x, locals %u, fidx %u", func->start_addr, num, func->local_count, import_fcount + i));
#ifdef WA_DEBUGGER
                if(!first_code) first_code = func->start_addr;
#endif
            }
            if(!m->cache_count) { free(m->lookup); m->lookup = NULL; }
            DBG(("  code cache: count %u, lookup %u", m->cache_count, m->lookup ? m->lookup_count : 0));
            break;
        case 11:
            DBG((" Parsing Data(11) section (at: 0x%x length: 0x%x)", pos, slen));
            count = wa_read_LEB(m, &pos, 32);
            DBG(("  data count: old %d, new %d", m->segs_count, m->segs_count + count));
            if(!(m->segs = wa_recalloc(m, m->segs, m->segs_count, m->segs_count + count, sizeof(Segment), __LINE__))) return 0;
#if defined(WA_ALLOW_GROW) || defined(WA_DIRECTMEM)
            /* in allow grow mode we iterate twice, first we get the upper bound and allocate if necessary */
            for(i = 0, t = pos; !m->err_code && i < count; i++, pos += num) {
                wa_read_LEB(m, &pos, 32);
                idx = wa_read_init_value(m, &pos);
                num = wa_read_LEB(m, &pos, 32);
                if(idx + num >= m->memory[0].start) m->memory[0].start = idx + num;
            }
            wa_check_mem(m);
            pos = t;
#endif
            for(i = 0; !m->err_code && i < count; i++) {
                idx = wa_read_LEB(m, &pos, 32);
                if(idx) { ERR(("wa_init: more than 1 memory not supported in MVP")); m->err_code = WA_ERR_BOUND; return 0; }
                idx = wa_read_init_value(m, &pos);
                num = wa_read_LEB(m, &pos, 32);
                DBG(("  memory: offset 0x%x, size %u", idx, num));
                if(idx + num >= m->memory[0].limit) {
                    ERR(("wa_init: memory overflow %u+%u > %"LL"u", idx, num, m->memory[0].limit));
                    m->err_code = WA_ERR_BOUND; return 0;
                }
                m->segs[m->segs_count].start = pos;
                m->segs[m->segs_count++].size = num;
                memcpy(m->memory[0].bytes + idx, bytes + pos, num);
                pos += num;
            }
            break;
        default:
            DBG((" Unknown(%u) section (at: 0x%x length: 0x%x)", id, pos, slen));
            break;
        }
        pos = start_pos + slen;
    }
#ifdef WA_DEBUGGER
    if(m->single_step && start_function >= m->function_count) { m->pc = first_code; WA_DEBUGGER(m, 255, first_code); }
#endif
    /* call constructor (if any) */
    if(!m->err_code && start_function != -1U && start_function < m->function_count) {
        if(m->functions[start_function].start_addr) wa_call(m, start_function);
        else wa_external_call(m, start_function);
    }
    return !m->err_code;
}

/**
 * Return the total size of all internal buffers
 * @param m module instance
 * @return the total size currently allocated
 */
size_t wa_sizeof(Module *m) {
    size_t sum;
    int i;
    for(i = 0, sum = 0; i < WA_NUMBUF; i++) sum += m->memory[i].size;
    return m->type_count * sizeof(Type) + (m->function_count + m->cache_count) * sizeof(Block) +
        (m->global_count + m->sp_count) * sizeof(StackValue) + m->global_count * sizeof(uint64_t) +
        m->csp_count * sizeof(Frame) + (m->table.size + m->br_count) * sizeof(uint32_t) +
        m->segs_count * sizeof(Segment) + sum;
}

/**
 * Free all internal buffers
 * @param m module instance
 * @return last error code
 */
int wa_free(Module *m) {
    int ret = m->err_code, i;
    DBG(("wa_free bytes %p byte_count %u sizeof %"LL"u limit %"LL"u err_code %d",
        (void*)m->bytes, m->byte_count, wa_sizeof(m), m->memory[0].size, ret));
    for(i = 0; i < WA_NUMBUF; i++)
        if(m->memory[i].bytes) free(m->memory[i].bytes);
    if(m->segs) free(m->segs);
    if(m->types) free(m->types);
    if(m->functions) free(m->functions);
    if(m->globals) free(m->globals);
    if(m->gptrs) free(m->gptrs);
    if(m->table.entries) free(m->table.entries);
    if(m->cache) free(m->cache);
    if(m->lookup) free(m->lookup);
    if(m->stack) free(m->stack);
    if(m->callstack) free(m->callstack);
    if(m->br_table) free(m->br_table);
    memset(m, 0, sizeof(Module));
    return ret;
}

#endif /* WA_IMPLEMENTATION */

#ifdef  __cplusplus
}
#endif

#endif /* _WA_H_ */
