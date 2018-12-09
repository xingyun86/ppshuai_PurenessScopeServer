#include "ProConnectHandle.h"

CProConnectHandle::CProConnectHandle(void) : m_u4LocalPort(0), m_u4SendCheckTime(0)
{
    m_szError[0]          = '\0';
    m_u4ConnectID         = 0;
    m_u4AllRecvCount      = 0;
    m_u4AllSendCount      = 0;
    m_u4AllRecvSize       = 0;
    m_u4AllSendSize       = 0;
    m_nIOCount            = 0;
    m_u4HandlerID         = 0;
    m_u2MaxConnectTime    = 0;
    m_u4SendThresHold     = MAX_MSG_SNEDTHRESHOLD;
    m_u2SendQueueMax      = MAX_MSG_SENDPACKET;
    m_u1ConnectState      = CONNECT_INIT;
    m_u1SendBuffState     = CONNECT_SENDNON;
    m_pPacketParse        = NULL;
    m_u4MaxPacketSize     = MAX_MSG_PACKETLENGTH;
    m_u8RecvQueueTimeCost = 0;
    m_u4RecvQueueCount    = 0;
    m_u8SendQueueTimeCost = 0;
    m_u4ReadSendSize      = 0;
    m_u4SuccessSendSize   = 0;
    m_u1IsActive          = 0;
    m_pBlockMessage       = NULL;
    m_u2SendQueueTimeout  = MAX_QUEUE_TIMEOUT * 1000;  //Ŀǰ��Ϊ��¼����΢�룬����������Ӧ������1000��
    m_u2RecvQueueTimeout  = MAX_QUEUE_TIMEOUT * 1000;  //Ŀǰ��Ϊ��¼����΢�룬����������Ӧ������1000��
    m_u2TcpNodelay        = TCP_NODELAY_ON;
    m_emStatus            = CLIENT_CLOSE_NOTHING;
    m_u4SendMaxBuffSize   = 5*1024;
    m_nHashID             = 0;
    m_szConnectName[0]    = '\0';
    m_blIsLog             = false;
    m_szLocalIP[0]        = '\0';
    m_u4RecvPacketCount   = 0;
    m_u4PacketParseInfoID = 0;
    m_u4PacketDebugSize   = 0;
    m_pPacketDebugData    = NULL;
    m_emIOType            = NET_INPUT;
    m_pFileTest           = NULL;
}

CProConnectHandle::~CProConnectHandle(void)
{
    SAFE_DELETE(m_pPacketDebugData);
    m_pPacketDebugData  = NULL;
    m_u4PacketDebugSize = 0;
}

void CProConnectHandle::Init(uint16 u2HandlerID)
{
    m_u4HandlerID      = u2HandlerID;
    m_u2MaxConnectTime = App_MainConfig::instance()->GetMaxConnectTime();
    m_u4SendThresHold  = App_MainConfig::instance()->GetSendTimeout();
    m_u2SendQueueMax   = App_MainConfig::instance()->GetSendQueueMax();
    m_u4MaxPacketSize  = App_MainConfig::instance()->GetRecvBuffSize();
    m_u2TcpNodelay     = App_MainConfig::instance()->GetTcpNodelay();

    m_u2SendQueueTimeout = App_MainConfig::instance()->GetSendQueueTimeout() * 1000;

    if(m_u2SendQueueTimeout == 0)
    {
        m_u2SendQueueTimeout = MAX_QUEUE_TIMEOUT * 1000;
    }

    m_u2RecvQueueTimeout = App_MainConfig::instance()->GetRecvQueueTimeout() * 1000;

    if(m_u2RecvQueueTimeout == 0)
    {
        m_u2RecvQueueTimeout = MAX_QUEUE_TIMEOUT * 1000;
    }

    m_u4SendMaxBuffSize  = App_MainConfig::instance()->GetBlockSize();
    m_emStatus           = CLIENT_CLOSE_NOTHING;

    m_pPacketDebugData   = new char[App_MainConfig::instance()->GetDebugSize()];
    m_u4PacketDebugSize  = App_MainConfig::instance()->GetDebugSize() / 5;
}

uint32 CProConnectHandle::GetHandlerID()
{
    return m_u4HandlerID;
}

const char* CProConnectHandle::GetError()
{
    return m_szError;
}

void CProConnectHandle::Close(int nIOCount, int nErrno)
{
    m_ThreadWriteLock.acquire();

    if(nIOCount > m_nIOCount)
    {
        m_nIOCount = 0;
    }

    //OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::Close]ConnectID=%d, m_nIOCount = %d, nIOCount = %d.\n", GetConnectID(), m_nIOCount, nIOCount));

    if(m_nIOCount > 0)
    {
        m_nIOCount -= nIOCount;
    }

    if(m_nIOCount == 0)
    {
        m_u1IsActive = 0;
    }

    m_ThreadWriteLock.release();

    if(m_nIOCount == 0)
    {
        m_ThreadWriteLock.acquire();

        //�������ӶϿ���Ϣ��֪ͨPacketParse�ӿ�
        App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->DisConnect(GetConnectID());

        //֪ͨ�߼��ӿڣ������Ѿ��Ͽ�
        OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::Close]Connectid=[%d] error(%d)...\n", GetConnectID(), nErrno));
        AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, RecvQueueCount=%d, RecvQueueTimeCost=%I64dns, SendQueueTimeCost=%I64dns.",
                                            m_addrRemote.get_host_addr(),
                                            m_addrRemote.get_port_number(),
                                            m_u4AllRecvSize,
                                            m_u4AllRecvCount,
                                            m_u4AllSendSize,
                                            m_u4AllSendCount,
                                            m_u4RecvQueueCount,
                                            m_u8RecvQueueTimeCost,
                                            m_u8SendQueueTimeCost);

        //��֯����
        _MakePacket objMakePacket;

        objMakePacket.m_u4ConnectID       = GetConnectID();
        objMakePacket.m_pPacketParse      = NULL;
        objMakePacket.m_AddrRemote        = m_addrRemote;
        objMakePacket.m_u1Option          = PACKET_CDISCONNECT;

        if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
        {
            objMakePacket.m_AddrListen.set(m_u4LocalPort);
        }
        else
        {
            objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
        }

        //���Ϳͻ������ӶϿ���Ϣ��
        ACE_Time_Value tvNow = ACE_OS::gettimeofday();

        if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
        }

        m_Reader.cancel();
        m_Writer.cancel();

        ACE_OS::shutdown(this->handle(), SD_BOTH);

        if(this->handle() != ACE_INVALID_HANDLE)
        {
            ACE_OS::closesocket(this->handle());
            this->handle(ACE_INVALID_HANDLE);
        }

        m_ThreadWriteLock.release();

        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close](0x%08x)Close(ConnectID=%d) OK.\n", this, GetConnectID()));

        //ɾ�������б��еĶ�������
        App_ProConnectManager::instance()->Close(GetConnectID());

        //������ָ�����ճ���
        App_ProConnectHandlerPool::instance()->Delete(this);
    }
}

bool CProConnectHandle::ServerClose(EM_Client_Close_status emStatus, uint8 u1OptionEvent)
{
    if (NET_INPUT == m_emIOType)
    {
        if (CLIENT_CLOSE_IMMEDIATLY == emStatus)
        {
            //��֯����
            _MakePacket objMakePacket;

            objMakePacket.m_u4ConnectID  = GetConnectID();
            objMakePacket.m_pPacketParse = NULL;
            objMakePacket.m_u1Option     = u1OptionEvent;
            objMakePacket.m_AddrRemote   = m_addrRemote;

            if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
            {
                objMakePacket.m_AddrListen.set(m_u4LocalPort);
            }
            else
            {
                objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
            }

            //���ͷ����������ӶϿ���Ϣ��
            ACE_Time_Value tvNow = ACE_OS::gettimeofday();

            if (false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
            {
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::ServerClose]ConnectID = %d, PACKET_SDISCONNECT is error.\n", GetConnectID()));
                return false;
            }

            m_Reader.cancel();
            m_Writer.cancel();

            ACE_OS::shutdown(this->handle(), SD_BOTH);

            if (this->handle() != ACE_INVALID_HANDLE)
            {
                ACE_OS::closesocket(this->handle());
                this->handle(ACE_INVALID_HANDLE);
            }

            m_u1ConnectState = CONNECT_SERVER_CLOSE;

        }
        else
        {
            m_emStatus = emStatus;
        }
    }

    return true;
}

void CProConnectHandle::SetConnectID(uint32 u4ConnectID)
{
    m_u4ConnectID = u4ConnectID;
}

uint32 CProConnectHandle::GetConnectID()
{
    return m_u4ConnectID;
}

void CProConnectHandle::addresses (const ACE_INET_Addr& remote_address, const ACE_INET_Addr& local_address)
{
    m_addrRemote = remote_address;
}

uint32 CProConnectHandle::file_open(IFileTestManager* pFileTest)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

    m_atvConnect = ACE_OS::gettimeofday();
    m_atvInput = ACE_OS::gettimeofday();
    m_atvOutput = ACE_OS::gettimeofday();
    m_atvSendAlive = ACE_OS::gettimeofday();

    m_u4AllRecvCount = 0;
    m_u4AllSendCount = 0;
    m_u4AllRecvSize = 0;
    m_u4AllSendSize = 0;
    m_u4RecvPacketCount = 0;
    m_nIOCount = 1;
    m_u8RecvQueueTimeCost = 0;
    m_u4RecvQueueCount = 0;
    m_u8SendQueueTimeCost = 0;
    m_u4SuccessSendSize = 0;
    m_u4ReadSendSize = 0;
    m_emStatus = CLIENT_CLOSE_NOTHING;
    m_blIsLog = false;
    m_szConnectName[0] = '\0';
    m_u1IsActive = 1;
    m_emIOType = FILE_INPUT;

    //��ʼ�������
    m_TimeConnectInfo.Init(App_MainConfig::instance()->GetClientDataAlert()->m_u4RecvPacketCount,
                           App_MainConfig::instance()->GetClientDataAlert()->m_u4RecvDataMax,
                           App_MainConfig::instance()->GetClientDataAlert()->m_u4SendPacketCount,
                           App_MainConfig::instance()->GetClientDataAlert()->m_u4SendDataMax);

    //��������ӷ������ӿ�
    if (false == App_ProConnectManager::instance()->AddConnect(this))
    {
        OUR_DEBUG((LM_ERROR, "%s.\n", App_ProConnectManager::instance()->GetError()));
        sprintf_safe(m_szError, MAX_BUFF_500, "%s", App_ProConnectManager::instance()->GetError());
        return 0;
    }

    //��֯����
    _MakePacket objMakePacket;

    objMakePacket.m_u4ConnectID = GetConnectID();
    objMakePacket.m_pPacketParse = NULL;
    objMakePacket.m_u1Option = PACKET_CONNECT;
    objMakePacket.m_AddrRemote = m_addrRemote;

    if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
    {
        objMakePacket.m_AddrListen.set(m_u4LocalPort);
    }
    else
    {
        objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
    }

    //�������ӽ�����Ϣ��
    ACE_Time_Value tvNow = ACE_OS::gettimeofday();

    if (false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::file_open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
    }

    OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::file_open]Open(%d) Connection from [%s:%d](0x%08x).\n", GetConnectID(), m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), this));
    m_pFileTest = pFileTest;

    return GetConnectID();
}

int CProConnectHandle::handle_write_file_stream(const char* pData, uint32 u4Size, uint8 u1ParseID)
{
    m_pPacketParse = App_PacketParsePool::instance()->Create();

    if (NULL == m_pPacketParse)
    {
        OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::handle_write_file_stream](%d) m_pPacketParse new error.\n", GetConnectID()));
        return -1;
    }

    if (App_PacketParseLoader::instance()->GetPacketParseInfo(u1ParseID)->m_u1PacketParseType == PACKET_WITHHEAD)
    {
        uint32 u4PacketHead = App_PacketParseLoader::instance()->GetPacketParseInfo(u1ParseID)->m_u4OrgLength;
        ACE_Message_Block* pMbHead = App_MessageBlockManager::instance()->Create(u4PacketHead);

        if (NULL == pMbHead)
        {
            OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::handle_write_file_stream](%d) pMbHead is NULL.\n", GetConnectID()));
            return -1;
        }

        memcpy_safe((char*)pData, u4PacketHead, pMbHead->wr_ptr(), u4PacketHead);
        pMbHead->wr_ptr(u4PacketHead);

        //������Ϣͷ
        _Head_Info objHeadInfo;
        bool blStateHead = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Parse_Packet_Head_Info(GetConnectID(), pMbHead, App_MessageBlockManager::instance(), &objHeadInfo);

        if (false == blStateHead)
        {
            OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::handle_write_file_stream](%d) Parse_Packet_Head_Info is illegal.\n", GetConnectID()));
            ClearPacketParse(*pMbHead);
            return -1;
        }

        m_pPacketParse->SetPacket_IsHandleHead(false);
        m_pPacketParse->SetPacket_Head_Message(objHeadInfo.m_pmbHead);
        m_pPacketParse->SetPacket_Head_Curr_Length(objHeadInfo.m_u4HeadCurrLen);
        m_pPacketParse->SetPacket_Body_Src_Length(objHeadInfo.m_u4BodySrcLen);
        m_pPacketParse->SetPacket_CommandID(objHeadInfo.m_u2PacketCommandID);

        //������Ϣ��
        ACE_Message_Block* pMbBody = App_MessageBlockManager::instance()->Create(u4Size - u4PacketHead);

        if (NULL == pMbBody)
        {
            OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::handle_write_file_stream](%d) pMbHead is NULL.\n", GetConnectID()));
            return -1;
        }

        memcpy_safe((char*)&pData[u4PacketHead], u4Size - u4PacketHead, pMbBody->wr_ptr(), u4Size - u4PacketHead);
        pMbBody->wr_ptr(u4Size - u4PacketHead);

        //�������ݰ���
        _Body_Info obj_Body_Info;
        bool blStateBody = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Parse_Packet_Body_Info(GetConnectID(), pMbBody, App_MessageBlockManager::instance(), &obj_Body_Info);

        if (false == blStateBody)
        {
            //������ݰ���Ƿ����Ͽ�����
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_write_file_stream]Parse_Packet_Body_Info is illegal.\n"));

            //����PacketParse
            ClearPacketParse(*pMbBody);
            return -1;
        }
        else
        {
            m_pPacketParse->SetPacket_Body_Message(obj_Body_Info.m_pmbBody);
            m_pPacketParse->SetPacket_Body_Curr_Length(obj_Body_Info.m_u4BodyCurrLen);

            if (obj_Body_Info.m_u2PacketCommandID > 0)
            {
                m_pPacketParse->SetPacket_CommandID(obj_Body_Info.m_u2PacketCommandID);
            }
        }

        if (false == CheckMessage())
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_write_file_stream]CheckMessage is false.\n"));
            return -1;
        }
    }
    else
    {
        //��ģʽ�����ļ�����
        ACE_Message_Block* pMbStream = App_MessageBlockManager::instance()->Create(u4Size);

        if (NULL == pMbStream)
        {
            OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::handle_write_file_stream](%d) pMbHead is NULL.\n", GetConnectID()));
            return -1;
        }

        memcpy_safe((char*)pData, u4Size, pMbStream->wr_ptr(), u4Size);

        _Packet_Info obj_Packet_Info;
        uint8 n1Ret = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Parse_Packet_Stream(GetConnectID(), pMbStream, dynamic_cast<IMessageBlockManager*>(App_MessageBlockManager::instance()), &obj_Packet_Info);

        if (PACKET_GET_ENOUGTH == n1Ret)
        {
            m_pPacketParse->SetPacket_Head_Message(obj_Packet_Info.m_pmbHead);
            m_pPacketParse->SetPacket_Body_Message(obj_Packet_Info.m_pmbBody);
            m_pPacketParse->SetPacket_CommandID(obj_Packet_Info.m_u2PacketCommandID);
            m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4HeadSrcLen);
            m_pPacketParse->SetPacket_Head_Curr_Length(obj_Packet_Info.m_u4HeadCurrLen);
            m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4BodySrcLen);
            m_pPacketParse->SetPacket_Body_Curr_Length(obj_Packet_Info.m_u4BodyCurrLen);

            //�Ѿ��������������ݰ����Ӹ������߳�ȥ����
            if (false == CheckMessage())
            {
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_write_file_stream]CheckMessage is false.\n"));
                return -1;
            }

        }
        else if (PACKET_GET_NO_ENOUGTH == n1Ret)
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_write_file_stream]Parse_Packet_Stream is PACKET_GET_NO_ENOUGTH.\n"));
            //���յ����ݲ�����
            ClearPacketParse(*pMbStream);
            return -1;
        }
        else
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_write_file_stream]Parse_Packet_Stream is PACKET_GET_ERROR.\n"));
            ClearPacketParse(*pMbStream);
            return -1;
        }
    }

    return 0;
}

