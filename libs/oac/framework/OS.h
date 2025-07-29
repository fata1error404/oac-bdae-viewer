#ifndef _OS_H_
#define _OS_H_

#include <string>
#include <string.h>
#include <unistd.h>
#include "Singleton.h"
#include "IReadResFile.h"

#define sOS (*OS::instance())

class OS : public Singleton<OS>
{
	friend class Singleton<OS>;

public:
	virtual ~OS() {}

	void SleepS(unsigned long sec) { sleep(sec); };

	void SleepMS(unsigned long msec) { usleep(msec * 1000); };

	unsigned int GetTimeMS();

	std::string GetModulePath();

	IReadResFile *CreateReadResFile(const char *fileName);

private:
	OS() {}
};

inline int gettimeofday(struct timeval *tv, void * /*tz*/)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;

	return 0;
}

#endif
