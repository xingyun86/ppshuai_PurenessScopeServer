#ifdef WIN32

#include "WindowsCPU.h"

double CComputerUsage::GetProcessCPU_Idle()
{
	return cpuUsage.GetUsage();
}

uint64 CComputerUsage::GetProcessMemorySize()
{
	PROCESS_MEMORY_COUNTERS pmc = { 0 };

	if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
	{
		return (uint64)(pmc.WorkingSetSize + pmc.PagefileUsage);
	}

	return (0LL);
}

#endif
