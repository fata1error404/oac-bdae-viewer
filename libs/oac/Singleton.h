#ifndef _MMO_SINGLETON_H_
#define _MMO_SINGLETON_H_

#ifndef ASSERT
#include <assert.h>
#define ASSERT(x) assert(x)
#endif

template <typename T>
class Singleton
{
	static T *s_instance;

public:
	static T *newInstance()
	{
		return new T();
	}
	static void deleteInstance()
	{
		T *instance = s_instance;
		s_instance = 0;
		delete instance;
	}
	static T *instance()
	{
		return (s_instance);
	}

private:
	Singleton(const Singleton &source) {}

protected:
	Singleton()
	{
		ASSERT(!s_instance && "the singleton object repeat construct");
		s_instance = (T *)this;
	}
	virtual ~Singleton()
	{
		s_instance = 0;
	}
};

template <typename T>
T *Singleton<T>::s_instance = 0;

#endif
