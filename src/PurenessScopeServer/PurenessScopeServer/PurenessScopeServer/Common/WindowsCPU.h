#ifndef _WINDOWSCPU_H
#define _WINDOWSCPU_H

#include "define.h"

#if defined(_WIN32) || defined(WIN32)
#include <time.h>
#include <windows.h> //I've ommited this line.
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

__inline static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME f = { 0 };
	static char _tz = 0;
	unsigned long long u = 0;

	if (tv)
	{
#ifdef _WIN32_WCE 
		SYSTEMTIME s = { 0 };
		::GetSystemTime(&s);
		::SystemTimeToFileTime(&s, &f);
#else 
		::GetSystemTimeAsFileTime(&f);
#endif 

		u |= f.dwHighDateTime;
		u <<= 32;
		u |= f.dwLowDateTime;

		/*converting file time to unix epoch*/
		u -= DELTA_EPOCH_IN_MICROSECS;
		u /= 10;  /*convert into microseconds*/
		tv->tv_sec = (long)(u / 1000000UL);
		tv->tv_usec = (long)(u % 1000000UL);
	}

	if (tz)
	{
		if (!_tz)
		{
			_tzset();
			_tz = 0x01;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

__inline static unsigned long long gettimeofday_msec()
{
	struct timeval tv = { 0 };
	gettimeofday(&tv, 0);
	return (((unsigned long long)tv.tv_sec)*(unsigned long long)1000) + (((unsigned long long)tv.tv_usec) / (unsigned long long)1000);
}

#endif

#include  <windows.h>
#include  <Psapi.h>
#include  <conio.h>
#include  <stdio.h>
//在windows下获得CPU使用率的方法
class CpuUsage
{
public:
	CpuUsage(DWORD dwProcessID = 0L, DWORD dwMinimumElapsedMs = WAIT_TIMEOUT) :
		m_lRunCount(0),
		m_dwLastRun(0LL),
		m_dCpuUsage(0.0f),
		m_dwProcessID(dwProcessID),
		m_ullPrevSysNonIdleTime(0),
		m_ullPrevProcNonIdleTime(0),
		m_dwMinimumElapsedMs(dwMinimumElapsedMs)
	{
		SYSTEM_INFO si = { 0 };
		if (m_dwProcessID > 0L)
		{
			ElevationPrivilege(::GetCurrentProcess(), SE_DEBUG_NAME, TRUE);
		}
		m_hProcess = (m_dwProcessID > 0L) ? ::OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, m_dwProcessID) : ::GetCurrentProcess();
		if (!m_hProcess)
		{
			ErrMsg(TEXT("OpenProcess"));
		}
		ZeroMemory(&m_ftPrevSysKernel, sizeof(FILETIME));
		ZeroMemory(&m_ftPrevSysUser, sizeof(FILETIME));
		ZeroMemory(&m_ftPrevProcKernel, sizeof(FILETIME));
		ZeroMemory(&m_ftPrevProcUser, sizeof(FILETIME));

		GetSystemInfo(&si);
		m_dwNumberOfProcessors = si.dwNumberOfProcessors;
	}
	CpuUsage::~CpuUsage()
	{
		if (m_dwProcessID)
		{
			::CloseHandle(m_hProcess);
		}
	}
	bool EnoughTimePassed()
	{
		//milliseconds
		return (gettimeofday_msec() - m_dwLastRun) > m_dwMinimumElapsedMs;
	}
#ifdef USE_DEPRECATED_FUNCS
	typedef enum _SYSTEM_INFORMATION_CLASS {
		SystemBasicInformation = 0,
		SystemPerformanceInformation = 2,
		SystemTimeInformation = 3,
		ProcessTimes = 4,
		SystemProcessorPerformanceInformation = 8,
	};
#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))
	typedef struct
	{
		DWORD dwUnknown1;
		ULONG uKeMaximumIncrement;
		ULONG uPageSize;
		ULONG uMmNumberOfPhysicalPages;
		ULONG uMmLowestPhysicalPage;
		ULONG uMmHighestPhysicalPage;
		ULONG uAllocationGranularity;
		PVOID pLowestUserAddress;
		PVOID pMmHighestUserAddress;
		ULONG uKeActiveProcessors;
		BYTE bKeNumberProcessors;
		BYTE bUnknown2;
		WORD wUnknown3;
	} SYSTEM_BASIC_INFORMATION;
	typedef struct
	{
		LARGE_INTEGER liIdleTime;
		DWORD dwSpare[312];
	} SYSTEM_PERFORMANCE_INFORMATION;
	typedef struct
	{
		LARGE_INTEGER liKeBootTime;
		LARGE_INTEGER liKeSystemTime;
		LARGE_INTEGER liExpTimeZoneBias;
		ULONG uCurrentTimeZoneId;
		DWORD dwReserved;
	} SYSTEMTEXTIME_INFORMATION;
	typedef struct
		_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
	{
		LARGE_INTEGER IdleTime;
		LARGE_INTEGER KernelTime;
		LARGE_INTEGER UserTime;
		LARGE_INTEGER Reserved1[2];
		ULONG Reserved2;
	} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;
	typedef struct _KERNEL_USERTEXTIMES
	{
		LARGE_INTEGER CreateTime;
		LARGE_INTEGER ExitTime;
		LARGE_INTEGER KernelTime;
		LARGE_INTEGER UserTime;
	} KERNEL_USERTEXTIMES, *PKERNEL_USERTEXTIMES;
	typedef LONG(WINAPI *PROCNTQSI)(UINT, PVOID, ULONG, PULONG);
	PROCNTQSI FnNtQuerySystemInformation;
	typedef LONG(WINAPI *PROCNTQIP)(HANDLE, UINT, PVOID, ULONG, PULONG);
	PROCNTQIP FnNtQueryInformationProcess;
	ULONGLONG GetSystemNonIdleTimes()
	{
		SYSTEM_PERFORMANCE_INFORMATION SysPerfInfo;
		SYSTEMTEXTIME_INFORMATION SysTimeInfo;
		SYSTEM_BASIC_INFORMATION SysBaseInfo;
		SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION SysProcPerfInfo[32];
		LONG status;
		FnNtQuerySystemInformation = (PROCNTQSI)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "NtQuerySystemInformation");
		if (!FnNtQuerySystemInformation)
		{
			return (0LL);
		}
		status = FnNtQuerySystemInformation(SystemBasicInformation, &SysBaseInfo, sizeof(SysBaseInfo), NULL);
		if (status != NO_ERROR)
		{
			ErrMsg(TEXT("FnNtQuerySystemInformation"));
			return (0LL);
		}
		status = FnNtQuerySystemInformation(SystemProcessorPerformanceInformation, SysProcPerfInfo, sizeof(SysProcPerfInfo), NULL);
		if (status != NO_ERROR)
		{
			return (0LL);
		}
		int nProcessors = SysBaseInfo.bKeNumberProcessors; //机器内部CPU的个数
		ULONGLONG ullSysTotal = 0LL;
		for (int i = 0; i < nProcessors; i++)
		{
			ullSysTotal += SysProcPerfInfo[i].KernelTime.QuadPart + SysProcPerfInfo[i].UserTime.QuadPart;
		}
		return ullSysTotal;
	}
	ULONGLONG GetProcessNonIdleTimes()
	{
		KERNEL_USERTEXTIMES KernelUserTimes;
		::ZeroMemory(&KernelUserTimes, sizeof(KernelUserTimes));
		FnNtQueryInformationProcess = (PROCNTQIP)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "NtQueryInformationProcess");
		LONG status = FnNtQueryInformationProcess(m_hProcess, ProcessTimes, &KernelUserTimes, sizeof(KernelUserTimes), NULL);
		if (status == 0)
		{
			ErrMsg(TEXT("FnNtQueryInformationProcess"));
			return (0LL);
		}
		return (KernelUserTimes.KernelTime.QuadPart + KernelUserTimes.UserTime.QuadPart);
	}