void CProConnectHandle::open(ACE_HANDLE h, ACE_Message_Block&)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
    //OUR_DEBUG((LM_INFO, "[CProConnectHandle::open] [0x%08x]Connection from [%s:%d]\n", this, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number()));

    m_atvConnect      = ACE_OS::gettimeofday();
    m_atvInput        = ACE_OS::gettimeofday();
    m_atvOutput       = ACE_OS::gettimeofday();
    m_atvSendAlive    = ACE_OS::gettimeofday();

    m_u4AllRecvCount      = 0;
    m_u4AllSendCount      = 0;
    m_u4AllRecvSize       = 0;
    m_u4AllSendSize       = 0;
    m_u4RecvPacketCount   = 0;
    m_nIOCount            = 1;
    m_u8RecvQueueTimeCost = 0;
    m_u4RecvQueueCount    = 0;
    m_u8SendQueueTimeCost = 0;
    m_u4SuccessSendSize   = 0;
    m_u4ReadSendSize      = 0;
    m_emStatus            = CLIENT_CLOSE_NOTHING;
    m_blIsLog             = false;
    m_szConnectName[0]    = '\0';
    m_u1IsActive          = 1;
    m_emIOType            = NET_INPUT;

    if (NULL == App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID))
    {
        //��������������ڣ���ֱ�ӶϿ�����
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open](%s)can't find PacketParseInfo.\n", m_addrRemote.get_host_addr()));
        Close();
        return;
    }

    if(App_ForbiddenIP::instance()->CheckIP(m_addrRemote.get_host_addr()) == false)
    {
        //�ڽ�ֹ�б��У�����������
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open]IP Forbidden(%s).\n", m_addrRemote.get_host_addr()));
        Close();
        return;
    }

    //��鵥λʱ�����Ӵ����Ƿ�ﵽ����
    if(false == App_IPAccount::instance()->AddIP((string)m_addrRemote.get_host_addr()))
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open]IP(%s) connect frequently.\n", m_addrRemote.get_host_addr()));
        App_ForbiddenIP::instance()->AddTempIP(m_addrRemote.get_host_addr(), App_MainConfig::instance()->GetIPAlert()->m_u4IPTimeout);

        //���͸澯�ʼ�
        AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
                                               App_MainConfig::instance()->GetIPAlert()->m_u4MailID,
                                               "Alert IP",
                                               "[CProConnectHandle::open] IP is more than IP Max,");

        Close();
        return;
    }

    if(m_u2TcpNodelay == TCP_NODELAY_OFF)
    {
        //��������˽���Nagle�㷨��������Ҫ���á�
        int nOpt = 1;
        ACE_OS::setsockopt(h, IPPROTO_TCP, TCP_NODELAY, (char* )&nOpt, sizeof(int));
    }

    //��ʼ�������
    m_TimeConnectInfo.Init(App_MainConfig::instance()->GetClientDataAlert()->m_u4RecvPacketCount,
                           App_MainConfig::instance()->GetClientDataAlert()->m_u4RecvDataMax,
                           App_MainConfig::instance()->GetClientDataAlert()->m_u4SendPacketCount,
                           App_MainConfig::instance()->GetClientDataAlert()->m_u4SendDataMax);

    this->handle(h);

    //Ĭ�ϱ�����IP��ַ
    SetConnectName(m_addrRemote.get_host_addr());

    if(this->m_Reader.open(*this, h, 0, proactor()) == -1 ||
       this->m_Writer.open(*this, h, 0, proactor()) == -1)
    {
        OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::open] m_reader or m_reader == 0.\n"));
        Close();
        return;
    }

    //д��������־
    AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Connection from [%s:%d]To Server GetHandlerID=%d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), GetHandlerID());

    //ACE_Sig_Action writeAction((ACE_SignalHandler)SIG_IGN);
    //writeAction.register_action(SIGPIPE, 0);

    //int nTecvBuffSize = MAX_MSG_SOCKETBUFF;
    //ACE_OS::setsockopt(this->get_handle(), SOL_SOCKET, SO_RCVBUF, (char* )&nTecvBuffSize, sizeof(nTecvBuffSize));
    //ACE_OS::setsockopt(h, SOL_SOCKET, SO_SNDBUF, (char* )&nTecvBuffSize, sizeof(nTecvBuffSize));

    //��������ӷ������ӿ�
    if(false == App_ProConnectManager::instance()->AddConnect(this))
    {
        OUR_DEBUG((LM_ERROR, "%s.\n", App_ProConnectManager::instance()->GetError()));
        sprintf_safe(m_szError, MAX_BUFF_500, "%s", App_ProConnectManager::instance()->GetError());
        Close();
        return;
    }

    m_u1ConnectState = CONNECT_OPEN;

    //OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::open] Open(%d).\n", GetConnectID()));

    m_pPacketParse = App_PacketParsePool::instance()->Create();

    if(NULL == m_pPacketParse)
    {
        OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::open] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
        Close();
        return;
    }

    //����PacketParse����Ӧ����
    App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Connect(GetConnectID(), GetClientIPInfo(), GetLocalIPInfo());

    //��֯����
    _MakePacket objMakePacket;

    objMakePacket.m_u4ConnectID       = GetConnectID();
    objMakePacket.m_pPacketParse      = NULL;
    objMakePacket.m_u1Option          = PACKET_CONNECT;
    objMakePacket.m_AddrRemote        = m_addrRemote;

    if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
    {
        objMakePacket.m_AddrListen.set(m_u4LocalPort);
    }
    else
    {
        objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
    }

    //�������ӽ�����Ϣ��
    ACE_Time_Value tvNow = ACE_OS::gettimeofday();

    if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
    }

    OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::open]Open(%d) Connection from [%s:%d](0x%08x).\n", GetConnectID(), m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), this));

    if(App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->m_u1PacketParseType == PACKET_WITHHEAD)
    {
        if (false == RecvClinetPacket(App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->m_u4OrgLength))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectHandle::open](%d)RecvClinetPacket error.\n", GetConnectID()));
        }
    }
    else
    {
        if (false == RecvClinetPacket(App_MainConfig::instance()->GetServerRecvBuff()))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectHandle::open](%d)RecvClinetPacket error.\n", GetConnectID()));
        }
    }

    return;
}

