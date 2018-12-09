#include "ControlListen.h"

CControlListen::CControlListen()
{
}

CControlListen::~CControlListen()
{
}

bool CControlListen::AddListen( const char* pListenIP, uint32 u4Port, uint8 u1IPType, int nPacketParseID)
{
    bool blState = App_ConnectAcceptorManager::instance()->CheckIPInfo(pListenIP, u4Port);

    if(true == blState)
    {
        //当前监听已经存在，不可以重复建设
        OUR_DEBUG((LM_INFO, "[CServerManager::AddListen](%s:%d) is exist.\n", pListenIP, u4Port));
        return false;
    }

    ACE_INET_Addr listenAddr;

    //判断IPv4还是IPv6
    int nErr = 0;

    if (u1IPType == TYPE_IPV4)
    {
        nErr = listenAddr.set(u4Port, pListenIP);
    }
    else
    {
        nErr = listenAddr.set(u4Port, pListenIP, 1, PF_INET6);
    }

    if (nErr != 0)
    {
        OUR_DEBUG((LM_INFO, "[CControlListen::AddListen](%s:%d)set_address error[%d].\n", pListenIP, u4Port, errno));
        return false;
    }

    //得到接收器
    ConnectAcceptor* pConnectAcceptor = App_ConnectAcceptorManager::instance()->GetNewConnectAcceptor();

    if (NULL == pConnectAcceptor)
    {
        OUR_DEBUG((LM_INFO, "[CControlListen::AddListen](%s:%d)pConnectAcceptor is NULL.\n", pListenIP, u4Port));
        return false;
    }

    pConnectAcceptor->SetPacketParseInfoID(nPacketParseID);

    int nRet = pConnectAcceptor->open2(listenAddr, App_ReactorManager::instance()->GetAce_Reactor(REACTOR_CLIENTDEFINE), ACE_NONBLOCK, (int)App_MainConfig::instance()->GetBacklog());

    if (-1 == nRet)
    {
        OUR_DEBUG((LM_INFO, "[CControlListen::AddListen] Listen from [%s:%d] error(%d).\n", listenAddr.get_host_addr(), listenAddr.get_port_number(), errno));
        return false;
    }

    OUR_DEBUG((LM_INFO, "[CControlListen::AddListen] Listen from [%s:%d] OK.\n", listenAddr.get_host_addr(), listenAddr.get_port_number()));

    return true;
}

bool CControlListen::DelListen( const char* pListenIP, uint32 u4Port )
{
    bool blState = App_ConnectAcceptorManager::instance()->CheckIPInfo(pListenIP, u4Port);

    if(false == blState)
    {
        //当前监听已经存在，不可以重复建设
        OUR_DEBUG((LM_INFO, "[CControlListen::AddListen](%s:%d) is exist.\n", pListenIP, u4Port));
        return false;
    }

    return App_ConnectAcceptorManager::instance()->Close(pListenIP, u4Port);
}

PControlInfo CControlListen::CreateListenSnapshot(int & nControlInfoNum)
{
	int nCount = 0;
	PControlInfo p_controlInfo = NULL;

	nControlInfoNum = 0;

	if (0 == App_ConnectAcceptorManager::instance()->GetCount())
	{
		//监控尚未启动，需要从配置文件中获取
		nCount = App_MainConfig::instance()->GetServerPortCount();
		p_controlInfo = new ControlInfo[nCount];
		if (p_controlInfo)
		{
			for (int i = 0; i < nCount; i++)
			{
				_ServerInfo * pServerInfo = App_MainConfig::instance()->GetServerPort(i);
				if (pServerInfo != NULL)
				{
					sprintf_safe(p_controlInfo[nControlInfoNum].m_szListenIP,
						MAX_BUFF_50, "%s", pServerInfo->m_szServerIP);
					p_controlInfo[nControlInfoNum].m_u4Port = pServerInfo->m_nPort;
					nControlInfoNum++;
				}
			}
		}
	}
	else
	{
		nCount = App_ConnectAcceptorManager::instance()->GetCount();
		p_controlInfo = new ControlInfo[nCount];
		for (int i = 0; i < nCount; i++)
		{
			ConnectAcceptor* pConnectAcceptor = App_ConnectAcceptorManager::instance()->GetConnectAcceptor(i);

			if (pConnectAcceptor != NULL)
			{
				sprintf_safe(p_controlInfo[nControlInfoNum].m_szListenIP,
					MAX_BUFF_50, "%s", pConnectAcceptor->GetListenIP());
				p_controlInfo[nControlInfoNum].m_u4Port = pConnectAcceptor->GetListenPort();
				nControlInfoNum++;
			}
		}
	}
	return p_controlInfo;
}
void CControlListen::ReleaseListenSnapshot(PPControlInfo ppControlInfo)
{
	if (ppControlInfo && (*ppControlInfo))
	{
		delete[](*ppControlInfo);
		(*ppControlInfo) = NULL;
	}
}
uint32 CControlListen::GetServerID()
{
    return App_MainConfig::instance()->GetServerID();
}
