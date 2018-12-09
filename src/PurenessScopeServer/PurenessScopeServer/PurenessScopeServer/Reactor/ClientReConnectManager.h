#ifndef _CLIENTCONNECTMANAGER_H
#define _CLIENTCONNECTMANAGER_H

#include "ace/Connector.h"
#include "ace/SOCK_Connector.h"

#include "TimerManager.h"
#include "IClientManager.h"
#include "ConnectClient.h"
#include "ReactorUDPClient.h"
#include "HashTable.h"

#define RE_CONNECT_SERVER_TIMEOUT 100*1000
#define WAIT_FOR_RECONNECT_FINISH 5000

typedef ACE_Connector<CConnectClient, ACE_SOCK_CONNECTOR> CConnectClientConnector;

class CReactorClientInfo
{
public:
    CReactorClientInfo();
    ~CReactorClientInfo();

    bool Init(int nServerID, const char* pIP, int nPort, uint8 u1IPType, CConnectClientConnector* pReactorConnect, IClientMessage* pClientMessage, ACE_Reactor* pReactor);  //��ʼ�����ӵ�ַ�Ͷ˿�
    void SetLocalAddr(const char* pIP, int nPort, uint8 u1IPType);                         //�󶨱��ص�IP�Ͷ˿�
    bool Run(bool blIsReady, EM_Server_Connect_State emState = SERVER_CONNECT_RECONNECT);  //��ʼ����
    bool SendData(ACE_Message_Block* pmblk);                                               //��������
    int  GetServerID();                                                                    //�õ�������ID
    bool Close();                                                                          //�رշ���������
    void SetConnectClient(CConnectClient* pConnectClient);                                 //��������״̬
    CConnectClient* GetConnectClient();                                                    //�õ�ProConnectClientָ��
    IClientMessage* GetClientMessage();                                                    //��õ�ǰ����Ϣ����ָ��
    ACE_INET_Addr GetServerAddr();                                                         //��÷������ĵ�ַ
    EM_Server_Connect_State GetServerConnectState();                                       //�õ���ǰ����״̬
    void SetServerConnectState(EM_Server_Connect_State objState);                          //���õ�ǰ����״̬

private:
    ACE_INET_Addr              m_AddrLocal;              //���ص����ӵ�ַ������ָ����
    ACE_INET_Addr              m_AddrServer;             //Զ�̷������ĵ�ַ
    CConnectClient*            m_pConnectClient;         //��ǰ���Ӷ���
    CConnectClientConnector*   m_pReactorConnect;        //Connector���Ӷ���
    IClientMessage*            m_pClientMessage;         //�ص������࣬�ص����ش���ͷ������ݷ���
    int                        m_nServerID;              //Զ�̷�������ID
    ACE_Reactor*               m_pReactor;               //��¼ʹ�õķ�Ӧ��
    EM_Server_Connect_State    m_emConnectState;         //����״̬
};

class CClientReConnectManager : public ACE_Event_Handler, public IClientManager
{
public:
    CClientReConnectManager(void);
    ~CClientReConnectManager(void);

public:
    bool Init(ACE_Reactor* pReactor);
    bool Connect(int nServerID, const char* pIP, int nPort, uint8 u1IPType, IClientMessage* pClientMessage);                                                             //���ӷ�����(TCP)
    bool Connect(int nServerID, const char* pIP, int nPort, uint8 u1IPType, const char* pLocalIP, int nLocalPort, uint8 u1LocalIPType, IClientMessage* pClientMessage);  //���ӷ�����(TCP)��ָ�����ص�ַ
    bool ConnectUDP(int nServerID, const char* pIP, int nPort, uint8 u1IPType, EM_UDP_TYPE emType, IClientUDPMessage* pClientUDPMessage);                                                    //����һ��ָ��UDP�����ӣ�UDP��
    bool ReConnect(int nServerID);                                                                                             //��������һ��ָ���ķ�����(TCP)
    bool CloseByClient(int nServerID);                                                                                         //Զ�̱����ر�(TCP)
    bool Close(int nServerID);                                                                                                 //�ر�����
    bool CloseUDP(int nServerID);                                                                                              //�ر����ӣ�UDP��
    bool ConnectErrorClose(int nServerID);                                                                                     //���ڲ����������ʧ�ܣ���ProConnectClient����
    bool SendData(int nServerID, char*& pData, int nSize, bool blIsDelete = true);                                              //��������
    bool SendDataUDP(int nServerID, const char* pIP, int nPort, const char*& pMessage, uint32 u4Len, bool blIsDelete = true);   //�������ݣ�UDP��
    bool SetHandler(int nServerID, CConnectClient* pConnectClient);                                                            //��ָ����CProConnectClient*�󶨸�nServerID
    IClientMessage* GetClientMessage(int nServerID);                                                                           //���ClientMessage����
    bool StartConnectTask(int nIntervalTime = CONNECT_LIMIT_RETRY);                                                            //�����Զ������Ķ�ʱ��
    void CancelConnectTask();                                                                                                  //�ر�������ʱ��
    void Close();                                                                                                              //�ر���������
    ACE_INET_Addr GetServerAddr(int nServerID);                                                                                //�õ�ָ����������Զ�̵�ַ������Ϣ
    bool SetServerConnectState(int nServerID, EM_Server_Connect_State objState);                                               //����ָ�����ӵ�����״̬
    bool GetServerIPInfo(int nServerID, _ClientIPInfo& objServerIPInfo);                                                       //�õ�һ��nServerID��Ӧ��ServerIP��Ϣ
    bool DeleteIClientMessage(IClientMessage* pClientMessage);                                                                 //ɾ��һ���������ڽ�����IClientMessage

    void GetConnectInfo(vecClientConnectInfo& VecClientConnectInfo);      //���ص�ǰ������ӵ���Ϣ��TCP��
    void GetUDPConnectInfo(vecClientConnectInfo& VecClientConnectInfo);   //���ص�ǰ������ӵ���Ϣ��UDP��
    EM_Server_Connect_State GetConnectState(int nServerID);               //�õ�һ����ǰ����״̬

private:
    bool ConnectTcpInit(int nServerID, const char* pIP, int nPort, uint8 u1IPType, const char* pLocalIP, int nLocalPort, uint8 u1LocalIPType, IClientMessage* pClientMessage, CReactorClientInfo*& pClientInfo);
    bool ConnectUdpInit(int nServerID, CReactorUDPClient*& pReactorUDPClient);

    virtual int handle_timeout(const ACE_Time_Value& current_time, const void* act = 0);               //��ʱ��ִ��

public:
    CHashTable<CReactorClientInfo> m_objClientTCPList;            //TCP�ͻ�������
    CHashTable<CReactorUDPClient>  m_objClientUDPList;            //UDP�ͻ�������
    CConnectClientConnector        m_ReactorConnect;              //Reactor���ӿͻ��˶���
    ACE_Recursive_Thread_Mutex     m_ThreadWritrLock;             //�߳���
    ActiveTimer                    m_ActiveTimer;                 //ʱ�������
    int                            m_nTaskID;                     //��ʱ��⹤��
    ACE_Reactor*                   m_pReactor;                    //��ǰ�ķ�Ӧ��
    bool                           m_blReactorFinish;             //Reactor�Ƿ��Ѿ�ע��
    uint32                         m_u4ConnectServerTimeout;      //���Ӽ��ʱ��
    int32                          m_u4MaxPoolCount;              //���ӳص�����
};

typedef ACE_Singleton<CClientReConnectManager, ACE_Recursive_Thread_Mutex> App_ClientReConnectManager;
#endif