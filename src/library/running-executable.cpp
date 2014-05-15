#include "running-executable.hpp"
#include "directory.hpp"
#include <stdexcept>

namespace
{
#if __unix__
#include <unistd.h>
	std::string readlink_generic(std::string path)
	{
		char buf[8192] = {0};
		if(readlink(path.c_str(), buf, sizeof(buf)) > 0)
			return buf;
		throw std::runtime_error("Can't determine executable path");
	}
#endif

#if __linux__
#include <unistd.h>
std::string _running_executable()
{
	return readlink_generic("/proc/self/exe");
}
#elif __APPLE__
#include <mach-o/dyld.h>
std::string _running_executable()
{
	char buf[8192];
	uint32_t size = 8192;
	_NSGetExecutablePath(buf, &size);
	return buf;
}
#elif __WIN32__ || __WIN64__
#include "windows.h"
std::string _running_executable()
{
	char buf[8192];
	GetModuleFileName(NULL, buf, 8191);
	return buf;
}
#elif __OpenBSD__
std::string _running_executable()
{
	//TODO: This isn't doable?
	throw std::runtime_error("running_executable() unsupported");
}
#elif __FreeBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
std::string _running_executable()
{
	char buf[8192];
	int args[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	size_t len = 8192;
	if(sysctl(args, 4, buf, &len, NULL, 0) < 0)
		throw std::runtime_error("Can't determine executable path");
	return buf;
}
#elif __NetBSD__
std::string _running_executable()
{
	return readlink_generic("/proc/curproc/exe");
}
#elif __sun__
#include <stdlib.h>
std::string _running_executable()
{
	return getexecname();
}
#else
//Unsupported
std::string _running_executable()
{
	throw std::runtime_error("running_executable() unsupported");
}
#endif
}

std::string running_executable()
{
	return directory::absolute_path(_running_executable());
}
