#define _CRT_SECURE_NO_WARNINGS
// ffmpeg_gui_demoDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "ffmpeg_gui_demo.h"
#include "ffmpeg_gui_demoDlg.h"
#include "afxdialogex.h"

#include "test.h"
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CffmpegguidemoDlg 对话框



CffmpegguidemoDlg::CffmpegguidemoDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FFMPEG_GUI_DEMO_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CffmpegguidemoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CffmpegguidemoDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CffmpegguidemoDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_RECAUDIO, &CffmpegguidemoDlg::OnBnClickedRecaudio)
END_MESSAGE_MAP()


// CffmpegguidemoDlg 消息处理程序

BOOL CffmpegguidemoDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CffmpegguidemoDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CffmpegguidemoDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CffmpegguidemoDlg::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CffmpegguidemoDlg::OnBnClickedRecaudio()
{
	// TODO: 在此添加控件通知处理程序代码
	CString title;
	GetDlgItemText(IDC_RECAUDIO, title);
	if (title == _T("开始录制"))
	{
		SetDlgItemText(IDC_RECAUDIO, _T("停止录制"));
		std::thread audio_thread(rec_audio);
		audio_thread.detach();
	}
	else
	{
		set_status(0);
		SetDlgItemText(IDC_RECAUDIO, _T("开始录制"));
	}
		

	

	
		
	//rec_audio();
}