void CProConnectHandle::handle_read_stream(const ACE_Asynch_Read_Stream::Result& result)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
    ACE_Message_Block& mb = result.message_block();
    uint32 u4PacketLen = (uint32)result.bytes_transferred();
    int nTran = (int)result.bytes_transferred();

    //OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::handle_read_stream](%d)  bytes_transferred=%d, bytes_to_read=%d.\n", GetConnectID(),  result.bytes_transferred(), result.bytes_to_read()));

    if(!result.success() || result.bytes_transferred() == 0)
    {
        //���ӶϿ�
        //����PacketParse
        ClearPacketParse(mb);

        //�رյ�ǰ����
        Close(2, errno);

        return;
    }

    m_atvInput = ACE_OS::gettimeofday();

    //�����DEBUG״̬����¼��ǰ���ܰ��Ķ���������
    if(App_MainConfig::instance()->GetDebug() == DEBUG_ON || m_blIsLog == true)
    {
        char szLog[10]  = {'\0'};
        int  nDebugSize = 0;
        bool blblMore   = false;

        if(mb.length() >= m_u4PacketDebugSize)
        {
            nDebugSize = m_u4PacketDebugSize - 1;
            blblMore   = true;
        }
        else
        {
            nDebugSize = (int)mb.length();
        }

        char* pData = mb.rd_ptr();

        for(int i = 0; i < nDebugSize; i++)
        {
            sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
            sprintf_safe(m_pPacketDebugData + 5*i, MAX_BUFF_1024 - 5*i, "%s", szLog);
        }

        m_pPacketDebugData[5 * nDebugSize] = '\0';

        if(blblMore == true)
        {
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTRECV, "[(%s)%s:%d]%s.(���ݰ�����ֻ��¼ǰ200�ֽ�)", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_pPacketDebugData);
        }
        else
        {
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTRECV, "[(%s)%s:%d]%s.", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_pPacketDebugData);
        }
    }

    if(App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->m_u1PacketParseType == PACKET_WITHHEAD)
    {
        if(result.bytes_transferred() < result.bytes_to_read())
        {
            //�̶���������
            int nRead = (int)result.bytes_to_read() - (int)result.bytes_transferred();

            if(-1 == m_Reader.read(mb, nRead))
            {
                //����PacketParse
                ClearPacketParse(mb);

                //�رյ�ǰ����
                Close(2, errno);
                return;
            }
        }
        else if(mb.length() == App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->m_u4OrgLength && m_pPacketParse->GetIsHandleHead())
        {
            //�ж�ͷ�ĺϷ���
            _Head_Info objHeadInfo;
            bool blStateHead = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Parse_Packet_Head_Info(GetConnectID(), &mb, App_MessageBlockManager::instance(), &objHeadInfo);

            if(false == blStateHead)
            {
                //�����ͷ�ǷǷ��ģ��򷵻ش��󣬶Ͽ����ӡ�
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_read_stream]PacketHead is illegal.\n"));

                //����PacketParse
                ClearPacketParse(mb);

                //�رյ�ǰ����
                Close(2, errno);
                return;
            }
            else
            {
                if (NULL == objHeadInfo.m_pmbHead)
                {
                    OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData]ConnectID=%d, objHeadInfo.m_pmbHead is NULL.\n", GetConnectID()));
                }

                m_pPacketParse->SetPacket_IsHandleHead(false);
                m_pPacketParse->SetPacket_Head_Message(objHeadInfo.m_pmbHead);
                m_pPacketParse->SetPacket_Head_Curr_Length(objHeadInfo.m_u4HeadCurrLen);
                m_pPacketParse->SetPacket_Body_Src_Length(objHeadInfo.m_u4BodySrcLen);
                m_pPacketParse->SetPacket_CommandID(objHeadInfo.m_u2PacketCommandID);
            }

            //��������ֻ������ͷ������
            //�������ֻ�а�ͷ������Ҫ���壬�����������һЩ������������ֻ������ͷ���ӵ�DoMessage()
            uint32 u4PacketBodyLen = m_pPacketParse->GetPacketBodySrcLen();

            if(u4PacketBodyLen == 0)
            {
                //���ֻ�а�ͷû�а��壬��ֱ�Ӷ����߼��ﴦ��
                if(false == CheckMessage())
                {
                    Close(2);
                    return;
                }

                m_pPacketParse = App_PacketParsePool::instance()->Create();

                if(NULL == m_pPacketParse)
                {
                    OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::handle_read_stream] Open(%d) m_pPacketParse new error.\n", GetConnectID()));

                    //��֯����
                    _MakePacket objMakePacket;

                    objMakePacket.m_u4ConnectID       = GetConnectID();
                    objMakePacket.m_pPacketParse      = NULL;
                    objMakePacket.m_u1Option          = PACKET_SDISCONNECT;
                    objMakePacket.m_AddrRemote        = m_addrRemote;

                    if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
                    {
                        objMakePacket.m_AddrListen.set(m_u4LocalPort);
                    }
                    else
                    {
                        objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
                    }

                    //��Ϊ��Ҫ�ر����ӣ�����Ҫ��ر�һ��IO����ӦOpen���õ�1�ĳ�ʼֵ
                    //���ͷ����������ӶϿ���Ϣ��
                    ACE_Time_Value tvNow = ACE_OS::gettimeofday();

                    if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
                    {
                        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
                    }

                    Close(2);
                    return;
                }

                Close();

                //������һ�����ݰ�
                if (false == RecvClinetPacket(App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->m_u4OrgLength))
                {
                    OUR_DEBUG((LM_INFO, "[CProConnectHandle::handle_read_stream](%d) RecvClinetPacket error.\n", GetConnectID()));
                }
            }
            else
            {
                //����������������ȣ�Ϊ�Ƿ�����
                if(u4PacketBodyLen >= m_u4MaxPacketSize)
                {
                    OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_read_stream]u4PacketHeadLen(%d) more than %d.\n", u4PacketBodyLen, m_u4MaxPacketSize));

                    //����PacketParse
                    ClearPacketParse(mb);

                    //�رյ�ǰ����
                    Close(2, errno);
                }
                else
                {
                    Close();

                    if (false == RecvClinetPacket(u4PacketBodyLen))
                    {
                        OUR_DEBUG((LM_INFO, "[CProConnectHandle::handle_read_stream](%d) RecvClinetPacket u4PacketBodyLen error.\n", GetConnectID()));
                    }
                }
            }
        }
        else
        {
            //��������������ɣ���ʼ�����������ݰ�
            _Body_Info obj_Body_Info;
            bool blStateBody = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Parse_Packet_Body_Info(GetConnectID(), &mb, App_MessageBlockManager::instance(), &obj_Body_Info);

            if(false == blStateBody)
            {
                //������ݰ���Ƿ����Ͽ�����
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::handle_read_stream]SetPacketBody is illegal.\n"));

                //����PacketParse
                ClearPacketParse(mb);

                //�رյ�ǰ����
                Close(2, errno);
                return;
            }
            else
            {
                m_pPacketParse->SetPacket_Body_Message(obj_Body_Info.m_pmbBody);
                m_pPacketParse->SetPacket_Body_Curr_Length(obj_Body_Info.m_u4BodyCurrLen);

                if(obj_Body_Info.m_u2PacketCommandID > 0)
                {
                    m_pPacketParse->SetPacket_CommandID(obj_Body_Info.m_u2PacketCommandID);
                }
            }

            if(false == CheckMessage())
            {
                Close(2);
                return;
            }

            m_pPacketParse = App_PacketParsePool::instance()->Create();

            if(NULL == m_pPacketParse)
            {
                OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::handle_read_stream] Open(%d) m_pPacketParse new error.\n", GetConnectID()));

                //��֯����
                _MakePacket objMakePacket;

                objMakePacket.m_u4ConnectID       = GetConnectID();
                objMakePacket.m_pPacketParse      = NULL;
                objMakePacket.m_u1Option          = PACKET_SDISCONNECT;
                objMakePacket.m_AddrRemote = m_addrRemote;

                if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
                {
                    objMakePacket.m_AddrListen.set(m_u4LocalPort);
                }
                else
                {
                    objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
                }

                //��Ϊ��Ҫ�ر����ӣ�����Ҫ��ر�һ��IO����ӦOpen���õ�1�ĳ�ʼֵ
                //���ͷ����������ӶϿ���Ϣ��
                ACE_Time_Value tvNow = ACE_OS::gettimeofday();

                if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
                {
                    OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
                }

                Close(2);
                return;
            }

            Close();

            //������һ�����ݰ�
            if (false == RecvClinetPacket(App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->m_u4OrgLength))
            {
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open](%d)RecvClinetPacket error.\n", GetConnectID()));
            }
        }
    }
    else
    {
        //����ģʽ����
        while(true)
        {
            _Packet_Info obj_Packet_Info;
            uint8 n1Ret = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Parse_Packet_Stream(GetConnectID(), &mb, dynamic_cast<IMessageBlockManager*>(App_MessageBlockManager::instance()), &obj_Packet_Info);

            if(PACKET_GET_ENOUGTH == n1Ret)
            {
                m_pPacketParse->SetPacket_Head_Message(obj_Packet_Info.m_pmbHead);
                m_pPacketParse->SetPacket_Body_Message(obj_Packet_Info.m_pmbBody);
                m_pPacketParse->SetPacket_CommandID(obj_Packet_Info.m_u2PacketCommandID);
                m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4HeadSrcLen);
                m_pPacketParse->SetPacket_Head_Curr_Length(obj_Packet_Info.m_u4HeadCurrLen);
                m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4BodySrcLen);
                m_pPacketParse->SetPacket_Body_Curr_Length(obj_Packet_Info.m_u4BodyCurrLen);

                //�Ѿ��������������ݰ����Ӹ������߳�ȥ����
                if(false == CheckMessage())
                {
                    App_MessageBlockManager::instance()->Close(&mb);

                    Close(2);
                    return;
                }

                m_pPacketParse = App_PacketParsePool::instance()->Create();

                if(NULL == m_pPacketParse)
                {
                    OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::handle_read_stream](%d) m_pPacketParse new error.\n", GetConnectID()));

                    //��֯����
                    _MakePacket objMakePacket;

                    objMakePacket.m_u4ConnectID       = GetConnectID();
                    objMakePacket.m_pPacketParse      = NULL;
                    objMakePacket.m_u1Option          = PACKET_SDISCONNECT;
                    objMakePacket.m_AddrRemote        = m_addrRemote;

                    if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
                    {
                        objMakePacket.m_AddrListen.set(m_u4LocalPort);
                    }
                    else
                    {
                        objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
                    }

                    //��Ϊ��Ҫ�ر����ӣ�����Ҫ��ر�һ��IO����ӦOpen���õ�1�ĳ�ʼֵ
                    //���ͷ����������ӶϿ���Ϣ��
                    ACE_Time_Value tvNow = ACE_OS::gettimeofday();

                    if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
                    {
                        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
                    }

                    App_MessageBlockManager::instance()->Close(&mb);

                    Close(2);
                    return;
                }

                //�����Ƿ���������
                if(mb.length() == 0)
                {
                    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] length = %d.\n",mb.length()));
                    App_MessageBlockManager::instance()->Close(&mb);
                    break;
                }
                else
                {
                    //�������ݣ���������
                    continue;
                }
            }
            else if(PACKET_GET_NO_ENOUGTH == n1Ret)
            {
                //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open]error n1Ret = %d.\n",n1Ret));
                App_MessageBlockManager::instance()->Close(&mb);
                //���յ����ݲ���������Ҫ��������
                break;
            }
            else
            {
                //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open]error n1Ret = %d.\n",n1Ret));
                App_MessageBlockManager::instance()->Close(&mb);
                //���ݰ�Ϊ���������������
                m_pPacketParse->Clear();

                Close(2);
                return;
            }
        }

        Close();

        //������һ�����ݰ�
        if (false == RecvClinetPacket(App_MainConfig::instance()->GetServerRecvBuff()))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectHandle::open](%d)RecvClinetPacket error.\n", GetConnectID()));
        }
    }

    return;
}

void CProConnectHandle::handle_write_stream(const ACE_Asynch_Write_Stream::Result& result)
{
    if(!result.success() || result.bytes_transferred()==0)
    {
        //����ʧ��
        int nErrno = errno;
        OUR_DEBUG ((LM_DEBUG,"[CProConnectHandle::handle_write_stream] Connectid=[%d] begin(%d)...\n",GetConnectID(), nErrno));

        AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "WriteError [%s:%d] nErrno = %d  result.bytes_transferred() = %d, ",
                                            m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), nErrno,
                                            result.bytes_transferred());

        OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::handle_write_stream] Connectid=[%d] finish ok...\n", GetConnectID()));
        m_atvOutput = ACE_OS::gettimeofday();
        //App_MessageBlockManager::instance()->Close(&result.message_block());
        //������Ϣ�ص�
        App_MakePacket::instance()->PutSendErrorMessage(GetConnectID(), &result.message_block(), m_atvOutput);

        //App_MessageBlockManager::instance()->Close(&result.message_block());

        return;
    }
    else
    {
        //���ͳɹ�
        m_ThreadWriteLock.acquire();
        m_atvOutput = ACE_OS::gettimeofday();
        m_u4AllSendSize += (uint32)result.bytes_to_write();

        int nMessageID = result.message_block().msg_type() - ACE_Message_Block::MB_USER;

        if(nMessageID > 0)
        {
            //��Ҫ�ص����ͳɹ���ִ
            _MakePacket objMakePacket;

            CPacketParse objPacketParse;
            ACE_Message_Block* pMbData = App_MessageBlockManager::instance()->Create(sizeof(int));
            memcpy_safe((char* )&nMessageID, sizeof(int), pMbData->wr_ptr(), sizeof(int));
            pMbData->wr_ptr(sizeof(int));
            objPacketParse.SetPacket_Head_Message(pMbData);
            objPacketParse.SetPacket_Head_Curr_Length((uint32)pMbData->length());

            objMakePacket.m_u4ConnectID       = GetConnectID();
            objMakePacket.m_pPacketParse      = &objPacketParse;
            objMakePacket.m_u1Option          = PACKET_SEND_OK;
            objMakePacket.m_AddrRemote        = m_addrRemote;

            if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
            {
                objMakePacket.m_AddrListen.set(m_u4LocalPort);
            }
            else
            {
                objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
            }

            //���Ϳͻ������ӶϿ���Ϣ��
            ACE_Time_Value tvNow = ACE_OS::gettimeofday();

            if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
            {
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
            }

            //��ԭ��Ϣ����
            result.message_block().msg_type(ACE_Message_Block::MB_DATA);
        }

        App_MessageBlockManager::instance()->Close(&result.message_block());
        m_ThreadWriteLock.release();

        //��¼�����ֽ���
        if(m_u4ReadSendSize >= m_u4SuccessSendSize + (uint32)result.bytes_to_write())
        {
            m_u4SuccessSendSize += (uint32)result.bytes_to_write();
        }

        return;
    }
}

void CProConnectHandle::SetRecvQueueTimeCost(uint32 u4TimeCost)
{
    m_ThreadWriteLock.acquire();
    m_nIOCount++;
    m_ThreadWriteLock.release();

    //���������ֵ�����¼����־��ȥ
    if((uint32)(m_u2RecvQueueTimeout * 1000) <= u4TimeCost)
    {
        AppLogManager::instance()->WriteLog(LOG_SYSTEM_RECVQUEUEERROR, "[TCP]IP=%s,Prot=%d,Timeout=[%d].", GetClientIPInfo().m_szClientIP, GetClientIPInfo().m_nPort, u4TimeCost);
    }

    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::SetRecvQueueTimeCost]m_u4RecvQueueCount=%d.\n", m_u4RecvQueueCount));
    m_u4RecvQueueCount++;

    //�����˼��죬�о����ʱ�����壬��Ϊ��ȡ���еĴ���ʱ��Ƭ���ܺܺ�ʱ������һ�����ݵĽ׶���ʱ������
    //ֻҪ��¼��ʱ�����ݼ���
    //m_u8RecvQueueTimeCost += u4TimeCost;

    Close();
}

void CProConnectHandle::SetSendQueueTimeCost(uint32 u4TimeCost)
{
    m_ThreadWriteLock.acquire();
    m_nIOCount++;
    m_ThreadWriteLock.release();

    //���������ֵ�����¼����־��ȥ
    if((uint32)(m_u2SendQueueTimeout * 1000) <= u4TimeCost)
    {
        AppLogManager::instance()->WriteLog(LOG_SYSTEM_SENDQUEUEERROR, "[TCP]IP=%s,Prot=%d,Timeout=[%d].", GetClientIPInfo().m_szClientIP, GetClientIPInfo().m_nPort, u4TimeCost);

        //��֯����
        _MakePacket objMakePacket;

        objMakePacket.m_u4ConnectID       = GetConnectID();
        objMakePacket.m_pPacketParse      = NULL;
        objMakePacket.m_u1Option          = PACKET_SEND_TIMEOUT;
        objMakePacket.m_AddrRemote        = m_addrRemote;

        if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
        {
            objMakePacket.m_AddrListen.set(m_u4LocalPort);
        }
        else
        {
            objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
        }

        //���߲�����ӷ��ͳ�ʱ��ֵ����
        ACE_Time_Value tvNow = ACE_OS::gettimeofday();

        if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
        }
    }

    //m_u8SendQueueTimeCost += u4TimeCost;

    Close();
}

uint8 CProConnectHandle::GetConnectState()
{
    return m_u1ConnectState;
}

uint8 CProConnectHandle::GetSendBuffState()
{
    return m_u1SendBuffState;
}