#else
	ULONGLONG GetSystemNonIdleTimes()
	{
		FILETIME ftSysIdle, ftSysKernel, ftSysUser;
		if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser))
		{
			ErrMsg(TEXT("GetSystemTimes"));
			return (0LL);
		}
		return AddTimes(ftSysKernel, ftSysUser);
	}
	ULONGLONG GetProcessNonIdleTimes()
	{
		FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
		if (!GetProcessTimes(m_hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser) && false)
		{
			ErrMsg(TEXT("GetProcessNonIdleTimes"));
			return (0LL);
		}
		return AddTimes(ftProcKernel, ftProcUser);
	}
#endif
	double GetUsage()
	{
		double dCpuCopy = m_dCpuUsage;
		if (::InterlockedIncrement(&m_lRunCount) == 1)
		{
			if (!EnoughTimePassed())
			{
				::InterlockedDecrement(&m_lRunCount);
				return dCpuCopy;
			}
			FILETIME ftSysIdle = { 0 }, ftSysKernel = { 0 }, ftSysUser = { 0 },
				ftProcCreation = { 0 }, ftProcExit = { 0 }, ftProcKernel = { 0 }, ftProcUser = { 0 };
			GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser);
			DWORD dwErr = GetLastError();
			GetProcessTimes(m_hProcess, &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser);
			dwErr = GetLastError();
			if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser) ||
				!GetProcessTimes(m_hProcess, &ftProcCreation,
					&ftProcExit, &ftProcKernel, &ftProcUser))
			{
				::InterlockedDecrement(&m_lRunCount);
				return dCpuCopy;
			}
			if (IsFirstRun())
			{
				m_ftPrevSysKernel = ftSysKernel;
				m_ftPrevSysUser = ftSysUser;
				m_ftPrevProcKernel = ftProcKernel;
				m_ftPrevProcUser = ftProcUser;
				m_dwLastRun = gettimeofday_msec();
			}
			else
			{
				/*
				CPU usage is calculated by getting the total amount of time
				the system has operated since the last measurement
				(made up of kernel + user) and the total
				amount of time the process has run (kernel + user).
				*/
				ULONGLONG ftSysKernelDiff = SubtractTimes(ftSysKernel, m_ftPrevSysKernel);
				ULONGLONG ftSysUserDiff = SubtractTimes(ftSysUser, m_ftPrevSysUser);
				ULONGLONG ftProcKernelDiff = SubtractTimes(ftProcKernel, m_ftPrevProcKernel);
				ULONGLONG ftProcUserDiff = SubtractTimes(ftProcUser, m_ftPrevProcUser);

				ULONGLONG nTotalSys = ftSysKernelDiff + ftSysUserDiff;
				ULONGLONG nTotalProc = ftProcKernelDiff + ftProcUserDiff;
				if (nTotalProc != 0)
				{
					m_dCpuUsage = (double)(((double)(nTotalProc) / ((gettimeofday_msec() - m_dwLastRun) * 100.0)) / m_dwNumberOfProcessors);
					m_dwLastRun = gettimeofday_msec();
					m_ftPrevSysKernel = ftSysKernel;
					m_ftPrevSysUser = ftSysUser;
					m_ftPrevProcKernel = ftProcKernel;
					m_ftPrevProcUser = ftProcUser;
				}
			}
			dCpuCopy = m_dCpuUsage;
		}
		::InterlockedDecrement(&m_lRunCount);
		return dCpuCopy;
	}

