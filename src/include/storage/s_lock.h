/*-------------------------------------------------------------------------
 *
 * s_lock.h
 *	   This file contains the in-line portion of the implementation
 *	   of spinlocks.
 *
 * Portions Copyright (c) 1996-2000, PostgreSQL, Inc
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header$
 *
 *-------------------------------------------------------------------------
 */

/*----------
 * DESCRIPTION
 *	The public macros that must be provided are:
 *
 *	void S_INIT_LOCK(slock_t *lock)
 *		Initialize a spinlock (to the unlocked state).
 *
 *	void S_LOCK(slock_t *lock)
 *		Acquire a spinlock, waiting if necessary.
 *		Time out and abort() if unable to acquire the lock in a
 *		"reasonable" amount of time --- typically ~ 1 minute.
 *
 *	void S_UNLOCK(slock_t *lock)
 *		Unlock a previously acquired lock.
 *
 *	bool S_LOCK_FREE(slock_t *lock)
 *		Tests if the lock is free. Returns TRUE if free, FALSE if locked.
 *		This does *not* change the state of the lock.
 *
 *	int TAS(slock_t *lock)
 *		Atomic test-and-set instruction.  Attempt to acquire the lock,
 *		but do *not* wait.  Returns 0 if successful, nonzero if unable
 *		to acquire the lock.
 *
 *	TAS() is a lower-level part of the API, but is used directly in a
 *	few places that want to do other things while waiting for a lock.
 *	The S_LOCK() macro is equivalent to
 *
 *	void
 *	S_LOCK(slock_t *lock)
 *	{
 *		unsigned	spins = 0;
 *
 *		while (TAS(lock))
 *		{
 *			S_LOCK_SLEEP(lock, spins++);
 *		}
 *	}
 *
 *	where S_LOCK_SLEEP() checks for timeout and sleeps for a short
 *	interval.  Callers that want to perform useful work while waiting
 *	can write out this entire loop and insert the "useful work" inside
 *	the loop.
 *
 *	CAUTION to TAS() callers: on some platforms TAS() may sometimes
 *	report failure to acquire a lock even when the lock is not locked.
 *	For example, on Alpha TAS() will "fail" if interrupted.  Therefore
 *	TAS() must *always* be invoked in a retry loop as depicted, even when
 *	you are certain the lock is free.
 *
 *	On most supported platforms, TAS() uses a tas() function written
 *	in assembly language to execute a hardware atomic-test-and-set
 *	instruction.  Equivalent OS-supplied mutex routines could be used too.
 *
 *	If no system-specific TAS() is available (ie, HAS_TEST_AND_SET is not
 *	defined), then we fall back on an emulation that uses SysV semaphores.
 *	This emulation will be MUCH MUCH MUCH slower than a proper TAS()
 *	implementation, because of the cost of a kernel call per lock or unlock.
 *	An old report is that Postgres spends around 40% of its time in semop(2)
 *	when using the SysV semaphore code.
 *
 *	Note to implementors: there are default implementations for all these
 *	macros at the bottom of the file.  Check if your platform can use
 *	these or needs to override them.
 *----------
 */
#ifndef S_LOCK_H
#define S_LOCK_H

#include "storage/ipc.h"

/* Platform-independent out-of-line support routines */
extern void s_lock(volatile slock_t *lock,
				   const char *file, const int line);
extern void s_lock_sleep(unsigned spins, int microsec,
						 volatile slock_t *lock,
						 const char *file, const int line);


#if defined(HAS_TEST_AND_SET)


#if defined(__GNUC__)
/*************************************************************************
 * All the gcc inlines
 */


#if defined(__i386__)
#define TAS(lock) tas(lock)

static __inline__ int
tas(volatile slock_t *lock)
{
	register slock_t _res = 1;

__asm__("lock; xchgb %0,%1": "=q"(_res), "=m"(*lock):"0"(_res));
	return (int) _res;
}

#endif	 /* __i386__ */


