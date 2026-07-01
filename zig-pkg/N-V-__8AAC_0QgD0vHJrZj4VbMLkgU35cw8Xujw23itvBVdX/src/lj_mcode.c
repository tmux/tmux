/*
** Machine code management.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_mcode_c
#define LUA_CORE

#include "lj_obj.h"
#if LJ_HASJIT
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_jit.h"
#include "lj_mcode.h"
#include "lj_trace.h"
#include "lj_dispatch.h"
#include "lj_prng.h"
#endif
#if LJ_HASJIT || LJ_HASFFI
#include "lj_vm.h"
#endif

/* -- OS-specific functions ----------------------------------------------- */

#if LJ_HASJIT || LJ_HASFFI

/* Define this if you want to run LuaJIT with Valgrind. */
#ifdef LUAJIT_USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#if LJ_TARGET_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if LJ_TARGET_IOS
void sys_icache_invalidate(void *start, size_t len);
#endif

/* Synchronize data/instruction cache. */
void lj_mcode_sync(void *start, void *end)
{
#ifdef LUAJIT_USE_VALGRIND
  VALGRIND_DISCARD_TRANSLATIONS(start, (char *)end-(char *)start);
#endif
#if LJ_TARGET_X86ORX64
  UNUSED(start); UNUSED(end);
#elif LJ_TARGET_WINDOWS
  FlushInstructionCache(GetCurrentProcess(), start, (char *)end-(char *)start);
#elif LJ_TARGET_IOS
  sys_icache_invalidate(start, (char *)end-(char *)start);
#elif LJ_TARGET_PPC
  lj_vm_cachesync(start, end);
#elif defined(__GNUC__) || defined(__clang__)
  __clear_cache(start, end);
#else
#error "Missing builtin to flush instruction cache"
#endif
}

#endif

#if LJ_HASJIT

#if LUAJIT_SECURITY_MCODE != 0
/* Protection twiddling failed. Probably due to kernel security. */
static LJ_NORET LJ_NOINLINE void mcode_protfail(jit_State *J)
{
  lua_CFunction panic = J2G(J)->panic;
  if (panic) {
    lua_State *L = J->L;
    setstrV(L, L->top++, lj_err_str(L, LJ_ERR_JITPROT));
    panic(L);
  }
  exit(EXIT_FAILURE);
}
#endif

#if LJ_TARGET_WINDOWS

#define MCPROT_RW	PAGE_READWRITE
#define MCPROT_RX	PAGE_EXECUTE_READ
#define MCPROT_RWX	PAGE_EXECUTE_READWRITE

static void *mcode_alloc_at(uintptr_t hint, size_t sz, DWORD prot)
{
  return LJ_WIN_VALLOC((void *)hint, sz,
		       MEM_RESERVE|MEM_COMMIT|MEM_TOP_DOWN, prot);
}

static void mcode_free(void *p, size_t sz)
{
  UNUSED(sz);
  VirtualFree(p, 0, MEM_RELEASE);
}

static void mcode_setprot(jit_State *J, void *p, size_t sz, DWORD prot)
{
#if LUAJIT_SECURITY_MCODE != 0
  DWORD oprot;
  if (!LJ_WIN_VPROTECT(p, sz, prot, &oprot)) mcode_protfail(J);
#else
  UNUSED(J); UNUSED(p); UNUSED(sz); UNUSED(prot);
#endif
}

#elif LJ_TARGET_POSIX

#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS	MAP_ANON
#endif

/* Check for macOS hardened runtime. */
#if defined(LUAJIT_ENABLE_OSX_HRT) && LUAJIT_SECURITY_MCODE != 0 && defined(MAP_JIT) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 110000
#include <pthread.h>
#define MCMAP_CREATE	MAP_JIT
#else
#define MCMAP_CREATE	0
#endif

#define MCPROT_RW	(PROT_READ|PROT_WRITE)
#define MCPROT_RX	(PROT_READ|PROT_EXEC)
#define MCPROT_RWX	(PROT_READ|PROT_WRITE|PROT_EXEC)
#ifdef PROT_MPROTECT
#define MCPROT_CREATE	(PROT_MPROTECT(MCPROT_RWX))
#elif MCMAP_CREATE
#define MCPROT_CREATE	PROT_EXEC
#else
#define MCPROT_CREATE	0
#endif