bool CProConnectHandle::SendMessage(uint16 u2CommandID, IBuffPacket* pBuffPacket, uint8 u1State, uint8 u1SendType, uint32& u4PacketSize, bool blDelete, int nMessageID)
{
    //OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::SendMessage]Connectid=%d,m_nIOCount=%d.\n", GetConnectID(), m_nIOCount));
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
    //OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::SendMessage]Connectid=%d,m_nIOCount=%d 1.\n", GetConnectID(), m_nIOCount));

    if(NULL == pBuffPacket)
    {
        OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::SendMessage] Connectid=[%d] pBuffPacket is NULL.\n", GetConnectID()));
        return false;
    }

    //�����ǰ�����ѱ�����̹߳رգ������ﲻ��������ֱ���˳�
    if(m_u1IsActive == 0)
    {
        //������Ӳ������ˣ������ﷵ��ʧ�ܣ��ص���ҵ���߼�ȥ����
        ACE_Message_Block* pSendMessage = App_MessageBlockManager::instance()->Create(pBuffPacket->GetPacketLen());
        memcpy_safe((char* )pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char* )pSendMessage->wr_ptr(), pBuffPacket->GetPacketLen());
        pSendMessage->wr_ptr(pBuffPacket->GetPacketLen());
        ACE_Time_Value tvNow = ACE_OS::gettimeofday();
        App_MakePacket::instance()->PutSendErrorMessage(0, pSendMessage, tvNow);

        if(blDelete == true)
        {
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
        }

        return false;
    }

    if (NET_INPUT == m_emIOType)
    {
        //�������ֱ�ӷ������ݣ���ƴ�����ݰ�
        if (u1State == PACKET_SEND_CACHE)
        {
            //���ж�Ҫ���͵����ݳ��ȣ������Ƿ���Է��뻺�壬�����Ƿ��Ѿ�������
            uint32 u4SendPacketSize = 0;

            if (u1SendType == SENDMESSAGE_NOMAL)
            {
                u4SendPacketSize = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Make_Send_Packet_Length(GetConnectID(), pBuffPacket->GetPacketLen(), u2CommandID);
            }
            else
            {
                u4SendPacketSize = (uint32)m_pBlockMessage->length();
            }

            u4PacketSize = u4SendPacketSize;

            if (u4SendPacketSize + (uint32)m_pBlockMessage->length() >= m_u4SendMaxBuffSize)
            {
                OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::SendMessage] Connectid=[%d] m_pBlockMessage is not enougth.\n", GetConnectID()));
                //������Ӳ������ˣ������ﷵ��ʧ�ܣ��ص���ҵ���߼�ȥ����
                ACE_Message_Block* pSendMessage = App_MessageBlockManager::instance()->Create(pBuffPacket->GetPacketLen());
                memcpy_safe((char*)pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char*)pSendMessage->wr_ptr(), pBuffPacket->GetPacketLen());
                pSendMessage->wr_ptr(pBuffPacket->GetPacketLen());
                ACE_Time_Value tvNow = ACE_OS::gettimeofday();
                App_MakePacket::instance()->PutSendErrorMessage(0, pSendMessage, tvNow);

                if (blDelete = true)
                {
                    App_BuffPacketManager::instance()->Delete(pBuffPacket);
                }

                return false;
            }
            else
            {
                //���ӽ�������
                //SENDMESSAGE_NOMAL����Ҫ��ͷ��ʱ�򣬷��򣬲����ֱ�ӷ���
                if (u1SendType == SENDMESSAGE_NOMAL)
                {
                    //������ɷ������ݰ�
                    App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Make_Send_Packet(GetConnectID(), pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), m_pBlockMessage, u2CommandID);
                }
                else
                {
                    //�������SENDMESSAGE_NOMAL����ֱ�����
                    memcpy_safe((char*)pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char*)m_pBlockMessage->wr_ptr(), pBuffPacket->GetPacketLen());
                    m_pBlockMessage->wr_ptr(pBuffPacket->GetPacketLen());
                }
            }

            if (blDelete = true)
            {
                App_BuffPacketManager::instance()->Delete(pBuffPacket);
            }

            return true;
        }
        else
        {
            //���ж��Ƿ�Ҫ��װ��ͷ�������Ҫ������װ��m_pBlockMessage��
            uint32 u4SendPacketSize = 0;
            ACE_Message_Block* pMbData = NULL;

            if (u1SendType == SENDMESSAGE_NOMAL)
            {
                u4SendPacketSize = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Make_Send_Packet_Length(GetConnectID(), pBuffPacket->GetPacketLen(), u2CommandID);

                if (u4SendPacketSize >= m_u4SendMaxBuffSize)
                {
                    OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::SendMessage](%d) u4SendPacketSize is more than(%d)(%d).\n", GetConnectID(), u4SendPacketSize, m_u4SendMaxBuffSize));

                    if (blDelete == true)
                    {
                        //ɾ���������ݰ�
                        App_BuffPacketManager::instance()->Delete(pBuffPacket);
                    }

                    return false;
                }

                App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Make_Send_Packet(GetConnectID(), pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), m_pBlockMessage, u2CommandID);
                //����MakePacket�Ѿ��������ݳ��ȣ����������ﲻ��׷��
            }
            else
            {
                u4SendPacketSize = (uint32)pBuffPacket->GetPacketLen();

                if (u4SendPacketSize >= m_u4SendMaxBuffSize)
                {
                    OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::SendMessage](%d) u4SendPacketSize is more than(%d)(%d).\n", GetConnectID(), u4SendPacketSize, m_u4SendMaxBuffSize));
                    //������Ӳ������ˣ������ﷵ��ʧ�ܣ��ص���ҵ���߼�ȥ����
                    ACE_Message_Block* pSendMessage = App_MessageBlockManager::instance()->Create(pBuffPacket->GetPacketLen());
                    memcpy_safe((char*)pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char*)pSendMessage->wr_ptr(), pBuffPacket->GetPacketLen());
                    pSendMessage->wr_ptr(pBuffPacket->GetPacketLen());
                    ACE_Time_Value tvNow = ACE_OS::gettimeofday();
                    App_MakePacket::instance()->PutSendErrorMessage(0, pSendMessage, tvNow);

                    if (blDelete == true)
                    {
                        //ɾ���������ݰ�
                        App_BuffPacketManager::instance()->Delete(pBuffPacket);
                    }

                    return false;
                }

                memcpy_safe((char*)pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char*)m_pBlockMessage->wr_ptr(), pBuffPacket->GetPacketLen());
                m_pBlockMessage->wr_ptr(pBuffPacket->GetPacketLen());
            }

            //���֮ǰ�л������ݣ���ͻ�������һ����
            u4PacketSize = (uint32)m_pBlockMessage->length();

            //���֮ǰ�л������ݣ���ͻ�������һ����
            if (m_pBlockMessage->length() > 0)
            {
                //��Ϊ���첽���ͣ����͵�����ָ�벻���������ͷţ�������Ҫ�����ﴴ��һ���µķ������ݿ飬�����ݿ���
                pMbData = App_MessageBlockManager::instance()->Create((uint32)m_pBlockMessage->length());

                if (NULL == pMbData)
                {
                    OUR_DEBUG((LM_DEBUG, "[CProConnectHandle::SendMessage] Connectid=[%d] pMbData is NULL.\n", GetConnectID()));
                    //������Ӳ������ˣ������ﷵ��ʧ�ܣ��ص���ҵ���߼�ȥ����
                    ACE_Message_Block* pSendMessage = App_MessageBlockManager::instance()->Create(pBuffPacket->GetPacketLen());
                    memcpy_safe((char*)pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char*)pSendMessage->wr_ptr(), pBuffPacket->GetPacketLen());
                    pSendMessage->wr_ptr(pBuffPacket->GetPacketLen());
                    ACE_Time_Value tvNow = ACE_OS::gettimeofday();
                    App_MakePacket::instance()->PutSendErrorMessage(0, pSendMessage, tvNow);

                    if (blDelete == true)
                    {
                        App_BuffPacketManager::instance()->Delete(pBuffPacket);
                    }

                    return false;
                }

                memcpy_safe(m_pBlockMessage->rd_ptr(), (uint32)m_pBlockMessage->length(), pMbData->wr_ptr(), (uint32)m_pBlockMessage->length());
                pMbData->wr_ptr(m_pBlockMessage->length());
                //������ɣ�����ջ������ݣ�ʹ�����
                m_pBlockMessage->reset();
            }

            if (blDelete = true)
            {
                App_BuffPacketManager::instance()->Delete(pBuffPacket);
            }

            //�ж��Ƿ�����ɺ�ر�����
            if (PACKET_SEND_FIN_CLOSE == u1State)
            {
                m_emStatus = CLIENT_CLOSE_SENDOK;
            }

            //����ϢID����MessageBlock
            ACE_Message_Block::ACE_Message_Type  objType = ACE_Message_Block::MB_USER + nMessageID;
            pMbData->msg_type(objType);

            return PutSendPacket(pMbData);
        }
    }
    else
    {
        //�ļ���ڣ�ֱ��д����־
        char szLog[10] = { '\0' };
        uint32 u4DebugSize = 0;
        bool blblMore = false;

        if (pBuffPacket->GetPacketLen() >= m_u4PacketDebugSize)
        {
            u4DebugSize = m_u4PacketDebugSize - 1;
            blblMore = true;
        }
        else
        {
            u4DebugSize = (int)pBuffPacket->GetPacketLen();
        }

        char* pData = (char* )pBuffPacket->GetData();

        for (uint32 i = 0; i < u4DebugSize; i++)
        {
            sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
            sprintf_safe(m_pPacketDebugData + 5 * i, MAX_BUFF_1024 - 5 * i, "0x%02X ", (unsigned char)pData[i]);
        }

        m_pPacketDebugData[5 * u4DebugSize] = '\0';

        if (blblMore == true)
        {
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTSEND, "[(%s)%s:%d]%s.(���ݰ�����)", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_pPacketDebugData);
        }
        else
        {
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTSEND, "[(%s)%s:%d]%s.", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_pPacketDebugData);
        }

        //�ص������ļ������ӿ�
        if (NULL != m_pFileTest)
        {
            m_pFileTest->HandlerServerResponse(GetConnectID());
        }

        if (blDelete == true)
        {
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
        }

        return true;
    }
}

bool CProConnectHandle::PutSendPacket(ACE_Message_Block* pMbData)
{
    if(NULL == pMbData)
    {
        return false;
    }

    //�����DEBUG״̬����¼��ǰ���Ͱ��Ķ���������
    if(App_MainConfig::instance()->GetDebug() == DEBUG_ON || m_blIsLog == true)
    {
        char szLog[10]  = {'\0'};
        uint32 u4DebugSize = 0;
        bool blblMore   = false;

        if(pMbData->length() >= m_u4PacketDebugSize)
        {
            u4DebugSize = m_u4PacketDebugSize - 1;
            blblMore   = true;
        }
        else
        {
            u4DebugSize = (int)pMbData->length();
        }

        char* pData = pMbData->rd_ptr();

        for(uint32 i = 0; i < u4DebugSize; i++)
        {
            sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
            sprintf_safe(m_pPacketDebugData + 5*i, MAX_BUFF_1024 -  5*i, "0x%02X ", (unsigned char)pData[i]);
        }

        m_pPacketDebugData[5 * u4DebugSize] = '\0';

        if(blblMore == true)
        {
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTSEND, "[(%s)%s:%d]%s.(���ݰ�����ֻ��¼ǰ200�ֽ�)", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_pPacketDebugData);
        }
        else
        {
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTSEND, "[(%s)%s:%d]%s.", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_pPacketDebugData);
        }
    }

    //OUR_DEBUG ((LM_ERROR, "[CProConnectHandle::PutSendPacket] Connectid=%d, m_nIOCount=%d!\n", GetConnectID(), m_nIOCount));
    //ͳ�Ʒ�������
    ACE_Date_Time dtNow;

    if(false == m_TimeConnectInfo.SendCheck((uint8)dtNow.minute(), 1, (uint32)pMbData->length()))
    {
        //�������޶��ķ�ֵ����Ҫ�ر����ӣ�����¼��־
        AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECTABNORMAL,
                                               App_MainConfig::instance()->GetClientDataAlert()->m_u4MailID,
                                               "Alert",
                                               "[TCP]IP=%s,Prot=%d,SendPacketCount=%d, SendSize=%d.",
                                               m_addrRemote.get_host_addr(),
                                               m_addrRemote.get_port_number(),
                                               m_TimeConnectInfo.m_u4SendPacketCount,
                                               m_TimeConnectInfo.m_u4SendSize);

        //���÷��ʱ��
        App_ForbiddenIP::instance()->AddTempIP(m_addrRemote.get_host_addr(), App_MainConfig::instance()->GetIPAlert()->m_u4IPTimeout);
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::PutSendPacket] ConnectID = %d, Send Data is more than limit.\n", GetConnectID()));

        App_MessageBlockManager::instance()->Close(pMbData);
        return false;
    }

    //�첽���ͷ���
    if(NULL != pMbData)
    {
        //��Ϊ���첽�ģ����Կ��ܻ���յ��ϴγɹ������ݰ��ۼƣ�������Ҫע��һ�¡�
        if(m_u4SuccessSendSize > m_u4ReadSendSize)
        {
            m_u4SuccessSendSize = m_u4ReadSendSize;
        }

        //�Ƚ�ˮλ�꣬�Ƿ񳬹�һ����ֵ��Ҳ����˵����������ʱ���������һ����ֵ���Ͽ�����
        if(m_u4ReadSendSize - m_u4SuccessSendSize >= App_MainConfig::instance()->GetSendDataMask())
        {
            OUR_DEBUG ((LM_ERROR, "[CProConnectHandle::PutSendPacket]ConnectID = %d, SingleConnectMaxSendBuffer is more than(%d)!\n", GetConnectID(), m_u4ReadSendSize - m_u4SuccessSendSize));
            AppLogManager::instance()->WriteLog(LOG_SYSTEM_SENDQUEUEERROR, "]Connection from [%s:%d], SingleConnectMaxSendBuffer is more than(%d)!.", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4ReadSendSize - m_u4SuccessSendSize);
            //App_MessageBlockManager::instance()->Close(pMbData);

            //���﷢�͸����һ����Ϣ����֪������ݳ�����ֵ
            _MakePacket objMakePacket;

            CPacketParse objPacketParse;
            objPacketParse.SetPacket_Body_Message(pMbData);
            objPacketParse.SetPacket_Body_Curr_Length((uint32)pMbData->length());

            objMakePacket.m_u4ConnectID       = GetConnectID();
            objMakePacket.m_pPacketParse      = &objPacketParse;
            objMakePacket.m_u1Option          = PACKET_SEND_TIMEOUT;
            objMakePacket.m_AddrRemote        = m_addrRemote;

            if (ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
            {
                objMakePacket.m_AddrListen.set(m_u4LocalPort);
            }
            else
            {
                objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
            }

            //���Ϳͻ������ӶϿ���Ϣ��
            ACE_Time_Value tvNow = ACE_OS::gettimeofday();

            if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
            {
                OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
            }

            return false;
        }

        //��¼ˮλ��
        m_u4ReadSendSize += (uint32)pMbData->length();

        //OUR_DEBUG ((LM_ERROR, "[CProConnectHandle::PutSendPacket] Connectid=%d, length=%d!\n", GetConnectID(), pMbData->length()));
        if(0 != m_Writer.write(*pMbData, pMbData->length()))
        {

            OUR_DEBUG ((LM_ERROR, "[CProConnectHandle::PutSendPacket] Connectid=%d mb=%d m_writer.write error(%d)!\n", GetConnectID(),  pMbData->length(), errno));
            //�������ʧ�ܣ������ﷵ��ʧ�ܣ��ص���ҵ���߼�ȥ����
            ACE_Time_Value tvNow = ACE_OS::gettimeofday();
            App_MakePacket::instance()->PutSendErrorMessage(GetConnectID(), pMbData, tvNow);
            return false;
        }
        else
        {
            //OUR_DEBUG ((LM_ERROR, "[CProConnectHandle::PutSendPacket](%s:%d) Send(%d) OK!\n", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), pMbData->length()));
            m_u4AllSendCount += 1;
            m_atvOutput      = ACE_OS::gettimeofday();
            return true;
        }
    }
    else
    {
        OUR_DEBUG ((LM_ERROR,"[CProConnectHandle::PutSendPacket] Connectid=%d mb is NULL!\n", GetConnectID()));;
        return false;
    }
}

