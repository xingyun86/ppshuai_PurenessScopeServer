
// PurenessLineDlg.h : 头文件
//

#pragma once

#include "ChartViewer.h"
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
	DECLARE_MESSAGE_MAP()

	CEdit m_txtServerIP;
	CEdit m_txtServerPort;
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();

private:
	void shiftData(double *data, int len, double newValue);
	void drawChart();
	void InitView();
	static DWORD WINAPI GetServerStateInfo(void * p);
	BOOL StartUserThread() {
		m_bUserRunning = TRUE;
		m_hUserThread = CreateThread(NULL, 0, &CPurenessLineDlg::GetServerStateInfo, this, 0, &m_dwUserThreadId);
		return (m_hUserThread && m_hUserThread != INVALID_HANDLE_VALUE);
	}
	void StopUserThread() {
		if (m_bUserConnected)
		{
			this->setUserConnected(FALSE);
		}
		if (m_hUserThread && m_hUserThread != INVALID_HANDLE_VALUE)
		{
			m_bUserRunning = FALSE;
			CloseHandle(m_hUserThread);
			m_hUserThread = 0;
			m_dwUserThreadId = 0;
		}
	}
public:
	BOOL InitConn() {
		//解析端口地址
		int nPort = 0;
		BOOL blFlag = FALSE;
		nPort = _ttoi(this->getServerPort());

		this->setUserConnected(FALSE);

		AfxSocketInit();

		blFlag = this->getServerSocket()->Create();
		if (FALSE == blFlag)
		{
			this->MessageBox(_T("初始化Socket失败。"), _T("链接信息"), MB_OK);
			return (FALSE);
		}

		blFlag = this->getServerSocket()->Connect(this->getServerIP(), nPort);
		if (FALSE == blFlag)
		{
			_TCHAR szError[MAXCHAR] = { _T('\0') };
			wsprintf(szError, _T("链接[%s]，端口[%d]失败，error[%d]。"), this->getServerIP(), nPort, GetLastError());
			this->MessageBox((LPCTSTR)szError, _T("链接信息"), MB_OK);
			this->getServerSocket()->ShutDown();
			this->getServerSocket()->Close();
			AfxSocketTerm();
			return (FALSE);
		}

		this->setUserConnected(TRUE);

		return TRUE;
	}
	VOID TermConn()
	{
		this->getServerSocket()->ShutDown();
		this->getServerSocket()->Close();
		this->setUserConnected(FALSE);
		AfxSocketTerm();
	}
public:
	BOOL IsUserRunning() { return m_bUserRunning; }
	BOOL IsUserConnected() { return m_bUserConnected; }
	VOID setUserConnected(BOOL bUserConnected) { 
		m_bUserConnected = bUserConnected;
		this->GetDlgItem(IDC_EDIT1)->EnableWindow(!m_bUserConnected);
		this->GetDlgItem(IDC_EDIT2)->EnableWindow(!m_bUserConnected);
		this->GetDlgItem(IDC_BUTTON1)->EnableWindow(!m_bUserConnected);
		this->GetDlgItem(IDC_BUTTON2)->EnableWindow(m_bUserConnected);
	}
	CSocket * getServerSocket() { return &m_socketServer; }
	ServerRunInfo * getServerRunInfo() { return &m_serverRunInfo; }
	LPCTSTR getServerIP() { return m_strServerIP; };
	LPCTSTR getServerPort() { return m_strServerPort; };
private:
	CChartViewer	m_cvCPU;                     //CPU使用量
	CChartViewer	m_cvMemory;                  //内存使用量
	double			m_dbCPU[MAX_DATA_COUNT];
	double			m_dbMemory[MAX_DATA_COUNT];
	CString			m_strServerIP;
	CString			m_strServerPort;
	CSocket			m_socketServer;                 //链接远程Server服务器
	BOOL			m_bUserConnected;                   //链接状态
	ServerRunInfo	m_serverRunInfo;			  //最新服务器运行状态
	DWORD			m_dwUserThreadId;//用户线程标识
	HANDLE			m_hUserThread;//用户线程句柄
	BOOL			m_bUserRunning;//用户线程状态
};
