#include "LinuxCPU.h"

double GetProcessCPU_Idle_Linux()
{
    int32 nRet = 0;
    ACE_TString strcmd ="ps -aux | grep ";
    char szbuffid[MAX_BUFF_20];
    ACE_TString strpid;
    sprintf_safe(szbuffid, MAX_BUFF_20, "%d", ACE_OS::getpid());
    strpid = szbuffid;
    strcmd += strpid;
    ACE_TString strCPU = strcmd;
    strCPU +="  |awk '{print $2,$3}' >> aasnowy.txt";
    nRet = system(strCPU.c_str());   //获取CPU命令

    if (nRet == -1)
    {
        return (double)(0.0f);
    }

    char szbuffer[MAX_BUFF_50];

    FILE* fd = ACE_OS::fopen("aasnowy.txt","r");

    if (NULL == fd)
    {
        return (double)(0.0f);
    }

    char* pReturn = ACE_OS::fgets(szbuffer,sizeof(szbuffer)/sizeof(*szbuffer),fd);
	
	ACE_OS::fclose(fd);
    
	if (NULL == pReturn)
    {
        return (double)(0.0f);
    }

    //切分出CPU数据
    bool blFlag = false;
    int32 nLen = (int32)ACE_OS::strlen(szbuffer);
    int32 i = 0;

    for(i = 0; i < nLen; i++)
    {
        if(szbuffer[i] == ' ')
        {
            blFlag = true;
            break;
        }
    }

    char szTmp[MAX_BUFF_50] = {'\0'};

    if(blFlag == true)
    {
        memcpy_safe(&szbuffer[i], (uint32)nLen - i, szTmp, (uint32)MAX_BUFF_50);
        szTmp[nLen - i] = '\0';
    }

	double dCpu = 0.0f;
	dCpu = (double)ACE_OS::strtod(szTmp, 0);
    nRet = system("rm -rf aasnowy.txt");

    return (double)(dCpu);
}

uint64 GetProcessMemorySize_Linux()
{
    int32 nRet = 0;
    ACE_TString strcmd ="ps -aux | grep ";
    char szbuffid[MAX_BUFF_20];
    ACE_TString strpid;
    sprintf_safe(szbuffid, MAX_BUFF_20, "%d", ACE_OS::getpid());
    strpid = szbuffid;
    strcmd += strpid;
    ACE_TString strMem = strcmd;
    strMem +="  |awk '{print $2,$6}' >> aasnowy.txt";
    nRet = ACE_OS::system(strMem.c_str()); //获取内存命令

    if (-1 == nRet)
    {
        return (0LL);
    }

    char szbuffer[MAX_BUFF_50];
    FILE* fd = ACE_OS::fopen("aasnowy.txt","r");

    if (NULL == fd)
    {
        return (0LL);
    }

    char* pReturn = ACE_OS::fgets(szbuffer,sizeof(szbuffer),fd);

	ACE_OS::fclose(fd);

    if (NULL == pReturn)
    {
        return (0LL);
    }

    //切分出内存数据
    bool blFlag = false;
    int32 nLen = (int32)ACE_OS::strlen(szbuffer);
    int32 i = 0;

    for(i = 0; i < nLen; i++)
    {
        if(szbuffer[i] == ' ')
        {
            blFlag = true;
            break;
        }
    }

    char szTmp[MAX_BUFF_50] = {'\0'};

    if(blFlag == true)
    {
        memcpy_safe(&szbuffer[i], (uint32)nLen - i, szTmp, (uint32)MAX_BUFF_50);
        szTmp[nLen - i] = '\0';
    }

    uint64 nMem = 0LL;
    nMem = ACE_OS::strtoull(szTmp, NULL, 0xA);

    nRet = system("rm -rf aasnowy.txt");

    return nMem * 1000;
}
