#ifndef _MUTEX_H_
#define _MUTEX_H_

#define CAutoLock _CAutoLock<Mutex>
#define CAutoRwLock _CAutoLock<RwLock>

#include <pthread.h>
#include "CompareAndSwap.h"

class Mutex
{
	friend class Condition;

public:
	int Lock();
	int Unlock();
	int TryLock();

	Mutex();
	virtual ~Mutex();

	void Acquire() { Lock(); }
	void Release() { Unlock(); }
	bool AttemptAcquire() { return TryLock() == 0; }

	pthread_mutex_t *Get() { return &_mutex; }

private:
	pthread_mutex_t _mutex;
};

class RwLock
{
private:
	AtomicType m_readLockCount; // Count of read locks.  0 - unlocked, positive - number of read locks, -1 - write locked.

public:
	RwLock();
	~RwLock();

	bool ReadLock();
	bool ReadUnlock();

	bool WriteLock();
	bool WriteUnlock();

	bool Lock() { return WriteLock(); }
	bool Unlock() { return WriteUnlock(); }
	bool TryLock() { return TryWriteLock(); }

	void Acquire() { Lock(); }
	void Release() { Unlock(); }
	bool AttemptAcquire() { return TryLock() == 0; }

private:
	bool TryWriteLock();
	void ClearLock() { m_readLockCount = 0; }
};

template <class LOCK>
class _CAutoLock
{
	LOCK *m_pLock;

public:
	_CAutoLock(LOCK *pLock)
	{
		m_pLock = pLock;
		m_pLock->Acquire();
	}

	~_CAutoLock()
	{
		m_pLock->Release();
	}
};

#endif
