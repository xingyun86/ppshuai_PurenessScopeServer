#ifndef _CONTROLLISTEN_H
#define _CONTROLLISTEN_H

#include "ConnectAccept.h"
#include "AceReactorManager.h"
#include "IControlListen.h"

class CControlListen : public IControlListen
{
public:
    CControlListen();
    virtual ~CControlListen();

    bool   AddListen(const char* pListenIP, uint32 u4Port, uint8 u1IPType, int nPacketParseID);  //打开一个新的监听端口
    bool   DelListen(const char* pListenIP, uint32 u4Port);                                      //关闭一个已知的连接
	PControlInfo CreateListenSnapshot(int & nControlInfoNum);									 //生成已打开的监听端口列表快照
	void ReleaseListenSnapshot(PPControlInfo ppControlInfo);									 //释放已打开的监听端口列表快照								
    uint32 GetServerID();                                                                        //得到服务器ID
};

typedef ACE_Singleton<CControlListen, ACE_Null_Mutex> App_ControlListen;

#endif