bool CProConnectHandle::RecvClinetPacket(uint32 u4PackeLen)
{
    m_nIOCount++;
    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvClinetPacket]Connectid=%d, m_nIOCount=%d.\n", GetConnectID(), m_nIOCount));

    ACE_Message_Block* pmb = App_MessageBlockManager::instance()->Create(u4PackeLen);

    if(NULL == pmb)
    {
        AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, RecvQueueCount=%d, RecvQueueTimeCost=%I64d, SendQueueTimeCost=%I64d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, m_u4RecvQueueCount, m_u8RecvQueueTimeCost, m_u8SendQueueTimeCost);
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvClinetPacket] pmb new is NULL.\n"));
        Close(2);
        return false;
    }

    if(m_Reader.read(*pmb, u4PackeLen) == -1)
    {
        //�����ʧ�ܣ���ر����ӡ�
        AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, RecvQueueCount=%d, RecvQueueTimeCost=%I64d, SendQueueTimeCost=%I64d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, m_u4RecvQueueCount, m_u8RecvQueueTimeCost, m_u8SendQueueTimeCost);
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvClinetPacket] m_reader.read is error(%d)(%d).\n", GetConnectID(), errno));
        ClearPacketParse(*pmb);
        Close(2);
        return false;
    }

    return true;
}

bool CProConnectHandle::CheckMessage()
{
    if(m_pPacketParse->GetMessageHead() != NULL)
    {
        //m_ThreadWriteLock.acquire();
        if(m_pPacketParse->GetMessageBody() == NULL)
        {
            m_u4AllRecvSize += (uint32)m_pPacketParse->GetMessageHead()->length();
        }
        else
        {
            m_u4AllRecvSize += (uint32)m_pPacketParse->GetMessageHead()->length() + (uint32)m_pPacketParse->GetMessageBody()->length();
        }

        m_u4AllRecvCount++;

        ACE_Date_Time dtNow;

        if(false == m_TimeConnectInfo.RecvCheck((uint8)dtNow.minute(), 1, m_u4AllRecvSize))
        {
            //�������޶��ķ�ֵ����Ҫ�ر����ӣ�����¼��־
            AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECTABNORMAL,
                                                   App_MainConfig::instance()->GetClientDataAlert()->m_u4MailID,
                                                   "Alert",
                                                   "[TCP]IP=%s,Prot=%d,PacketCount=%d, RecvSize=%d.",
                                                   m_addrRemote.get_host_addr(),
                                                   m_addrRemote.get_port_number(),
                                                   m_TimeConnectInfo.m_u4RecvPacketCount,
                                                   m_TimeConnectInfo.m_u4RecvSize);


            App_PacketParsePool::instance()->Delete(m_pPacketParse, true);
            //���÷��ʱ��
            App_ForbiddenIP::instance()->AddTempIP(m_addrRemote.get_host_addr(), App_MainConfig::instance()->GetIPAlert()->m_u4IPTimeout);
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::CheckMessage] ConnectID = %d, PutMessageBlock is check invalid.\n", GetConnectID()));
            return false;
        }

        //��֯����
        _MakePacket objMakePacket;

        objMakePacket.m_u4ConnectID       = GetConnectID();
        objMakePacket.m_pPacketParse      = m_pPacketParse;
        objMakePacket.m_AddrRemote        = m_addrRemote;
        objMakePacket.m_u4PacketParseID   = GetPacketParseInfoID();

        if(ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
        {
            objMakePacket.m_AddrListen.set(m_u4LocalPort);
        }
        else
        {
            objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
        }

        objMakePacket.m_u1Option = PACKET_PARSE;

        //������Buff������Ϣ���У����ݸ�MakePacket������
        ACE_Time_Value tvNow = ACE_OS::gettimeofday();

        if(false == App_MakePacket::instance()->PutMessageBlock(&objMakePacket, tvNow))
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::CheckMessage] ConnectID = %d, PutMessageBlock is error.\n", GetConnectID()));
        }

        //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::CheckMessage] ConnectID = %d, put OK.\n", GetConnectID()));
        //����ʱ������
        App_ProConnectManager::instance()->SetConnectTimeWheel(this);

        //���������m_pPacketParse
        App_PacketParsePool::instance()->Delete(m_pPacketParse);
    }
    else
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::CheckMessage] ConnectID = %d, m_pPacketParse is NULL.\n", GetConnectID()));
    }

    return true;
}

_ClientConnectInfo CProConnectHandle::GetClientInfo()
{
    _ClientConnectInfo ClientConnectInfo;

    ClientConnectInfo.m_blValid             = true;
    ClientConnectInfo.m_u4ConnectID         = GetConnectID();
    ClientConnectInfo.m_addrRemote          = m_addrRemote;
    ClientConnectInfo.m_u4RecvCount         = m_u4AllRecvCount;
    ClientConnectInfo.m_u4SendCount         = m_u4AllSendCount;
    ClientConnectInfo.m_u4AllRecvSize       = m_u4AllSendSize;
    ClientConnectInfo.m_u4AllSendSize       = m_u4AllSendSize;
    ClientConnectInfo.m_u4BeginTime         = (uint32)m_atvConnect.sec();
    ClientConnectInfo.m_u4AliveTime         = (uint32)(ACE_OS::gettimeofday().sec()  -  m_atvConnect.sec());
    ClientConnectInfo.m_u4RecvQueueCount    = m_u4RecvQueueCount;
    ClientConnectInfo.m_u8RecvQueueTimeCost = m_u8RecvQueueTimeCost;
    ClientConnectInfo.m_u8SendQueueTimeCost = m_u8SendQueueTimeCost;

    return ClientConnectInfo;
}

_ClientIPInfo CProConnectHandle::GetClientIPInfo()
{
    _ClientIPInfo ClientIPInfo;
    sprintf_safe(ClientIPInfo.m_szClientIP, MAX_BUFF_50, "%s", m_addrRemote.get_host_addr());
    ClientIPInfo.m_nPort = (int)m_addrRemote.get_port_number();
    return ClientIPInfo;
}

_ClientIPInfo CProConnectHandle::GetLocalIPInfo()
{
    _ClientIPInfo ClientIPInfo;
    sprintf_safe(ClientIPInfo.m_szClientIP, MAX_BUFF_50, "%s", m_szLocalIP);
    ClientIPInfo.m_nPort = (int)m_u4LocalPort;
    return ClientIPInfo;
}

void CProConnectHandle::ClearPacketParse(ACE_Message_Block& mbCurrBlock)
{
    if(NULL != m_pPacketParse && m_pPacketParse->GetMessageHead() != NULL)
    {
        App_MessageBlockManager::instance()->Close(m_pPacketParse->GetMessageHead());
    }

    if(NULL != m_pPacketParse && m_pPacketParse->GetMessageBody() != NULL)
    {
        App_MessageBlockManager::instance()->Close(m_pPacketParse->GetMessageBody());
    }

    if(NULL != m_pPacketParse)
    {
        if(NULL != &mbCurrBlock && &mbCurrBlock != m_pPacketParse->GetMessageHead() && &mbCurrBlock != m_pPacketParse->GetMessageBody())
        {
            //OUR_DEBUG((LM_DEBUG,"[CProConnectHandle::handle_read_stream] Message_block release.\n"));
            App_MessageBlockManager::instance()->Close(&mbCurrBlock);
        }

        App_PacketParsePool::instance()->Delete(m_pPacketParse);
        m_pPacketParse = NULL;
    }
    else
    {
        if(NULL != &mbCurrBlock)
        {
            App_MessageBlockManager::instance()->Close(&mbCurrBlock);
        }
    }
}

char* CProConnectHandle::GetConnectName()
{
    return m_szConnectName;
}

void CProConnectHandle::SetConnectName( const char* pName )
{
    sprintf_safe(m_szConnectName, MAX_BUFF_100, "%s", pName);
}

void CProConnectHandle::SetIsLog(bool blIsLog)
{
    m_blIsLog = blIsLog;
}

bool CProConnectHandle::GetIsLog()
{
    return m_blIsLog;
}

void CProConnectHandle::SetHashID(int nHashID)
{
    m_nHashID = nHashID;
}

int CProConnectHandle::GetHashID()
{
    return m_nHashID;
}

void CProConnectHandle::SetLocalIPInfo(const char* pLocalIP, uint32 u4LocalPort)
{
    sprintf_safe(m_szLocalIP, MAX_BUFF_50, "%s", pLocalIP);
    m_u4LocalPort = u4LocalPort;
}

void CProConnectHandle::PutSendPacketError(ACE_Message_Block* pMbData)
{

}

void CProConnectHandle::SetSendCacheManager(ISendCacheManager* pSendCacheManager)
{
    m_pBlockMessage = pSendCacheManager->GetCacheData(GetConnectID());

    if(NULL == m_pBlockMessage)
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::SetSendCacheManager] ConnectID = %d, m_pBlockMessage is NULL.\n", GetConnectID()));
    }
}

void CProConnectHandle::SetPacketParseInfoID(uint32 u4PacketParseInfoID)
{
    m_u4PacketParseInfoID = u4PacketParseInfoID;
}

uint32 CProConnectHandle::GetPacketParseInfoID()
{
    return m_u4PacketParseInfoID;
}

//***************************************************************************
CProConnectManager::CProConnectManager(void):m_mutex(), m_cond(m_mutex), m_u4SendQueuePutTime(0)
{
    m_u4TimeCheckID      = 0;
    m_szError[0]         = '\0';
    m_blRun              = false;

    m_u4TimeConnect      = 0;
    m_u4TimeDisConnect   = 0;

    m_tvCheckConnect     = ACE_OS::gettimeofday();

    m_SendMessagePool.Init();
}

CProConnectManager::~CProConnectManager(void)
{
    CloseAll();
}

void CProConnectManager::CloseAll()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);

    if(m_blRun)
    {
        if (false == this->CloseMsgQueue())
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManager::CloseAll]CloseMsgQueue fail.\n"));
        }
    }
    else
    {
        msg_queue()->deactivate();
    }

    if (false == KillTimer())
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManager::CloseAll]KillTimer fail.\n"));
    }

    vector<CProConnectHandle*> vecCloseConnectHandler;
    m_objHashConnectList.Get_All_Used(vecCloseConnectHandler);

    //��ʼ�ر���������
    for(int i = 0; i < (int)vecCloseConnectHandler.size(); i++)
    {
        CProConnectHandle* pConnectHandler = vecCloseConnectHandler[i];
        pConnectHandler->Close();
    }

    //ɾ��hash���ռ�
    m_objHashConnectList.Close();

    //ɾ���������
    m_SendCacheManager.Close();
}

bool CProConnectManager::Close(uint32 u4ConnectID)
{
    //�ͻ��˹ر�
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close](%d)Begin.\n", u4ConnectID));

    //��ʱ�����������
    char szConnectID[10] = { '\0' };
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);

    //���ӹرգ����ʱ������
    if (false == DelConnectTimeWheel(m_objHashConnectList.Get_Hash_Box_Data(szConnectID)))
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close]DelConnectTimeWheel ConnectID=%d fail.\n", u4ConnectID));
    }
    else
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close]DelConnectTimeWheel ConnectID=%d.\n", u4ConnectID));
    }

    m_objHashConnectList.Del_Hash_Data(szConnectID);
    m_u4TimeDisConnect++;

    //���շ����ڴ��
    m_SendCacheManager.FreeCacheData(u4ConnectID);

    //��������ͳ�ƹ���
    App_ConnectAccount::instance()->AddDisConnect();

    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close](%d)End.\n", u4ConnectID));
    return true;
}

bool CProConnectManager::CloseConnect(uint32 u4ConnectID, EM_Client_Close_status emStatus)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    //�������ر�
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(pConnectHandler != NULL)
    {
        //��ʱ�����������
        if (false == DelConnectTimeWheel(m_objHashConnectList.Get_Hash_Box_Data(szConnectID)))
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close]DelConnectTimeWheel ConnectID=%d fail.\n", u4ConnectID));
        }

        pConnectHandler->ServerClose(emStatus);

        m_u4TimeDisConnect++;

        //���շ����ڴ��
        m_SendCacheManager.FreeCacheData(u4ConnectID);

        //��������ͳ�ƹ���
        if (false == App_ConnectAccount::instance()->AddDisConnect())
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close]AddDisConnect ConnectID=%d fail.\n", u4ConnectID));
        }

        if (-1 == m_objHashConnectList.Del_Hash_Data(szConnectID))
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectHandle::Close]Del_Hash_Data ConnectID=%d fail.\n", u4ConnectID));
        }

        return true;
    }
    else
    {
        sprintf_safe(m_szError, MAX_BUFF_500, "[CProConnectManager::CloseConnect] ConnectID[%d] is not find.", u4ConnectID);
        return true;
    }
}

bool CProConnectManager::CloseConnect_By_Queue(uint32 u4ConnectID)
{
    //���뷢�Ͷ���
    _SendMessage* pSendMessage = m_SendMessagePool.Create();

    if (NULL == pSendMessage)
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectManager::CloseConnect_By_Queue] new _SendMessage is error.\n"));
        return false;
    }

    ACE_Message_Block* mb = pSendMessage->GetQueueMessage();

    if (NULL != mb)
    {
        //��װ�ر�����ָ��
        pSendMessage->m_u4ConnectID = u4ConnectID;
        pSendMessage->m_pBuffPacket = NULL;
        pSendMessage->m_nEvents     = 0;
        pSendMessage->m_u2CommandID = 0;
        pSendMessage->m_u1SendState = 0;
        pSendMessage->m_blDelete    = false;
        pSendMessage->m_nMessageID  = 0;
        pSendMessage->m_u1Type      = 1;
        pSendMessage->m_tvSend      = ACE_OS::gettimeofday();

        //�ж϶����Ƿ����Ѿ����
        int nQueueCount = (int)msg_queue()->message_count();

        if (nQueueCount >= (int)MAX_MSG_THREADQUEUE)
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectManager::CloseConnect_By_Queue] Queue is Full nQueueCount = [%d].\n", nQueueCount));
            m_SendMessagePool.Delete(pSendMessage);
            return false;
        }

        ACE_Time_Value xtime = ACE_OS::gettimeofday() + ACE_Time_Value(0, m_u4SendQueuePutTime);

        if (this->putq(mb, &xtime) == -1)
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectManager::CloseConnect_By_Queue] Queue putq  error nQueueCount = [%d] errno = [%d].\n", nQueueCount, errno));
            m_SendMessagePool.Delete(pSendMessage);
            return false;
        }
    }
    else
    {
        OUR_DEBUG((LM_ERROR, "[CMessageService::CloseConnect_By_Queue] mb new error.\n"));
        m_SendMessagePool.Delete(pSendMessage);
        return false;
    }

    return true;
}

