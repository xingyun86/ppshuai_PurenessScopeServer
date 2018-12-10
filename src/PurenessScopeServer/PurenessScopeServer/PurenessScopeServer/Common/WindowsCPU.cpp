#ifdef WIN32

#include "WindowsCPU.h"

double GetProcessCPU_Idle()
{
    CpuUsage cpuUsage;
	return cpuUsage.GetUsageDuration();
}

uint64 GetProcessMemorySize()
{
	PROCESS_MEMORY_COUNTERS pmc = { 0 };

    if(::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return (uint64)(pmc.WorkingSetSize + pmc.PagefileUsage);
    }

    return (0LL);
}

#endif
