#ifndef _CONNECTACCEPT_H
#define _CONNECTACCEPT_H

#include "ace/Acceptor.h"
#include "ace/SOCK_Acceptor.h"
#include "ace/INET_Addr.h"
#include "ConnectHandler.h"
#include "AceReactorManager.h"

#include <vector>

using namespace std;

//������ͨ�ͻ���
class ConnectAcceptor : public ACE_Acceptor<CConnectHandler, ACE_SOCK_ACCEPTOR>
{
public:
    ConnectAcceptor();

    void InitClientReactor(uint32 u4ClientReactorCount);
    void SetPacketParseInfoID(uint32 u4PaccketParseInfoID);
    uint32 GetPacketParseInfoID();
    
protected:
    virtual int make_svc_handler(CConnectHandler*& sh);

public:
    //��Ϊ������open����������������������������һ��open�ӿ�
    //�ṩ����backlog�ķ���
    int open2(ACE_INET_Addr& local_addr,
              ACE_Reactor* reactor,
              int flags,
              int backlog);

    int Init_Open(const ACE_INET_Addr& local_addr,
                  int flags = 0,
                  int use_select = 1,
                  int reuse_addr = 1,
                  int backlog = ACE_DEFAULT_BACKLOG);

    int Run_Open(ACE_Reactor* reactor);

    char*  GetListenIP();
    uint32 GetListenPort();

private:
    char   m_szListenIP[MAX_BUFF_20];
    uint32 m_u4Port;
    uint32 m_u4AcceptCount;             //���յ���������
    uint32 m_u4ClientReactorCount;      //�ͻ��˷�Ӧ���ĸ���
    uint32 m_u4PacketParseInfoID;       //������������ģ��ID
};

class CConnectAcceptorManager
{
public:
    CConnectAcceptorManager(void);
    ~CConnectAcceptorManager(void);

    bool InitConnectAcceptor(int nCount, uint32 u4ClientReactorCount);
    void Close();
    int GetCount();
    ConnectAcceptor* GetConnectAcceptor(int nIndex);
    ConnectAcceptor* GetNewConnectAcceptor();
    const char* GetError();

    bool Close(const char* pIP, uint32 n4Port);
    bool CheckIPInfo(const char* pIP, uint32 n4Port);

private:
    typedef vector<ConnectAcceptor*> vecConnectAcceptor;
    vecConnectAcceptor m_vecConnectAcceptor;
    int                m_nAcceptorCount;
    char               m_szError[MAX_BUFF_500];
};

typedef ACE_Singleton<CConnectAcceptorManager, ACE_Null_Mutex> App_ConnectAcceptorManager;
#endif