#ifdef __ia64__
#define TAS(lock) tas(lock)

static __inline__ int
tas (volatile slock_t *lock)
{
  long int ret;

  __asm__ __volatile__(
       "xchg4 %0=%1,%2"
       : "=r"(ret), "=m"(*lock)
       : "r"(1), "1"(*lock)
       : "memory");

  return (int) ret;
}
#endif /* __ia64__ */


#if defined(__arm__) || defined(__arm__)
#define TAS(lock) tas(lock)

static __inline__ int
tas(volatile slock_t *lock)
{
        register slock_t _res = 1;

__asm__("swpb %0, %0, [%3]": "=r"(_res), "=m"(*lock):"0"(_res), "r" (lock));
        return (int) _res;
}

#endif   /* __arm__ */

#if defined(__s390__)
/*
 * S/390 Linux
 */
#define TAS(lock)      tas(lock)

static inline int
tas(volatile slock_t *lock)
{
 int _res;

        __asm__ __volatile("    la    1,1\n"
                           "    l     2,%2\n"
                           "    slr   0,0\n"
                           "    cs    0,1,0(2)\n"
                           "    lr    %1,0"
                           : "=m" (lock), "=d" (_res)
                           : "m" (lock)
                           : "0", "1", "2");

       return (_res);
}
#endif  /* __s390__ */


#if defined(__sparc__)
#define TAS(lock) tas(lock)

static __inline__ int
tas(volatile slock_t *lock)
{
	register slock_t _res = 1;

	__asm__("ldstub [%2], %0" \
:			"=r"(_res), "=m"(*lock) \
:			"r"(lock));
	return (int) _res;
}

#endif	 /* __sparc__ */


#if defined(__mc68000__) && defined(__linux__)
#define TAS(lock) tas(lock)

static __inline__ int
tas(volatile slock_t *lock)
{
	register int rv;
	
	__asm__ __volatile__ (
		"tas %1; sne %0"
		: "=d" (rv), "=m"(*lock)
		: "1" (*lock)
		: "cc" );
	return rv;
}

#endif /* defined(__mc68000__) && defined(__linux__) */


#if defined(NEED_VAX_TAS_ASM)
/*
 * VAXen -- even multiprocessor ones
 * (thanks to Tom Ivar Helbekkmo)
 */
#define TAS(lock) tas(lock)

typedef unsigned char slock_t;

static __inline__ int
tas(volatile slock_t *lock)
{
	register	_res;

	__asm__("	movl $1, r0 \
			bbssi $0, (%1), 1 f \
			clrl r0 \
1:			movl r0, %0 "
:			"=r"(_res)			/* return value, in register */
:			"r"(lock)			/* argument, 'lock pointer', in register */
:			"r0");				/* inline code uses this register */
	return (int) _res;
}

#endif	 /* NEED_VAX_TAS_ASM */


#if defined(NEED_NS32K_TAS_ASM)
#define TAS(lock) tas(lock)

static __inline__ int
tas(volatile slock_t *lock)
{
  register _res;
  __asm__("sbitb 0, %0 \n\
	sfsd %1"
	: "=m"(*lock), "=r"(_res));
  return (int) _res; 
}

#endif  /* NEED_NS32K_TAS_ASM */



#else							/* !__GNUC__ */

/***************************************************************************
 * All non-gcc inlines
 */

#if defined(NEED_I386_TAS_ASM) && defined(USE_UNIVEL_CC)
#define TAS(lock)	tas(lock)

asm int
tas(volatile slock_t *s_lock)
{
/* UNIVEL wants %mem in column 1, so we don't pg_indent this file */
%mem s_lock
	pushl %ebx
	movl s_lock, %ebx
	movl $255, %eax
	lock
	xchgb %al, (%ebx)
	popl %ebx
}

#endif /* defined(NEED_I386_TAS_ASM) && defined(USE_UNIVEL_CC) */

#endif	 /* defined(__GNUC__) */