static void *mcode_alloc_at(uintptr_t hint, size_t sz, int prot)
{
  void *p = mmap((void *)hint, sz, prot|MCPROT_CREATE, MAP_PRIVATE|MAP_ANONYMOUS|MCMAP_CREATE, -1, 0);
  if (p == MAP_FAILED) return NULL;
#if MCMAP_CREATE
  pthread_jit_write_protect_np(0);
#endif
  return p;
}

static void mcode_free(void *p, size_t sz)
{
  munmap(p, sz);
}

static void mcode_setprot(jit_State *J, void *p, size_t sz, int prot)
{
#if LUAJIT_SECURITY_MCODE != 0
#if MCMAP_CREATE
  UNUSED(J); UNUSED(p); UNUSED(sz);
  pthread_jit_write_protect_np((prot & PROT_EXEC));
  return 0;
#else
  if (mprotect(p, sz, prot)) mcode_protfail(J);
#endif
#else
  UNUSED(J); UNUSED(p); UNUSED(sz); UNUSED(prot);
#endif
}

#else

#error "Missing OS support for explicit placement of executable memory"

#endif

#ifdef LUAJIT_MCODE_TEST
/* Test wrapper for mcode allocation. DO NOT ENABLE in production! Try:
**   LUAJIT_MCODE_TEST=hhhhhhhhhhhhhhhh luajit -jv main.lua
**   LUAJIT_MCODE_TEST=F luajit -jv main.lua
*/
static void *mcode_alloc_at_TEST(jit_State *J, uintptr_t hint, size_t sz, int prot)
{
  static int test_ofs = 0;
  static const char *test_str;
  if (!test_str) {
    test_str = getenv("LUAJIT_MCODE_TEST");
    if (!test_str) test_str = "";
  }
  switch (test_str[test_ofs]) {
  case 'a':  /* OK for one allocation. */
    test_ofs++;
    /* fallthrough */
  case '\0':  /* EOS: OK for any further allocations. */
    break;
  case 'h':  /* Ignore one hint. */
    test_ofs++;
    /* fallthrough */
  case 'H':  /* Ignore any further hints. */
    hint = 0u;
    break;
  case 'r':  /* Randomize one hint. */
    test_ofs++;
    /* fallthrough */
  case 'R':  /* Randomize any further hints. */
    hint = lj_prng_u64(&J2G(J)->prng) & ~(uintptr_t)0xffffu;
    hint &= ((uintptr_t)1 << (LJ_64 ? 47 : 31)) - 1;
    break;
  case 'f':  /* Fail one allocation. */
    test_ofs++;
    /* fallthrough */
  default:  /* 'F' or unknown: Fail any further allocations. */
    return NULL;
  }
  return mcode_alloc_at(hint, sz, prot);
}
#define mcode_alloc_at(hint, sz, prot)	mcode_alloc_at_TEST(J, hint, sz, prot)
#endif

/* -- MCode area protection ----------------------------------------------- */

#if LUAJIT_SECURITY_MCODE == 0

/* Define this ONLY if page protection twiddling becomes a bottleneck.
**
** It's generally considered to be a potential security risk to have
** pages with simultaneous write *and* execute access in a process.
**
** Do not even think about using this mode for server processes or
** apps handling untrusted external data.
**
** The security risk is not in LuaJIT itself -- but if an adversary finds
** any *other* flaw in your C application logic, then any RWX memory pages
** simplify writing an exploit considerably.
*/
#define MCPROT_GEN	MCPROT_RWX
#define MCPROT_RUN	MCPROT_RWX

static void mcode_protect(jit_State *J, int prot)
{
  UNUSED(J); UNUSED(prot);
}

#else

/* This is the default behaviour and much safer:
**
** Most of the time the memory pages holding machine code are executable,
** but NONE of them is writable.
**
** The current memory area is marked read-write (but NOT executable) only
** during the short time window while the assembler generates machine code.
*/
#define MCPROT_GEN	MCPROT_RW
#define MCPROT_RUN	MCPROT_RX

