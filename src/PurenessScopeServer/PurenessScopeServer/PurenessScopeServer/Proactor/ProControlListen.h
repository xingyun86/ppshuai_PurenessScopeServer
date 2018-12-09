#ifndef _PROCONTROLLISTEN_H
#define _PROCONTROLLISTEN_H

#include "ProConnectAccept.h"
#include "AceProactorManager.h"
#include "IControlListen.h"

//连接管理类，用于创建删除和查看监听
//add by freeeyes
class CProControlListen : public IControlListen
{
public:
    CProControlListen();
    virtual ~CProControlListen();

    bool   AddListen(const char* pListenIP, uint32 u4Port, uint8 u1IPType, int nPacketParseID);  //打开一个新的监听端口
    bool   DelListen(const char* pListenIP, uint32 u4Port);                                      //关闭一个已知的连接
	PControlInfo CreateListenSnapshot(int & nControlInfoNum);									 //生成已打开的监听端口列表快照
	void ReleaseListenSnapshot(PPControlInfo ppControlInfo);									 //释放已打开的监听端口列表快照
    uint32 GetServerID();                                                                        //得到服务器ID
};

typedef ACE_Singleton<CProControlListen, ACE_Null_Mutex> App_ProControlListen;

#endif
