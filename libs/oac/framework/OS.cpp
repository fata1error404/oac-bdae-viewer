#include <string>
#include <string.h>
#include <unistd.h>

#include "OS.h"
#include "IReadResFile.h"
#include "Base.h"

unsigned int OS::GetTimeMS()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

std::string OS::GetModulePath()
{
	static std::string modulePath;

	if (modulePath.empty())
	{
		char tmpBuf[1024] = {0};
		ssize_t len = readlink("/proc/self/exe", tmpBuf, sizeof(tmpBuf) - 1);
		if (len != -1)
		{
			tmpBuf[len] = '\0';
			*(strrchr(tmpBuf, '/') + 1) = '\0';
			modulePath = tmpBuf;
		}
	}

	return modulePath;
}

IReadResFile *OS::CreateReadResFile(const char *fileName)
{
	IReadResFile *resFile = createReadFile(fileName);

	if (resFile)
		return resFile;

	std::string fn = GetModulePath() + fileName;
	resFile = createReadFile(fn.c_str());

	if (!resFile)
		WLOG("Can't Open File [%s]!\n", fn.c_str());

	return resFile;
}