private:
	__inline ULONGLONG SubtractTimes(const FILETIME& ftA, const FILETIME& ftB)
	{
		LARGE_INTEGER a, b;
		a.LowPart = ftA.dwLowDateTime;
		a.HighPart = ftA.dwHighDateTime;
		b.LowPart = ftB.dwLowDateTime;
		b.HighPart = ftB.dwHighDateTime;
		return a.QuadPart - b.QuadPart;
	}
	__inline ULONGLONG AddTimes(const FILETIME &ftA, const FILETIME &ftB)
	{
		LARGE_INTEGER a, b;
		a.LowPart = ftA.dwLowDateTime;
		a.HighPart = ftA.dwHighDateTime;
		b.LowPart = ftB.dwLowDateTime;
		b.HighPart = ftB.dwHighDateTime;
		return a.QuadPart + b.QuadPart;
	}
	__inline BOOL ElevationPrivilege(HANDLE hProcess, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
	{
		HANDLE hToken;
		if (!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
		{
			ErrMsg(TEXT("OpenProcessToken"));
			return FALSE;
		}
		LUID luid;
		if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
		{
			ErrMsg(TEXT("LookupPrivilegeValue"));
			return FALSE;
		}
		TOKEN_PRIVILEGES tkp;
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Luid = luid;
		tkp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : FALSE;
		if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
		{
			ErrMsg(TEXT("AdjustTokenPrivileges"));
			return FALSE;
		}
		return TRUE;
	}
	__inline void ErrMsg(LPTSTR lpszFunction)
	{
		// Retrieve the system error message for the last-error code
		LPVOID lpMsgBuf = NULL;
		LPVOID lpDisplayBuf = NULL;
		DWORD dw = GetLastError();
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			LANG_USER_DEFAULT,
			(LPTSTR)&lpMsgBuf,
			0, NULL);
		// Display the error message
		lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + MAXCHAR) * sizeof(TCHAR));
		if (lpDisplayBuf)
		{
			wsprintf((LPTSTR)lpDisplayBuf, TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf);
			//MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
			OutputDebugString((LPTSTR)lpDisplayBuf);
			LocalFree(lpDisplayBuf);
			lpDisplayBuf = NULL;
		}
		LocalFree(lpMsgBuf);
		lpMsgBuf = NULL;
	}
	__inline bool IsFirstRun() const { return (m_dwLastRun == 0); }

	//system total times
	FILETIME m_ftPrevSysKernel;
	FILETIME m_ftPrevSysUser;
	//process times
	FILETIME m_ftPrevProcKernel;
	FILETIME m_ftPrevProcUser;
	//minimum timeout time
	DWORD m_dwMinimumElapsedMs;

	//这个变量和后面的便利记录上次获取的非idle的系统cpu时间和进程cpu时间.
	ULONGLONG m_ullPrevSysNonIdleTime;
	//这个类只绑定一个进程, 在构造函数里面初始化进来.
	ULONGLONG m_ullPrevProcNonIdleTime;
	//Cpu usage
	double m_dCpuUsage;
	//Last run tick
	ULONGLONG m_dwLastRun;

	//Process id
	DWORD m_dwProcessID;
	//Process handle
	HANDLE m_hProcess;

	//Processor number
	DWORD m_dwNumberOfProcessors;

	volatile LONG m_lRunCount;
};

class CComputerUsage {
public:
	double GetProcessCPU_Idle();

	uint64 GetProcessMemorySize();
private:
	CpuUsage cpuUsage;
};

typedef ACE_Singleton<CComputerUsage, ACE_Recursive_Thread_Mutex> App_ProComputerUsageManager;
//double GetProcessCPU_Idle();
//uint64 GetProcessMemorySize();

#endif
