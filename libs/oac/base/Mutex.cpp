
#include <errno.h>
#include <pthread.h>
#include "Mutex.h"
#include "assert.h"
#include "CompareAndSwap.h"
#include "OS.h"

Mutex::Mutex()
{
	int rc;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	if ((rc = pthread_mutex_init(&_mutex, &attr)) != 0)
		assert(false);

	pthread_mutexattr_destroy(&attr);
}

Mutex::~Mutex()
{
	int rc;
	if ((rc = pthread_mutex_destroy(&_mutex)) != 0)
		assert(false);
}

int Mutex::Lock()
{
	int rc;
	if ((rc = pthread_mutex_lock(&_mutex)) != 0)
	{
		assert(false);
		return -1;
	}

	return 0;
}

int Mutex::Unlock()
{
	int rc;
	if ((rc = pthread_mutex_unlock(&_mutex)) != 0)
	{
		assert(false);
		return -1;
	}

	return 0;
}

int Mutex::TryLock()
{
	int rc;
	if ((rc = pthread_mutex_trylock(&_mutex)) != 0)
	{
		if (rc == EBUSY)
		{
			return 1;
		}
		else
		{
			assert(false);
			return -1;
		}
	}

	return 0;
}

RwLock::RwLock()
{
	m_readLockCount = 0;
}

RwLock::~RwLock()
{
}

bool RwLock::ReadLock()
{
	while (1)
	{
		// Take a guess at what our compare and swap should be.
		int readLockCount = (int)(m_readLockCount & 0xFFFFFFFF);

		if (readLockCount < 0)
			usleep(1 * 1000);
		else
		{
			// No write locks - attempt to increase the readlock count.
			if (CompareAndSwap(&m_readLockCount, readLockCount, readLockCount + 1))
			{
				return true;
			}
		}
	}
}

bool RwLock::ReadUnlock()
{
	while (1)
	{
		// Take a guess at what our compare and swap should be.
		int readLockCount = (int)(m_readLockCount & 0xFFFFFFFF);

		assert(readLockCount > 0);
		if (readLockCount <= 0)
			return true;

		// No write locks - attempt to increase the readlock count.
		if (CompareAndSwap(&m_readLockCount, readLockCount, readLockCount - 1))
			return true;
	}
}

bool RwLock::WriteLock()
{
#ifdef _MMO_SERVER_
	while (!TryWriteLock())
		;
	return true;
#else
	while (1)
	{
		if (TryWriteLock())
			return true;
		usleep(1 * 1000);
	}
#endif
}

bool RwLock::TryWriteLock()
{
	int newVal = ~0U;

	// Try to change lock count from 0 to -1. (no locks to write lock).
	if (CompareAndSwap(&m_readLockCount, 0, newVal))
		return true;

	return false;
}

bool RwLock::WriteUnlock()
{
	while (1)
	{
		if (m_readLockCount == 0)
			return true;

		// We should be on -1 locks - we have the only lock.
		int oldVal = ~0U;

		// Try to change lock count from 0 to -1. (no locks to write lock).
		if (CompareAndSwap(&m_readLockCount, oldVal, 0))
			return true;
	}
}
