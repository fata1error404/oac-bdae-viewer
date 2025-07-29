#ifndef _COMPAREANDSWAP_H_
#define _COMPAREANDSWAP_H_

//! Define types and methods for compare and swap implementations.
// Description:
// Atomic compare and swap operation.  Compare *value with oldVal, and if equal set *value to newVal.
// Return boolean indicating if swap took place.
// Arguments:
// value - Pointer to value to test and swap.
// oldVal - Value to compare with to see if we should swap.
// newVal - New value to swap with if comparison was successful.
// Return Value List:
// true - Value was equal to oldVal and newVal was set.
// false - Value was not equal to oldVal.

typedef unsigned int AtomicType; // Typedef for the data type used for atomic locks.
static inline bool CompareAndSwap(AtomicType *value, unsigned int oldVal, unsigned int newVal)
{
#ifdef __GNUC__
	return __sync_bool_compare_and_swap(value, oldVal, newVal);
#else
	bool ret;
	unsigned int dummy;
	/* %0 : Result set by setz, true for compare result true. */
	/* %1 : Address of lock variable. */
	/* %2 : Dummy result bound to EAX to clobber */
	/* %3 : Bound to %2, pass in old lock value via EAX */
	/* %4 : New lock value */
	__asm__ __volatile__(
		"lock ; cmpxchg %4,%1\n\t"
		"setz %0"
		: "=d"(ret),
		  "=m"(*(__volatile__ unsigned int *)value),
		  "=a"(dummy)
		: "2"(oldVal),
		  "r"(newVal));

	return ret;
#endif
}

#endif
