
// PurenessLineDlg.h : 头文件
//

#pragma once

#include "ChartViewer.h"

#define WM_USER_NOTIFY (WM_USER + 1)

#include <stdlib.h>
#include <time.h>
#include "afxwin.h"
#include <afxsock.h>

//服务器运行时信息
typedef struct _ServerRunInfo
{
	double m_dCPU;        //当前CPU占用率
	UINT64 m_nMemorySize; //当前内存占用率

	_ServerRunInfo()
	{
		m_dCPU        = 0.0f;
		m_nMemorySize = 0;
	}

	_ServerRunInfo& operator = (const _ServerRunInfo& ar)
	{
		this->m_dCPU        = ar.m_dCPU;
		this->m_nMemorySize = ar.m_nMemorySize;
		return *this;
	}
}ServerRunInfo, *PServerRunInfo;

//设置一个随机种子
inline void InitRandom()
{
	srand((int)time(NULL));
};

//从一个值域中获取一个随机值
inline int RandomValue(int nMin, int nMax)
{
	return  nMin + (int) (nMax * (rand() / (RAND_MAX + 1.0)));
};

#define TIMER_ID       1        //定时器ID
#define TIMER_INTERVAL 1000     //定时器间隔
#define MAX_DATA_COUNT 25       //数组最大上限

// CPurenessLineDlg 对话框
class CPurenessLineDlg : public CDialog
{
// 构造
public:
	CPurenessLineDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_PURENESSLINE_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg void OnViewPortChanged();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg LRESULT OnUserNotify(WPARAM /*wParam*/, LPARAM /*lParam*/);
	DECLARE_MESSAGE_MAP()