bool CProConnectManager::AddConnect(uint32 u4ConnectID, CProConnectHandle* pConnectHandler)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::AddConnect](%d)Begin.\n", u4ConnectID));

    if(pConnectHandler == NULL)
    {
        sprintf_safe(m_szError, MAX_BUFF_500, "[CProConnectManager::AddConnect] pConnectHandler is NULL.");
        return false;
    }

    //m_ThreadWriteLock.acquire();
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pCurrConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pCurrConnectHandler)
    {
        sprintf_safe(m_szError, MAX_BUFF_500, "[CProConnectManager::AddConnect] ConnectID[%d] is exist.", u4ConnectID);
        //m_ThreadWriteLock.release();
        return false;
    }

    pConnectHandler->SetConnectID(u4ConnectID);
    pConnectHandler->SetSendCacheManager((ISendCacheManager* )&m_SendCacheManager);

    //����Hash����
    m_objHashConnectList.Add_Hash_Data(szConnectID, pConnectHandler);
    m_u4TimeConnect++;

    //��������ͳ�ƹ���
    App_ConnectAccount::instance()->AddConnect();

    //����ʱ������
    if (false == SetConnectTimeWheel(pConnectHandler))
    {
        OUR_DEBUG((LM_INFO, "[CProConnectHandle::AddConnect](%d)SetConnectTimeWheel is fail", u4ConnectID));
    }

    //m_ThreadWriteLock.release();

    //OUR_DEBUG((LM_ERROR, "[CProConnectHandle::AddConnect](%d)End.\n", u4ConnectID));

    return true;
}

bool CProConnectManager::SetConnectTimeWheel(CProConnectHandle* pConnectHandler)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    bool bAddResult = m_TimeWheelLink.Add_TimeWheel_Object(pConnectHandler);

    if(!bAddResult)
    {
        OUR_DEBUG((LM_ERROR,"[CProConnectManager::SetConnectTimeWheel]Fail to set pConnectHandler(0x%08x).\n", pConnectHandler));
    }

    return bAddResult;
}

bool CProConnectManager::DelConnectTimeWheel(CProConnectHandle* pConnectHandler)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    m_TimeWheelLink.Del_TimeWheel_Object(pConnectHandler);
    return true;
}

bool CProConnectManager::SendMessage(uint32 u4ConnectID, IBuffPacket* pBuffPacket, uint16 u2CommandID, uint8 u1SendState, uint8 u1SendType, ACE_Time_Value& tvSendBegin, bool blDelete, int nMessageID)
{
    m_ThreadWriteLock.acquire();
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);
    m_ThreadWriteLock.release();

    //OUR_DEBUG((LM_ERROR,"[CProConnectManager::SendMessage] (%d) Send Begin 1(0x%08x).\n", u4ConnectID, pConnectHandler));

    uint32 u4CommandSize = pBuffPacket->GetPacketLen();

    if(NULL != pConnectHandler)
    {
        uint32 u4PacketSize  = 0;

        if (false == pConnectHandler->SendMessage(u2CommandID, pBuffPacket, u1SendState, u1SendType, u4PacketSize, blDelete, nMessageID))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManager::SendMessage]ConnectID=%d, CommandID=%d, SendMessage error.\n", u4ConnectID, u2CommandID));
        }

        if (false == m_CommandAccount.SaveCommandData(u2CommandID, (uint64)0, PACKET_TCP, u4PacketSize, u4CommandSize, COMMAND_TYPE_OUT))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManager::SendMessage]ConnectID=%d, CommandID=%d, SaveCommandData error.\n", u4ConnectID, u2CommandID));
        }

        return true;
    }
    else
    {
        sprintf_safe(m_szError, MAX_BUFF_500, "[CProConnectManager::SendMessage] ConnectID[%d] is not find.", u4ConnectID);
        //������Ӳ������ˣ������ﷵ��ʧ�ܣ��ص���ҵ���߼�ȥ����
        ACE_Message_Block* pSendMessage = App_MessageBlockManager::instance()->Create(pBuffPacket->GetPacketLen());
        memcpy_safe((char* )pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), (char* )pSendMessage->wr_ptr(), pBuffPacket->GetPacketLen());
        pSendMessage->wr_ptr(pBuffPacket->GetPacketLen());
        ACE_Time_Value tvNow = ACE_OS::gettimeofday();

        if (false == App_MakePacket::instance()->PutSendErrorMessage(0, pSendMessage, tvNow))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManager::SendMessage]ConnectID=%d, CommandID=%d, PutSendErrorMessage error.\n", u4ConnectID, u2CommandID));
        }

        if(true == blDelete)
        {
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
        }

        return true;
    }

    return true;
}

bool CProConnectManager::PostMessage(uint32 u4ConnectID, IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    //OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage]BEGIN.\n"));
    if(NULL == pBuffPacket)
    {
        OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] pBuffPacket is NULL.\n"));
        return false;
    }

    //���뷢�Ͷ���
    _SendMessage* pSendMessage = m_SendMessagePool.Create();

    if(NULL == pSendMessage)
    {
        OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] new _SendMessage is error.\n"));

        if(blDelete == true)
        {
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
        }

        return false;
    }

    ACE_Message_Block* mb = pSendMessage->GetQueueMessage();

    if(NULL != mb)
    {
        pSendMessage->m_u4ConnectID = u4ConnectID;
        pSendMessage->m_pBuffPacket = pBuffPacket;
        pSendMessage->m_nEvents     = u1SendType;
        pSendMessage->m_u2CommandID = u2CommandID;
        pSendMessage->m_u1SendState = u1SendState;
        pSendMessage->m_blDelete    = blDelete;
        pSendMessage->m_nMessageID  = nMessageID;
        pSendMessage->m_u1Type      = 0;
        pSendMessage->m_tvSend      = ACE_OS::gettimeofday();

        //�ж϶����Ƿ����Ѿ����
        int nQueueCount = (int)msg_queue()->message_count();

        if(nQueueCount >= (int)MAX_MSG_THREADQUEUE)
        {
            OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] Queue is Full nQueueCount = [%d].\n", nQueueCount));

            if(blDelete == true)
            {
                App_BuffPacketManager::instance()->Delete(pBuffPacket);
            }

            m_SendMessagePool.Delete(pSendMessage);
            return false;
        }

        ACE_Time_Value xtime = ACE_OS::gettimeofday() + ACE_Time_Value(0, m_u4SendQueuePutTime);

        if(this->putq(mb, &xtime) == -1)
        {
            OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] Queue putq  error nQueueCount = [%d] errno = [%d].\n", nQueueCount, errno));

            if(blDelete == true)
            {
                App_BuffPacketManager::instance()->Delete(pBuffPacket);
            }

            m_SendMessagePool.Delete(pSendMessage);
            return false;
        }
    }
    else
    {
        OUR_DEBUG((LM_ERROR,"[CMessageService::PutMessage] mb new error.\n"));

        if(blDelete == true)
        {
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
        }

        m_SendMessagePool.Delete(pSendMessage);
        return false;
    }

    return true;
}

const char* CProConnectManager::GetError()
{
    return m_szError;
}

bool CProConnectManager::StartTimer()
{
    //���������߳�
    if(0 != open())
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectManager::StartTimer]Open() is error.\n"));
        return false;
    }

    //���ⶨʱ���ظ�����
    if (false == KillTimer())
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectManager::StartTimer]KillTimer() is error.\n"));
        return false;
    }

    OUR_DEBUG((LM_ERROR, "CProConnectManager::StartTimer()-->begin....\n"));

    //������ӷ��ʹ�����
    uint16 u2CheckAlive = App_MainConfig::instance()->GetCheckAliveTime();
    long lTimeCheckID = App_TimerManager::instance()->schedule(this, (void*)NULL, ACE_OS::gettimeofday() + ACE_Time_Value(u2CheckAlive), ACE_Time_Value(u2CheckAlive));

    if(-1 == lTimeCheckID)
    {
        OUR_DEBUG((LM_ERROR, "CProConnectManager::StartTimer()--> Start thread m_u4TimeCheckID error.\n"));
        return false;
    }
    else
    {
        m_u4TimeCheckID = (uint32)lTimeCheckID;
        OUR_DEBUG((LM_ERROR, "CProConnectManager::StartTimer()--> Start thread time OK.\n"));
        return true;
    }
}

bool CProConnectManager::KillTimer()
{
    if(m_u4TimeCheckID > 0)
    {
        App_TimerManager::instance()->cancel(m_u4TimeCheckID);
        m_u4TimeCheckID = 0;
    }

    return true;
}

int CProConnectManager::handle_write_file_stream(uint32 u4ConnectID, const char* pData, uint32 u4Size, uint8 u1ParseID)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    char szConnectID[10] = { '\0' };
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if (NULL != pConnectHandler)
    {
        return pConnectHandler->handle_write_file_stream(pData, u4Size, u1ParseID);
    }
    else
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManager::handle_write_file_stream]m_objHashConnectList not find.\n"));
        return -1;
    }
}

int CProConnectManager::handle_timeout(const ACE_Time_Value& tv, const void* arg)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    ACE_Time_Value tvNow = ACE_OS::gettimeofday();

    //ת��ʱ������
    m_TimeWheelLink.Tick();

    //�ж��Ƿ�Ӧ�ü�¼������־
    ACE_Time_Value tvInterval(tvNow - m_tvCheckConnect);

    if(tvInterval.sec() >= MAX_MSG_HANDLETIME)
    {
        AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "[CProConnectManager]CurrConnectCount = %d,TimeInterval=%d, TimeConnect=%d, TimeDisConnect=%d.",
                                            GetCount(), MAX_MSG_HANDLETIME, m_u4TimeConnect, m_u4TimeDisConnect);

        //���õ�λʱ���������ͶϿ�������
        m_u4TimeConnect    = 0;
        m_u4TimeDisConnect = 0;
        m_tvCheckConnect   = tvNow;
    }

    //������������Ƿ�Խ��ط�ֵ
    if(App_MainConfig::instance()->GetConnectAlert()->m_u4ConnectAlert > 0)
    {
        if(GetCount() > (int)App_MainConfig::instance()->GetConnectAlert()->m_u4ConnectAlert)
        {
            AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
                                                   App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
                                                   (char* )"Alert",
                                                   "[CProConnectManager]active ConnectCount is more than limit(%d > %d).",
                                                   GetCount(),
                                                   App_MainConfig::instance()->GetConnectAlert()->m_u4ConnectAlert);
        }
    }

    //��ⵥλʱ���������Ƿ�Խ��ֵ
    int nCheckRet = App_ConnectAccount::instance()->CheckConnectCount();

    if(nCheckRet == 1)
    {
        AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
                                               App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
                                               "Alert",
                                               "[CProConnectManager]CheckConnectCount is more than limit(%d > %d).",
                                               App_ConnectAccount::instance()->GetCurrConnect(),
                                               App_ConnectAccount::instance()->GetConnectMax());
    }
    else if(nCheckRet == 2)
    {
        AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
                                               App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
                                               "Alert",
                                               "[CProConnectManager]CheckConnectCount is little than limit(%d < %d).",
                                               App_ConnectAccount::instance()->GetCurrConnect(),
                                               App_ConnectAccount::instance()->Get4ConnectMin());
    }

    //��ⵥλʱ�����ӶϿ����Ƿ�Խ��ֵ
    nCheckRet = App_ConnectAccount::instance()->CheckDisConnectCount();

    if(nCheckRet == 1)
    {
        AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
                                               App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
                                               "Alert",
                                               "[CProConnectManager]CheckDisConnectCount is more than limit(%d > %d).",
                                               App_ConnectAccount::instance()->GetCurrConnect(),
                                               App_ConnectAccount::instance()->GetDisConnectMax());
    }
    else if(nCheckRet == 2)
    {
        AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
                                               App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
                                               "Alert",
                                               "[CProConnectManager]CheckDisConnectCount is little than limit(%d < %d).",
                                               App_ConnectAccount::instance()->GetCurrConnect(),
                                               App_ConnectAccount::instance()->GetDisConnectMin());
    }

    return 0;
}

int CProConnectManager::GetCount()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    return (int)m_objHashConnectList.Get_Used_Count();
}

int CProConnectManager::open(void* args)
{
    m_blRun = true;
    msg_queue()->high_water_mark(MAX_MSG_MASK);
    msg_queue()->low_water_mark(MAX_MSG_MASK);

    OUR_DEBUG((LM_INFO,"[CProConnectManager::open] m_u4HighMask = [%d] m_u4LowMask = [%d]\n", MAX_MSG_MASK, MAX_MSG_MASK));

    if(activate(THREAD_PARAM, MAX_MSG_THREADCOUNT) == -1)
    {
        OUR_DEBUG((LM_ERROR, "[CProConnectManager::open] activate error ThreadCount = [%d].", MAX_MSG_THREADCOUNT));
        m_blRun = false;
        return -1;
    }

    m_u4SendQueuePutTime = App_MainConfig::instance()->GetSendQueuePutTime() * 1000;

    resume();

    return 0;
}