/*************************************************************************
 * These are the platforms that do not use inline assembler (and hence
 * have common code for gcc and non-gcc compilers, if both are available).
 */


#if defined(__alpha)

/*
 * Correct multi-processor locking methods are explained in section 5.5.3
 * of the Alpha AXP Architecture Handbook, which at this writing can be
 * found at ftp://ftp.netbsd.org/pub/NetBSD/misc/dec-docs/index.html.
 * For gcc we implement the handbook's code directly with inline assembler.
 */
#if defined(__GNUC__)

#define TAS(lock)  tas(lock)
#define S_UNLOCK(lock)  do { __asm__ volatile ("mb"); *(lock) = 0; } while (0)

static __inline__ int
tas(volatile slock_t *lock)
{
	register slock_t _res;

	__asm__ volatile
("		ldq   $0, %0		\n\
		bne   $0, 2f		\n\
		ldq_l %1, %0		\n\
		bne   %1, 2f		\n\
		mov   1, $0			\n\
		stq_c $0, %0		\n\
		beq   $0, 2f		\n\
		mb					\n\
		br    3f			\n\
	 2: mov   1, %1			\n\
	 3:       \n" : "=m"(*lock), "=r"(_res) : : "0");

	return (int) _res;
}

#else /* !defined(__GNUC__) */

/*
 * The Tru64 compiler doesn't support gcc-style inline asm, but it does
 * have some builtin functions that accomplish much the same results.
 * For simplicity, slock_t is defined as long (ie, quadword) on Alpha
 * regardless of the compiler in use.  LOCK_LONG and UNLOCK_LONG only
 * operate on an int (ie, longword), but that's OK as long as we define
 * S_INIT_LOCK to zero out the whole quadword.
 */

#include <alpha/builtins.h>

#define S_INIT_LOCK(lock)  (*(lock) = 0)
#define TAS(lock)          (__LOCK_LONG_RETRY((lock), 1) == 0)
#define S_UNLOCK(lock)     __UNLOCK_LONG(lock)

#endif /* defined(__GNUC__) */

#endif /* __alpha */


#if defined(__hpux)
/*
 * HP-UX (PA-RISC)
 *
 * Note that slock_t on PA-RISC is a structure instead of char
 * (see include/port/hpux.h).
 *
 * a "set" slock_t has a single word cleared.  a "clear" slock_t has
 * all words set to non-zero. tas() is in tas.s
 */

#define S_UNLOCK(lock) \
do { \
	volatile slock_t *lock_ = (volatile slock_t *) (lock); \
	lock_->sema[0] = lock_->sema[1] = lock_->sema[2] = lock_->sema[3] = -1; \
} while (0)

#define S_LOCK_FREE(lock)	( *(int *) (((long) (lock) + 15) & ~15) != 0)

#endif	 /* __hpux */


#if defined(__QNX__)
/*
 * QNX 4
 *
 * Note that slock_t under QNX is sem_t instead of char
 */
#define TAS(lock)       (sem_trywait((lock)) < 0)
#define S_UNLOCK(lock)  sem_post((lock))
#define S_INIT_LOCK(lock)       sem_init((lock), 1, 1)
#define S_LOCK_FREE(lock)       ((lock)->value)
#endif   /* __QNX__ */


#if defined(__sgi)
/*
 * SGI IRIX 5
 * slock_t is defined as a unsigned long. We use the standard SGI
 * mutex API. 
 *
 * The following comment is left for historical reasons, but is probably
 * not a good idea since the mutex ABI is supported.
 *
 * This stuff may be supplemented in the future with Masato Kataoka's MIPS-II
 * assembly from his NECEWS SVR4 port, but we probably ought to retain this
 * for the R3000 chips out there.
 */
#include "mutex.h"
#define TAS(lock)	(test_and_set(lock,1))
#define S_UNLOCK(lock)	(test_then_and(lock,0))
#define S_INIT_LOCK(lock)	(test_then_and(lock,0))
#define S_LOCK_FREE(lock)	(test_then_add(lock,0) == 0)
#endif	 /* __sgi */