	CEdit m_txtServerIP;
	CEdit m_txtServerPort;

private:
	void shiftData(double *data, int len, double newValue);
	void drawChart();
	void InitView();
	static DWORD WINAPI GetServerStateInfo(void * p);
	BOOL StartUserThread() {
		m_bUserRunning = TRUE;
		// Create a manual-reset event object. The write thread sets this
		// object to the signaled state when it finishes writing to a 
		// shared buffer. 
		m_hQuitEvent = CreateEvent(
			NULL,               // default security attributes
			TRUE,               // manual-reset event
			FALSE,              // initial state is nonsignaled
			TEXT("__EVENT")  // object name
		);
		m_hUserThread = CreateThread(NULL, 0, &CPurenessLineDlg::GetServerStateInfo, this, 0, &m_dwUserThreadId);
		return (m_hUserThread && (m_hUserThread != INVALID_HANDLE_VALUE));
	}
	void StopUserThread() {
		if (m_bUserRunning)
		{
			m_bUserRunning = FALSE;
		}
		if (m_bUserConnected != NetworkConnectType::NCTYPE_DISCONNECT)
		{
			this->setUserConnected(NetworkConnectType::NCTYPE_DISCONNECT);
		}
		if (m_hUserThread && (m_hUserThread != INVALID_HANDLE_VALUE))
		{
			if (WaitForSingleObject(m_hUserThread, INFINITE) == WAIT_OBJECT_0)
			{
				;//
			}
			CloseHandle(m_hUserThread);
			m_hUserThread = 0;
		}
		m_dwUserThreadId = 0;
		if (m_hQuitEvent && (m_hQuitEvent != INVALID_HANDLE_VALUE))
		{
			CloseHandle(m_hQuitEvent);
			m_hQuitEvent = NULL;
		}
	}
public:
	VOID sendUserNotify() {
		PostMessage(WM_USER_NOTIFY);
	}
	BOOL IsAutoConnect() { 
		return (((CButton*)this->GetDlgItem(IDC_CHECK1))->GetCheck() == BST_CHECKED); 
	}
	BOOL InitConn() {
		//解析端口地址
		int nPort = 0;
		BOOL blFlag = FALSE;
		_TCHAR szErrMsg[MAXCHAR] = { _T('\0') };

		nPort = _ttoi(this->getServerPort());
		
		AfxSocketInit();

		blFlag = this->getServerSocket()->Create();
		if (!blFlag)
		{
			_stprintf(szErrMsg, _T("初始化Socket失败。\n"));
			//this->MessageBox(szErrMsg, _T("链接信息"), MB_OK);
			OutputDebugString(szErrMsg);
			return (FALSE);
		}

		blFlag = this->getServerSocket()->Connect(this->getServerIP(), nPort);
		if (!blFlag)
		{
			_stprintf(szErrMsg, _T("链接[%s]，端口[%d]失败，error[%d]。\n"), this->getServerIP(), nPort, GetLastError());
			//this->MessageBox(szErrMsg, _T("链接信息"), MB_OK);
			OutputDebugString(szErrMsg);
			this->TermConn();
			return (FALSE);
		}
		
		return TRUE;
	}
	VOID TermConn()
	{
		this->getServerSocket()->ShutDown();
		this->getServerSocket()->Close();
		AfxSocketTerm();
	}
public:
	HANDLE GetQuitEvent() { return m_hQuitEvent; }
	BOOL IsUserRunning() { return m_bUserRunning; }
	enum NetworkConnectType {
		NCTYPE_RECONNECTING = 0,
		NCTYPE_DISCONNECT = 1,
		NCTYPE_CONNECTED = 2,
	};
	BOOL getUserConnected() { return m_bUserConnected; }
	VOID setUserConnected(NetworkConnectType bUserConnected) {
		this->m_bUserConnected = bUserConnected;
		this->sendUserNotify();
	}
	VOID setUserControlText()
	{
		this->GetDlgItem(IDC_BUTTON1)->SetWindowText((m_bUserConnected != NetworkConnectType::NCTYPE_CONNECTED) ? ((m_bUserConnected != NetworkConnectType::NCTYPE_DISCONNECT) ? _T("正在重连...") : _T("开始监听")) : _T("正在监听..."));
		this->GetDlgItem(IDC_EDIT1)->EnableWindow((m_bUserConnected != NetworkConnectType::NCTYPE_CONNECTED));
		this->GetDlgItem(IDC_EDIT2)->EnableWindow((m_bUserConnected != NetworkConnectType::NCTYPE_CONNECTED));
		this->GetDlgItem(IDC_BUTTON1)->EnableWindow((m_bUserConnected != NetworkConnectType::NCTYPE_CONNECTED) & (m_bUserConnected != NetworkConnectType::NCTYPE_RECONNECTING));
		this->GetDlgItem(IDC_BUTTON2)->EnableWindow((m_bUserConnected == NetworkConnectType::NCTYPE_CONNECTED) | (m_bUserConnected == NetworkConnectType::NCTYPE_RECONNECTING));
	}
	CSocket * getServerSocket() { return &m_socketServer; }
	ServerRunInfo * getServerRunInfo() { return &m_serverRunInfo; }
	LPCTSTR getServerIP() { return m_strServerIP; };
	LPCTSTR getServerPort() { return m_strServerPort; };
private:
	CChartViewer		m_cvCPU;                     //CPU使用量
	CChartViewer		m_cvMemory;                  //内存使用量
	double				m_dbCPU[MAX_DATA_COUNT];
	double				m_dbMemory[MAX_DATA_COUNT];
	CString				m_strServerIP;
	CString				m_strServerPort;
	CSocket				m_socketServer;   //链接远程Server服务器
	NetworkConnectType	m_bUserConnected; //链接状态
	ServerRunInfo		m_serverRunInfo;//最新服务器运行状态
	DWORD				m_dwUserThreadId;//用户线程标识
	HANDLE				m_hUserThread;//用户线程句柄
	BOOL				m_bUserRunning;//用户线程状态
	HANDLE				m_hQuitEvent;//退出事件句柄
};