/* Change protection of MCode area. */
static void mcode_protect(jit_State *J, int prot)
{
  if (J->mcprot != prot) {
    mcode_setprot(J, J->mcarea, J->szmcarea, prot);
    J->mcprot = prot;
  }
}

#endif

/* -- MCode area allocation ----------------------------------------------- */

#ifdef LJ_TARGET_JUMPRANGE

#define MCODE_RANGE64	((1u << LJ_TARGET_JUMPRANGE) - 0x10000u)

/* Set a memory range for mcode allocation with addr in the middle. */
static void mcode_setrange(jit_State *J, uintptr_t addr)
{
#if LJ_TARGET_MIPS
  /* Use the whole 256MB-aligned region. */
  J->mcmin = addr & ~(uintptr_t)((1u << LJ_TARGET_JUMPRANGE) - 1);
  J->mcmax = J->mcmin + (1u << LJ_TARGET_JUMPRANGE);
#else
  /* Every address in the 64KB-aligned range should be able to reach
  ** any other, so MCODE_RANGE64 is only half the (signed) branch range.
  */
  J->mcmin = (addr - (MCODE_RANGE64 >> 1) + 0xffffu) & ~(uintptr_t)0xffffu;
  J->mcmax = J->mcmin + MCODE_RANGE64;
#endif
  /* Avoid wrap-around and the 64KB corners. */
  if (addr < J->mcmin || !J->mcmin) J->mcmin = 0x10000u;
  if (addr > J->mcmax) J->mcmax = ~(uintptr_t)0xffffu;
}

/* Check if an address is in range of the mcode allocation range. */
static LJ_AINLINE int mcode_inrange(jit_State *J, uintptr_t addr, size_t sz)
{
  /* Take care of unsigned wrap-around of addr + sz, too. */
  return addr >= J->mcmin && addr + sz >= J->mcmin && addr + sz <= J->mcmax;
}

/* Get memory within a specific jump range in 64 bit mode. */
static void *mcode_alloc(jit_State *J, size_t sz)
{
  uintptr_t hint;
  int i = 0, j;
  if (!J->mcmin)  /* Place initial range near the interpreter code. */
    mcode_setrange(J, (uintptr_t)(void *)lj_vm_exit_handler);
  else if (!J->mcmax)  /* Switch to a new range (already flushed). */
    goto newrange;
  /* First try a contiguous area below the last one (if in range). */
  hint = (uintptr_t)J->mcarea - sz;
  if (!mcode_inrange(J, hint, sz))  /* Also takes care of NULL J->mcarea. */
    goto probe;
  for (; i < 16; i++) {
    void *p = mcode_alloc_at(hint, sz, MCPROT_GEN);
    if (mcode_inrange(J, (uintptr_t)p, sz))
      return p;  /* Success. */
    else if (p)
      mcode_free(p, sz);  /* Free badly placed area. */
  probe:
    /* Next try probing 64KB-aligned pseudo-random addresses. */
    j = 0;
    do {
      hint = J->mcmin + (lj_prng_u64(&J2G(J)->prng) & MCODE_RANGE64);
      if (++j > 15) goto fail;
    } while (!mcode_inrange(J, hint, sz));
  }
fail:
  if (!J->mcarea) {  /* Switch to a new range now. */
    void *p;
  newrange:
    p = mcode_alloc_at(0, sz, MCPROT_GEN);
    if (p) {
      mcode_setrange(J, (uintptr_t)p + (sz >> 1));
      return p;  /* Success. */
    }
  } else {
    J->mcmax = 0;  /* Switch to a new range after the flush. */
  }
  lj_trace_err(J, LJ_TRERR_MCODEAL);  /* Give up. OS probably ignores hints? */
  return NULL;
}

#else

/* All memory addresses are reachable by relative jumps. */
static void *mcode_alloc(jit_State *J, size_t sz)
{
#if defined(__OpenBSD__) || defined(__NetBSD__) || LJ_TARGET_UWP
  /* Allow better executable memory allocation for OpenBSD W^X mode. */
  void *p = mcode_alloc_at(0, sz, MCPROT_RUN);
  if (p) mcode_setprot(J, p, sz, MCPROT_GEN);
#else
  void *p = mcode_alloc_at(0, sz, MCPROT_GEN);
#endif
  if (!p) lj_trace_err(J, LJ_TRERR_MCODEAL);
  return p;
}