#if defined(sinix)
/*
 * SINIX / Reliant UNIX 
 * slock_t is defined as a struct abilock_t, which has a single unsigned long
 * member. (Basically same as SGI)
 *
 */
#define TAS(lock)	(!acquire_lock(lock))
#define S_UNLOCK(lock)	release_lock(lock)
#define S_INIT_LOCK(lock)	init_lock(lock)
#define S_LOCK_FREE(lock)	(stat_lock(lock) == UNLOCKED)
#endif	 /* sinix */
 

#if defined(_AIX)
/*
 * AIX (POWER)
 *
 * Note that slock_t on POWER/POWER2/PowerPC is int instead of char
 * (see storage/ipc.h).
 */
#define TAS(lock)	cs((int *) (lock), 0, 1)
#endif	 /* _AIX */


#if defined (nextstep)
/*
 * NEXTSTEP (mach)
 * slock_t is defined as a struct mutex.
 */

#define S_LOCK(lock)	mutex_lock(lock)
#define S_UNLOCK(lock)	mutex_unlock(lock)
#define S_INIT_LOCK(lock)	mutex_init(lock)
/* For Mach, we have to delve inside the entrails of `struct mutex'.  Ick! */
#define S_LOCK_FREE(alock)	((alock)->lock == 0)
#endif	 /* nextstep */



#else	 /* !HAS_TEST_AND_SET */

/*
 * Fake spinlock implementation using SysV semaphores --- slow and prone
 * to fall foul of kernel limits on number of semaphores, so don't use this
 * unless you must!
 */

typedef struct
{
	/* reference to semaphore used to implement this spinlock */
	IpcSemaphoreId	semId;
	int				sem;
} slock_t;

extern bool s_lock_free_sema(volatile slock_t *lock);
extern void s_unlock_sema(volatile slock_t *lock);
extern void s_init_lock_sema(volatile slock_t *lock);
extern int tas_sema(volatile slock_t *lock);

#define S_LOCK_FREE(lock)   s_lock_free_sema(lock)
#define S_UNLOCK(lock)   s_unlock_sema(lock)
#define S_INIT_LOCK(lock)   s_init_lock_sema(lock)
#define TAS(lock)   tas_sema(lock)

#endif	 /* HAS_TEST_AND_SET */



/****************************************************************************
 * Default Definitions - override these above as needed.
 */

#if !defined(S_LOCK)
#define S_LOCK(lock) \
	do { \
		if (TAS(lock)) \
			s_lock((lock), __FILE__, __LINE__); \
	} while (0)
#endif	 /* S_LOCK */

#if !defined(S_LOCK_SLEEP)
#define S_LOCK_SLEEP(lock,spins) \
	s_lock_sleep((spins), 0, (lock), __FILE__, __LINE__)
#endif	 /* S_LOCK_SLEEP */

#if !defined(S_LOCK_SLEEP_INTERVAL)
#define S_LOCK_SLEEP_INTERVAL(lock,spins,microsec) \
	s_lock_sleep((spins), (microsec), (lock), __FILE__, __LINE__)
#endif	 /* S_LOCK_SLEEP_INTERVAL */

#if !defined(S_LOCK_FREE)
#define S_LOCK_FREE(lock)	(*(lock) == 0)
#endif	 /* S_LOCK_FREE */

#if !defined(S_UNLOCK)
#define S_UNLOCK(lock)		(*(lock) = 0)
#endif	 /* S_UNLOCK */

#if !defined(S_INIT_LOCK)
#define S_INIT_LOCK(lock)	S_UNLOCK(lock)
#endif	 /* S_INIT_LOCK */

#if !defined(TAS)
extern int	tas(volatile slock_t *lock);		/* in port/.../tas.s, or
												 * s_lock.c */

#define TAS(lock)		tas(lock)
#endif	 /* TAS */


#endif	 /* S_LOCK_H */