int CProConnectManager::svc (void)
{
    ACE_Time_Value xtime;

    while(true)
    {
        ACE_Message_Block* mb = NULL;
        ACE_OS::last_error(0);

        //xtime = ACE_OS::gettimeofday() + ACE_Time_Value(0, MAX_MSG_PUTTIMEOUT);
        if(getq(mb, 0) == -1)
        {
            OUR_DEBUG((LM_INFO,"[CProConnectManager::svc] getq is error[%d]!\n", ACE_OS::last_error()));
            m_blRun = false;
            break;
        }
        else
        {
            if (mb == NULL)
            {
                continue;
            }

            if ((0 == mb->size ()) && (mb->msg_type () == ACE_Message_Block::MB_STOP))
            {
                m_mutex.acquire();
                mb->release ();
                this->msg_queue ()->deactivate ();
                m_cond.signal();
                m_mutex.release();
                break;
            }

            _SendMessage* msg = *((_SendMessage**)mb->base());

            if (!msg)
            {
                continue;
            }

            if (0 == msg->m_u1Type)
            {
                //������������
                if (false == SendMessage(msg->m_u4ConnectID, msg->m_pBuffPacket, msg->m_u2CommandID, msg->m_u1SendState, msg->m_nEvents, msg->m_tvSend, msg->m_blDelete, msg->m_nMessageID))
                {
                    OUR_DEBUG((LM_INFO, "[CProConnectManager::svc]ConnectID=%d, m_u2CommandID=%d, SendMessage error.\n", msg->m_u4ConnectID, msg->m_u2CommandID));
                }
            }
            else if (1 == msg->m_u1Type)
            {
                //�������ӷ����������ر�
                if (false == CloseConnect(msg->m_u4ConnectID, CLIENT_CLOSE_IMMEDIATLY))
                {
                    OUR_DEBUG((LM_INFO, "[CProConnectManager::svc]ConnectID=%d CloseConnect error.\n", msg->m_u4ConnectID));
                }
            }

            m_SendMessagePool.Delete(msg);
        }
    }

    OUR_DEBUG((LM_INFO,"[CProConnectManager::svc] svc finish!\n"));
    return 0;
}

int CProConnectManager::close(u_long)
{
    m_blRun = false;
    OUR_DEBUG((LM_INFO,"[CProConnectManager::close] close().\n"));
    return 0;
}

void CProConnectManager::GetConnectInfo(vecClientConnectInfo& VecClientConnectInfo)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    vector<CProConnectHandle*> vecProConnectHandle;
    m_objHashConnectList.Get_All_Used(vecProConnectHandle);

    for(int i = 0; i < (int)vecProConnectHandle.size(); i++)
    {
        CProConnectHandle* pConnectHandler = vecProConnectHandle[i];

        if(pConnectHandler != NULL)
        {
            VecClientConnectInfo.push_back(pConnectHandler->GetClientInfo());
        }
    }
}

void CProConnectManager::SetRecvQueueTimeCost(uint32 u4ConnectID, uint32 u4TimeCost)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pConnectHandler)
    {
        pConnectHandler->SetRecvQueueTimeCost(u4TimeCost);
    }
}

_ClientIPInfo CProConnectManager::GetClientIPInfo(uint32 u4ConnectID)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pConnectHandler)
    {
        return pConnectHandler->GetClientIPInfo();
    }
    else
    {
        _ClientIPInfo ClientIPInfo;
        return ClientIPInfo;
    }
}

_ClientIPInfo CProConnectManager::GetLocalIPInfo(uint32 u4ConnectID)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pConnectHandler)
    {
        return pConnectHandler->GetLocalIPInfo();
    }
    else
    {
        _ClientIPInfo ClientIPInfo;
        return ClientIPInfo;
    }
}

bool CProConnectManager::PostMessageAll( IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    m_ThreadWriteLock.acquire();
    vector<CProConnectHandle*> objveCProConnectManager;
    m_objHashConnectList.Get_All_Used(objveCProConnectManager);
    m_ThreadWriteLock.release();

    uint32 u4ConnectID = 0;

    for(uint32 i = 0; i < (uint32)objveCProConnectManager.size(); i++)
    {
        IBuffPacket* pCurrBuffPacket = App_BuffPacketManager::instance()->Create();

        if(NULL == pCurrBuffPacket)
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManager::PostMessage]pCurrBuffPacket is NULL.\n"));
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
            return false;
        }

        pCurrBuffPacket->WriteStream(pBuffPacket->GetData(), pBuffPacket->GetPacketLen());

        //���뷢�Ͷ���
        _SendMessage* pSendMessage = m_SendMessagePool.Create();

        if(NULL == pSendMessage)
        {
            OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] new _SendMessage is error.\n"));
            App_BuffPacketManager::instance()->Delete(pBuffPacket);
            return false;
        }

        ACE_Message_Block* mb = pSendMessage->GetQueueMessage();

        if(NULL != mb)
        {
            pSendMessage->m_u4ConnectID = objveCProConnectManager[i]->GetConnectID();
            pSendMessage->m_pBuffPacket = pCurrBuffPacket;
            pSendMessage->m_nEvents     = u1SendType;
            pSendMessage->m_u2CommandID = u2CommandID;
            pSendMessage->m_u1SendState = u1SendState;
            pSendMessage->m_blDelete    = blDelete;
            pSendMessage->m_nMessageID  = nMessageID;
            pSendMessage->m_u1Type      = 0;
            pSendMessage->m_tvSend      = ACE_OS::gettimeofday();

            //�ж϶����Ƿ����Ѿ����
            int nQueueCount = (int)msg_queue()->message_count();

            if(nQueueCount >= (int)MAX_MSG_THREADQUEUE)
            {
                OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] Queue is Full nQueueCount = [%d].\n", nQueueCount));

                if(blDelete == true)
                {
                    App_BuffPacketManager::instance()->Delete(pBuffPacket);
                }

                m_SendMessagePool.Delete(pSendMessage);
                return false;
            }

            ACE_Time_Value xtime = ACE_OS::gettimeofday() + ACE_Time_Value(0, MAX_MSG_PUTTIMEOUT);

            if(this->putq(mb, &xtime) == -1)
            {
                OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] Queue putq  error nQueueCount = [%d] errno = [%d].\n", nQueueCount, errno));

                if(blDelete == true)
                {
                    App_BuffPacketManager::instance()->Delete(pBuffPacket);
                }

                m_SendMessagePool.Delete(pSendMessage);
                return false;
            }
        }
        else
        {
            OUR_DEBUG((LM_ERROR,"[CProConnectManager::PutMessage] mb new error.\n"));

            if(blDelete == true)
            {
                App_BuffPacketManager::instance()->Delete(pBuffPacket);
            }

            m_SendMessagePool.Delete(pSendMessage);
            return false;
        }
    }

    return true;
}

bool CProConnectManager::SetConnectName( uint32 u4ConnectID, const char* pName )
{
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pConnectHandler)
    {
        pConnectHandler->SetConnectName(pName);
        return true;
    }
    else
    {
        return false;
    }
}

bool CProConnectManager::SetIsLog( uint32 u4ConnectID, bool blIsLog )
{
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pConnectHandler)
    {
        pConnectHandler->SetIsLog(blIsLog);
        return true;
    }
    else
    {
        return false;
    }
}

void CProConnectManager::GetClientNameInfo(const char* pName, vecClientNameInfo& objClientNameInfo)
{
    vector<CProConnectHandle*> vecProConnectHandle;
    m_objHashConnectList.Get_All_Used(vecProConnectHandle);

    for(int i = 0; i < (int)vecProConnectHandle.size(); i++)
    {
        CProConnectHandle* pConnectHandler = vecProConnectHandle[i];

        if(NULL != pConnectHandler && ACE_OS::strcmp(pConnectHandler->GetConnectName(), pName) == 0)
        {
            _ClientNameInfo ClientNameInfo;
            ClientNameInfo.m_nConnectID = (int)pConnectHandler->GetConnectID();
            sprintf_safe(ClientNameInfo.m_szName, MAX_BUFF_100, "%s", pConnectHandler->GetConnectName());
            sprintf_safe(ClientNameInfo.m_szClientIP, MAX_BUFF_50, "%s", pConnectHandler->GetClientIPInfo().m_szClientIP);
            ClientNameInfo.m_nPort =  pConnectHandler->GetClientIPInfo().m_nPort;

            if(pConnectHandler->GetIsLog() == true)
            {
                ClientNameInfo.m_nLog = 1;
            }
            else
            {
                ClientNameInfo.m_nLog = 0;
            }

            objClientNameInfo.push_back(ClientNameInfo);
        }
    }
}

void CProConnectManager::Init(uint16 u2Index)
{
    //�����̳߳�ʼ��ͳ��ģ�������
    char szName[MAX_BUFF_50] = {'\0'};
    sprintf_safe(szName, MAX_BUFF_50, "�����߳�(%d)", u2Index);
    m_CommandAccount.InitName(szName, App_MainConfig::instance()->GetMaxCommandCount());

    //��ʼ��ͳ��ģ�鹦��
    m_CommandAccount.Init(App_MainConfig::instance()->GetCommandAccount(),
                          App_MainConfig::instance()->GetCommandFlow(),
                          App_MainConfig::instance()->GetPacketTimeOut());

    //��ʼ�����ͻ����
    m_SendCacheManager.Init(App_MainConfig::instance()->GetBlockCount(), App_MainConfig::instance()->GetBlockSize());

    //��ʼ��Hash��
    uint16 u2PoolSize = App_MainConfig::instance()->GetMaxHandlerCount();
    m_objHashConnectList.Init((int)u2PoolSize);

    //��ʼ��ʱ������
    m_TimeWheelLink.Init(App_MainConfig::instance()->GetMaxConnectTime(), App_MainConfig::instance()->GetCheckAliveTime(), (int)u2PoolSize, CProConnectManager::TimeWheel_Timeout_Callback, (void* )this);
}

void CProConnectManager::TimeWheel_Timeout_Callback(void* pArgsContext, vector<CProConnectHandle*> vecProConnectHandle)
{
    OUR_DEBUG((LM_INFO, "[CProConnectManager::TimeWheel_Timeout_Callback]Timeout Count(%d).\n", vecProConnectHandle.size()));

    for (int i = 0; i < (int)vecProConnectHandle.size(); i++)
    {
        //�Ͽ���ʱ������
        if (NULL != vecProConnectHandle[i])
        {
            CProConnectManager* pManager = reinterpret_cast<CProConnectManager*>(pArgsContext);
            OUR_DEBUG((LM_INFO, "[CProConnectManager::TimeWheel_Timeout_Callback]ConnectID(%d).\n", vecProConnectHandle[i]->GetConnectID()));

            if (NULL != pManager)
            {
                if (false == pManager->CloseConnect_By_Queue(vecProConnectHandle[i]->GetConnectID()))
                {
                    OUR_DEBUG((LM_INFO, "[CProConnectManager::TimeWheel_Timeout_Callback]CloseConnect_By_Queue error.\n"));
                }
            }
        }
    }
}

_CommandData* CProConnectManager::GetCommandData(uint16 u2CommandID)
{
    return m_CommandAccount.GetCommandData(u2CommandID);
}

uint32 CProConnectManager::GetCommandFlowAccount()
{
    return m_CommandAccount.GetFlowOut();
}

EM_Client_Connect_status CProConnectManager::GetConnectState(uint32 u4ConnectID)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
    char szConnectID[10] = {'\0'};
    sprintf_safe(szConnectID, 10, "%d", u4ConnectID);
    CProConnectHandle* pConnectHandler = m_objHashConnectList.Get_Hash_Box_Data(szConnectID);

    if(NULL != pConnectHandler)
    {
        return CLIENT_CONNECT_EXIST;
    }
    else
    {
        return CLIENT_CONNECT_NO_EXIST;
    }
}

CSendCacheManager* CProConnectManager::GetSendCacheManager()
{
    return &m_SendCacheManager;
}

int CProConnectManager::CloseMsgQueue()
{
    // We can choose to process the message or to differ it into the message
    // queue, and process them into the svc() method. Chose the last option.
    int retval;
    ACE_Message_Block* mblk = 0;
    ACE_NEW_RETURN(mblk,ACE_Message_Block (0, ACE_Message_Block::MB_STOP),-1);

    // If queue is full, flush it before block in while
    if (msg_queue ()->is_full())
    {
        if ((retval=msg_queue ()->flush()) == -1)
        {
            OUR_DEBUG((LM_ERROR, "[CProConnectManager::CloseMsgQueue]put error flushing queue\n"));
            return -1;
        }
    }

    m_mutex.acquire();

    while ((retval = putq (mblk)) == -1)
    {
        if (msg_queue ()->state () != ACE_Message_Queue_Base::PULSED)
        {
            OUR_DEBUG((LM_ERROR,ACE_TEXT("[CProConnectManager::CloseMsgQueue]put Queue not activated.\n")));
            break;
        }
    }

    m_cond.wait();
    m_mutex.release();

    return retval;
}

//*********************************************************************************

CProConnectHandlerPool::CProConnectHandlerPool(void)
{
    //ConnectID��������1��ʼ
    m_u4CurrMaxCount = 1;
}

CProConnectHandlerPool::~CProConnectHandlerPool(void)
{
    Close();
}

void CProConnectHandlerPool::Init(int nObjcetCount)
{
    Close();

    //��ʼ��HashTable
    m_objHashHandleList.Init((int)nObjcetCount);

    for(int i = 0; i < nObjcetCount; i++)
    {
        CProConnectHandle* pHandler = new CProConnectHandle();

        if(NULL != pHandler)
        {
            pHandler->Init(m_u4CurrMaxCount);

            //��ID��Handlerָ��Ĺ�ϵ����hashTable
            char szHandlerID[10] = {'\0'};
            sprintf_safe(szHandlerID, 10, "%d", m_u4CurrMaxCount);
            int nHashPos = m_objHashHandleList.Add_Hash_Data(szHandlerID, pHandler);

            if(-1 != nHashPos)
            {
                pHandler->Init(i);
                pHandler->SetHashID(i);
            }

            m_u4CurrMaxCount++;
        }
    }
}

void CProConnectHandlerPool::Close()
{
    //���������Ѵ��ڵ�ָ��
    vector<CProConnectHandle*> vecProConnectHandle;
    m_objHashHandleList.Get_All_Used(vecProConnectHandle);

    for(int i = 0; i < (int)vecProConnectHandle.size(); i++)
    {
        CProConnectHandle* pHandler = vecProConnectHandle[i];
        SAFE_DELETE(pHandler);
    }

    m_u4CurrMaxCount  = 1;
}

int CProConnectHandlerPool::GetUsedCount()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

    return m_objHashHandleList.Get_Count() - m_objHashHandleList.Get_Used_Count();
}

int CProConnectHandlerPool::GetFreeCount()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

    return (int)m_objHashHandleList.Get_Used_Count();
}

CProConnectHandle* CProConnectHandlerPool::Create()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

    //��Hash���е���һ����ʹ�õ�����
    CProConnectHandle* pHandler = m_objHashHandleList.Pop();
    //OUR_DEBUG((LM_INFO, "[CProConnectHandlerPool::Create]pHandler(0x%08x)\n", pHandler));

    //û�ҵ������
    return pHandler;
}