#endif

/* -- MCode area management ----------------------------------------------- */

/* Allocate a new MCode area. */
static void mcode_allocarea(jit_State *J)
{
  MCode *oldarea = J->mcarea;
  size_t sz = (size_t)J->param[JIT_P_sizemcode] << 10;
  J->mcarea = (MCode *)mcode_alloc(J, sz);
  J->szmcarea = sz;
  J->mcprot = MCPROT_GEN;
  J->mctop = (MCode *)((char *)J->mcarea + J->szmcarea);
  J->mcbot = (MCode *)((char *)J->mcarea + sizeof(MCLink));
  ((MCLink *)J->mcarea)->next = oldarea;
  ((MCLink *)J->mcarea)->size = sz;
  J->szallmcarea += sz;
  J->mcbot = (MCode *)lj_err_register_mcode(J->mcarea, sz, (uint8_t *)J->mcbot);
}

/* Free all MCode areas. */
void lj_mcode_free(jit_State *J)
{
  MCode *mc = J->mcarea;
  J->mcarea = NULL;
  J->szallmcarea = 0;
  while (mc) {
    MCode *next = ((MCLink *)mc)->next;
    size_t sz = ((MCLink *)mc)->size;
    lj_err_deregister_mcode(mc, sz, (uint8_t *)mc + sizeof(MCLink));
    mcode_free(mc, sz);
    mc = next;
  }
}

/* -- MCode transactions -------------------------------------------------- */

/* Reserve the remainder of the current MCode area. */
MCode *lj_mcode_reserve(jit_State *J, MCode **lim)
{
  if (!J->mcarea)
    mcode_allocarea(J);
  else
    mcode_protect(J, MCPROT_GEN);
  *lim = J->mcbot;
  return J->mctop;
}

/* Commit the top part of the current MCode area. */
void lj_mcode_commit(jit_State *J, MCode *top)
{
  J->mctop = top;
  mcode_protect(J, MCPROT_RUN);
}

/* Abort the reservation. */
void lj_mcode_abort(jit_State *J)
{
  if (J->mcarea)
    mcode_protect(J, MCPROT_RUN);
}

/* Set/reset protection to allow patching of MCode areas. */
MCode *lj_mcode_patch(jit_State *J, MCode *ptr, int finish)
{
  if (finish) {
    if (J->mcarea == ptr)
      mcode_protect(J, MCPROT_RUN);
    else
      mcode_setprot(J, ptr, ((MCLink *)ptr)->size, MCPROT_RUN);
    return NULL;
  } else {
    uintptr_t base = (uintptr_t)J->mcarea, addr = (uintptr_t)ptr;
    /* Try current area first to use the protection cache. */
    if (addr >= base && addr < base + J->szmcarea) {
      mcode_protect(J, MCPROT_GEN);
      return (MCode *)base;
    }
    /* Otherwise search through the list of MCode areas. */
    for (;;) {
      base = (uintptr_t)(((MCLink *)base)->next);
      lj_assertJ(base != 0, "broken MCode area chain");
      if (addr >= base && addr < base + ((MCLink *)base)->size) {
	mcode_setprot(J, (MCode *)base, ((MCLink *)base)->size, MCPROT_GEN);
	return (MCode *)base;
      }
    }
  }
}

/* Limit of MCode reservation reached. */
void lj_mcode_limiterr(jit_State *J, size_t need)
{
  size_t sizemcode, maxmcode;
  lj_mcode_abort(J);
  sizemcode = (size_t)J->param[JIT_P_sizemcode] << 10;
  maxmcode = (size_t)J->param[JIT_P_maxmcode] << 10;
  if (need * sizeof(MCode) > sizemcode)
    lj_trace_err(J, LJ_TRERR_MCODEOV);  /* Too long for any area. */
  if (J->szallmcarea + sizemcode > maxmcode)
    lj_trace_err(J, LJ_TRERR_MCODEAL);
  mcode_allocarea(J);
  lj_trace_err(J, LJ_TRERR_MCODELM);  /* Retry with new area. */
}

#endif
