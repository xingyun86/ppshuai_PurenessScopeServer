#ifndef _ICONTROLLISTEN_H
#define _ICONTROLLISTEN_H

#include "define.h"

typedef struct _ControlInfo
{
    char m_szListenIP[MAX_BUFF_50];
    uint32 m_u4Port;

    _ControlInfo()
    {
        m_szListenIP[0] = '\0';
        m_u4Port        = 0;
    }
}ControlInfo, *PControlInfo, **PPControlInfo;

class IControlListen
{
public:
    virtual ~IControlListen() {}
    virtual bool   AddListen(const char* pListenIP, uint32 u4Port, uint8 u1IPType, int nPacketParseID) = 0; //打开一个新的监听端口
    virtual bool   DelListen(const char* pListenIP, uint32 u4Port)                                     = 0; //关闭一个已知的连接
	virtual PControlInfo CreateListenSnapshot(int & nControlInfoNum)							       = 0; //生成已打开的监听端口列表快照
	virtual void ReleaseListenSnapshot(PPControlInfo ppControlInfo)								   = 0;	//释放已打开的监听端口列表快照
	virtual uint32 GetServerID()                                                                       = 0; //得到当前服务器的ID
};

#endif