bool CProConnectHandlerPool::Delete(CProConnectHandle* pObject)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

    if(NULL == pObject)
    {
        return false;
    }

    char szHandlerID[10] = {'\0'};
    sprintf_safe(szHandlerID, 10, "%d", pObject->GetHandlerID());
    //int nPos = m_objHashHandleList.Add_Hash_Data(szHandlerID, pObject);
    //������Ϊ�ڴ��ǹ̶��ģ�ֱ��д��Hashԭ��λ��
    bool blState = m_objHashHandleList.Push(szHandlerID, pObject);

    if(false == blState)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectHandlerPool::Delete]szHandlerID=%s(0x%08x).\n", szHandlerID, pObject));
        //m_objHashHandleList.Add_Hash_Data(szHandlerID, pObject);
    }
    else
    {
        //OUR_DEBUG((LM_INFO, "[CProConnectHandlerPool::Delete]szHandlerID=%s(0x%08x) nPos=%d.\n", szHandlerID, pObject, nPos));
    }

    return true;
}

//==============================================================
CProConnectManagerGroup::CProConnectManagerGroup()
{
    m_objProConnnectManagerList = NULL;
    m_u4CurrMaxCount            = 0;
    m_u2ThreadQueueCount        = SENDQUEUECOUNT;
}

CProConnectManagerGroup::~CProConnectManagerGroup()
{
    OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::~CProConnectManagerGroup].\n"));
    Close();
}

void CProConnectManagerGroup::Close()
{
    if(NULL != m_objProConnnectManagerList)
    {
        for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
        {
            CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];
            SAFE_DELETE(pConnectManager);
        }
    }

    SAFE_DELETE_ARRAY(m_objProConnnectManagerList);
    m_u2ThreadQueueCount = 0;
}

void CProConnectManagerGroup::Init(uint16 u2SendQueueCount)
{
    Close();

    m_objProConnnectManagerList = new CProConnectManager*[u2SendQueueCount];
    memset(m_objProConnnectManagerList, 0, sizeof(CProConnectManager*)*u2SendQueueCount);

    for(int i = 0; i < u2SendQueueCount; i++)
    {
        CProConnectManager* pConnectManager = new CProConnectManager();

        if(NULL != pConnectManager)
        {
            //��ʼ��ͳ����
            pConnectManager->Init((uint16)i);

            //��������
            m_objProConnnectManagerList[i] = pConnectManager;
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::Init]Creat %d SendQueue OK.\n", i));
        }
    }

    m_u2ThreadQueueCount = u2SendQueueCount;
}

uint32 CProConnectManagerGroup::GetGroupIndex()
{
    //�������ӻ�����У��������������㷨��
    ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
    m_u4CurrMaxCount++;
    return m_u4CurrMaxCount;
}

bool CProConnectManagerGroup::AddConnect(CProConnectHandle* pConnectHandler)
{
    uint32 u4ConnectID = GetGroupIndex();

    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::AddConnect]No find send Queue object.\n"));
        return false;
    }

    //OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::Init]u4ConnectID=%d, u2ThreadIndex=%d.\n", u4ConnectID, u2ThreadIndex));

    return pConnectManager->AddConnect(u4ConnectID, pConnectHandler);
}

bool CProConnectManagerGroup::SetConnectTimeWheel(CProConnectHandle* pConnectHandler)
{
    uint32 u4ConnectID = pConnectHandler->GetConnectID();

    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if (NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::AddConnect]No find send Queue object.\n"));
        return false;
    }

    return pConnectManager->SetConnectTimeWheel(pConnectHandler);
}

bool CProConnectManagerGroup::DelConnectTimeWheel(CProConnectHandle* pConnectHandler)
{
    uint32 u4ConnectID = pConnectHandler->GetConnectID();

    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if (NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::AddConnect]No find send Queue object.\n"));
        return false;
    }

    return pConnectManager->DelConnectTimeWheel(pConnectHandler);
}

bool CProConnectManagerGroup::PostMessage(uint32 u4ConnectID, IBuffPacket*& pBuffPacket, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]No find send Queue object.\n"));
        return false;
    }

    //OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]u4ConnectID=%d, u2ThreadIndex=%d.\n", u4ConnectID, u2ThreadIndex));

    return pConnectManager->PostMessage(u4ConnectID, pBuffPacket, u1SendType, u2CommandID, u1SendState, blDelete, nMessageID);
}

bool CProConnectManagerGroup::PostMessage( uint32 u4ConnectID, const char*& pData, uint32 nDataLen, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]No find send Queue object.\n"));

        if(blDelete == true)
        {
            SAFE_DELETE_ARRAY(pData);
        }

        return false;
    }

    //OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]u4ConnectID=%d, u2ThreadIndex=%d.\n", u4ConnectID, u2ThreadIndex));
    IBuffPacket* pBuffPacket = App_BuffPacketManager::instance()->Create();

    if(NULL != pBuffPacket)
    {
        bool bWriteResult = pBuffPacket->WriteStream(pData, nDataLen);

        if(blDelete == true)
        {
            SAFE_DELETE_ARRAY(pData);
        }

        if(bWriteResult)
        {
            return pConnectManager->PostMessage(u4ConnectID, pBuffPacket, u1SendType, u2CommandID, u1SendState, true, nMessageID);
        }
        else
        {
            return false;
        }
    }
    else
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]pBuffPacket is NULL.\n"));

        if(blDelete == true)
        {
            SAFE_DELETE_ARRAY(pData);
        }

        return false;
    }

}

bool CProConnectManagerGroup::PostMessage( vector<uint32> vecConnectID, IBuffPacket*& pBuffPacket, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    uint32 u4ConnectID = 0;

    for(uint32 i = 0; i < (uint32)vecConnectID.size(); i++)
    {
        //�ж����е���һ���߳�������ȥ
        u4ConnectID = vecConnectID[i];
        uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

        CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

        if(NULL == pConnectManager)
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]No find send Queue object.\n"));
            continue;
        }

        //Ϊÿһ��Connect���÷��Ͷ������ݰ�
        IBuffPacket* pCurrBuffPacket = App_BuffPacketManager::instance()->Create();

        if(NULL == pCurrBuffPacket)
        {
            continue;
        }

        pCurrBuffPacket->WriteStream(pBuffPacket->GetData(), pBuffPacket->GetWriteLen());

        if (false == pConnectManager->PostMessage(u4ConnectID, pCurrBuffPacket, u1SendType, u2CommandID, u1SendState, true, nMessageID))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]PostMessage(%d) is error.\n", u4ConnectID));
        }
    }

    if(true == blDelete)
    {
        App_BuffPacketManager::instance()->Delete(pBuffPacket);
    }


    return true;
}

bool CProConnectManagerGroup::PostMessage( vector<uint32> vecConnectID, const char*& pData, uint32 nDataLen, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    uint32 u4ConnectID = 0;

    for(uint32 i = 0; i < (uint32)vecConnectID.size(); i++)
    {
        //�ж����е���һ���߳�������ȥ
        u4ConnectID = vecConnectID[i];
        uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

        CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

        if(NULL == pConnectManager)
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]No find send Queue object.\n"));
            continue;
        }

        //Ϊÿһ��Connect���÷��Ͷ������ݰ�
        IBuffPacket* pBuffPacket = App_BuffPacketManager::instance()->Create();

        if(NULL == pBuffPacket)
        {
            continue;
        }

        pBuffPacket->WriteStream(pData, nDataLen);

        if (false == PostMessage(u4ConnectID, pBuffPacket, u1SendType, u2CommandID, u1SendState, true, nMessageID))
        {
            OUR_DEBUG((LM_INFO, "[ CProConnectManagerGroup::PostMessage]PostMessage(%d) error.\n", u4ConnectID));
        }
    }

    if(true == blDelete)
    {
        SAFE_DELETE_ARRAY(pData);
    }

    return true;
}

bool CProConnectManagerGroup::CloseConnect(uint32 u4ConnectID)
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
        return false;
    }

    //ͨ����Ϣ����ȥʵ�ֹر�
    return pConnectManager->CloseConnect_By_Queue(u4ConnectID);
}

_ClientIPInfo CProConnectManagerGroup::GetClientIPInfo(uint32 u4ConnectID)
{
    _ClientIPInfo objClientIPInfo;
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
        return objClientIPInfo;
    }

    return pConnectManager->GetClientIPInfo(u4ConnectID);
}

_ClientIPInfo CProConnectManagerGroup::GetLocalIPInfo(uint32 u4ConnectID)
{
    _ClientIPInfo objClientIPInfo;
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::GetLocalIPInfo]No find send Queue object.\n"));
        return objClientIPInfo;
    }

    return pConnectManager->GetLocalIPInfo(u4ConnectID);
}


void CProConnectManagerGroup::GetConnectInfo(vecClientConnectInfo& VecClientConnectInfo)
{
    VecClientConnectInfo.clear();

    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            pConnectManager->GetConnectInfo(VecClientConnectInfo);
        }
    }
}

int CProConnectManagerGroup::GetCount()
{
    uint32 u4Count = 0;

    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            u4Count += pConnectManager->GetCount();
        }
    }

    return u4Count;
}

void CProConnectManagerGroup::CloseAll()
{
    uint32 u4Count = 0;

    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            pConnectManager->CloseAll();
        }
    }
}

bool CProConnectManagerGroup::StartTimer()
{
    uint32 u4Count = 0;

    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            if (false == pConnectManager->StartTimer())
            {
                OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::StartTimer]StartTimer error.\n"));
            }
        }
    }

    return true;
}

bool CProConnectManagerGroup::Close(uint32 u4ConnectID)
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
        return false;
    }

    return pConnectManager->Close(u4ConnectID);
}

const char* CProConnectManagerGroup::GetError()
{
    return (char* )"";
}

void CProConnectManagerGroup::SetRecvQueueTimeCost(uint32 u4ConnectID, uint32 u4TimeCost)
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
        return;
    }

    pConnectManager->SetRecvQueueTimeCost(u4ConnectID, u4TimeCost);
}

bool CProConnectManagerGroup::PostMessageAll( IBuffPacket*& pBuffPacket, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    //ȫ��Ⱥ��
    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL == pConnectManager)
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]No find send Queue object.\n"));
            continue;
        }

        if (false == pConnectManager->PostMessageAll(pBuffPacket, u1SendType, u2CommandID, u1SendState, false, nMessageID))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessageAll]PostMessageAll error.\n"));
        }
    }

    if(true == blDelete)
    {
        //�����˾�ɾ��
        App_BuffPacketManager::instance()->Delete(pBuffPacket);
    }

    return true;
}

bool CProConnectManagerGroup::PostMessageAll( const char*& pData, uint32 nDataLen, uint8 u1SendType, uint16 u2CommandID, uint8 u1SendState, bool blDelete, int nMessageID)
{
    IBuffPacket* pBuffPacket = App_BuffPacketManager::instance()->Create();

    if(NULL == pBuffPacket)
    {
        OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessageAll]pBuffPacket is NULL.\n"));

        if(blDelete == true)
        {
            SAFE_DELETE_ARRAY(pData);
        }

        return false;
    }
    else
    {
        pBuffPacket->WriteStream(pData, nDataLen);
    }

    //ȫ��Ⱥ��
    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL == pConnectManager)
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessage]No find send Queue object.\n"));
            continue;
        }

        if (false == pConnectManager->PostMessageAll(pBuffPacket, u1SendType, u2CommandID, u1SendState, false, nMessageID))
        {
            OUR_DEBUG((LM_INFO, "[CProConnectManagerGroup::PostMessageAll]PostMessageAll error.\n"));
        }
    }

    //�����˾�ɾ��
    App_BuffPacketManager::instance()->Delete(pBuffPacket);

    //�����˾�ɾ��
    if(true == blDelete)
    {
        SAFE_DELETE_ARRAY(pData);
    }

    return true;
}

bool CProConnectManagerGroup::SetConnectName( uint32 u4ConnectID, const char* pName )
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
        return false;
    }

    return pConnectManager->SetConnectName(u4ConnectID, pName);
}

bool CProConnectManagerGroup::SetIsLog( uint32 u4ConnectID, bool blIsLog )
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
        return false;
    }

    return pConnectManager->SetIsLog(u4ConnectID, blIsLog);
}

void CProConnectManagerGroup::GetClientNameInfo( const char* pName, vecClientNameInfo& objClientNameInfo )
{
    objClientNameInfo.clear();

    //ȫ������
    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            pConnectManager->GetClientNameInfo(pName, objClientNameInfo);
        }
    }
}

void CProConnectManagerGroup::GetCommandData(uint16 u2CommandID, _CommandData& objCommandData)
{
    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            _CommandData* pCommandData = pConnectManager->GetCommandData(u2CommandID);

            if(pCommandData != NULL)
            {
                objCommandData += (*pCommandData);
            }
        }
    }
}

void CProConnectManagerGroup::GetCommandFlowAccount(_CommandFlowAccount& objCommandFlowAccount)
{
    for(uint16 i = 0; i < m_u2ThreadQueueCount; i++)
    {
        CProConnectManager* pConnectManager = m_objProConnnectManagerList[i];

        if(NULL != pConnectManager)
        {
            uint32 u4FlowOut =  pConnectManager->GetCommandFlowAccount();
            objCommandFlowAccount.m_u4FlowOut += u4FlowOut;
        }
    }
}

EM_Client_Connect_status CProConnectManagerGroup::GetConnectState(uint32 u4ConnectID)
{
    //�ж����е���һ���߳�������ȥ
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if(NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
        return CLIENT_CONNECT_NO_EXIST;
    }

    return pConnectManager->GetConnectState(u4ConnectID);
}

int CProConnectManagerGroup::handle_write_file_stream(uint32 u4ConnectID, const char* pData, uint32 u4Size, uint8 u1ParseID)
{
    //�ļ���� ָ��һ���߳���
    uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

    CProConnectManager* pConnectManager = m_objProConnnectManagerList[u2ThreadIndex];

    if (NULL == pConnectManager)
    {
        OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::file_open]No find.\n"));
        return 0;
    }

    return pConnectManager->handle_write_file_stream(u4ConnectID, pData, u4Size, u1ParseID);
}