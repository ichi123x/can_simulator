
// MFCApplication1Dlg.cpp : 実装ファイル
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include "afxdialogex.h"

#include "CanSim/ConfigLoader.h"
#include "CanSim/SimulationEngine.h"
#include "CanSim/Logger.h"
#include "CanSim/MetricsAggregator.h"
#include "CanSim/StartupControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// アプリケーションのバージョン情報に使われる CAboutDlg ダイアログ

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

// 実装
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMFCApplication1Dlg ダイアログ



CMFCApplication1Dlg::CMFCApplication1Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFCAPPLICATION1_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCApplication1Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMFCApplication1Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, &CMFCApplication1Dlg::OnBnClickedButtonBrowse)
	ON_BN_CLICKED(IDC_BUTTON_RUN, &CMFCApplication1Dlg::OnBnClickedButtonRun)
END_MESSAGE_MAP()


// CMFCApplication1Dlg メッセージ ハンドラー

BOOL CMFCApplication1Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// "バージョン情報..." メニューをシステム メニューに追加します。

	// IDM_ABOUTBOX は、システム コマンドの範囲内になければなりません。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// このダイアログのアイコンを設定します。アプリケーションのメイン ウィンドウがダイアログでない場合、
	//  Framework は、この設定を自動的に行います。
	SetIcon(m_hIcon, TRUE);			// 大きいアイコンの設定
	SetIcon(m_hIcon, FALSE);		// 小さいアイコンの設定

	GetDlgItem(IDC_EDIT_DURATION)->SetWindowText(_T("10"));

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

void CMFCApplication1Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// ダイアログに最小化ボタンを追加する場合、アイコンを描画するための
//  下のコードが必要です。ドキュメント/ビュー モデルを使う MFC アプリケーションの場合、
//  これは、Framework によって自動的に設定されます。

void CMFCApplication1Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 描画のデバイス コンテキスト

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// クライアントの四角形領域内の中央
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// アイコンの描画
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR CMFCApplication1Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMFCApplication1Dlg::SetResultText(const CString& text)
{
	GetDlgItem(IDC_STATIC_RESULT)->SetWindowText(text);
}

void CMFCApplication1Dlg::OnBnClickedButtonBrowse()
{
	CFileDialog dlg(TRUE, _T("cfg"), nullptr,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("Config Files (*.cfg;*.txt)|*.cfg;*.txt|All Files (*.*)|*.*||"), this);
	if (dlg.DoModal() == IDOK)
	{
		GetDlgItem(IDC_EDIT_CONFIG_PATH)->SetWindowText(dlg.GetPathName());
	}
}

void CMFCApplication1Dlg::OnBnClickedButtonRun()
{
	CString configPathStr;
	GetDlgItem(IDC_EDIT_CONFIG_PATH)->GetWindowText(configPathStr);
	if (configPathStr.IsEmpty())
	{
		SetResultText(_T("Please choose a configuration file first."));
		return;
	}

	CString durationStr;
	GetDlgItem(IDC_EDIT_DURATION)->GetWindowText(durationStr);
	const long long durationSec = _wtoi64(durationStr);
	if (durationSec <= 0)
	{
		SetResultText(_T("Please enter a simulation duration (seconds) greater than 0."));
		return;
	}

	const std::wstring configPath((LPCTSTR)configPathStr);
	CanSim::ConfigLoadResult cfg = CanSim::LoadConfig(configPath);
	if (!cfg.Success)
	{
		CString msg;
		msg.Format(_T("Configuration error: %hs"), cfg.ErrorMessage.c_str());
		SetResultText(msg);
		return;
	}

	CanSim::SimulationEngine engine(cfg.Config, durationSec * 1000000LL);
	CanSim::Logger logger;
	engine.Run(logger);

	CString logPathStr = configPathStr + _T(".log.csv");
	const std::wstring logPath((LPCTSTR)logPathStr);
	const bool logWritten = logger.WriteCsv(logPath);

	const std::vector<CanSim::GroupMetrics> metrics = CanSim::ComputeMetrics(logger.Rows(), cfg.Config);
	CString summaryPathStr = configPathStr + _T(".summary.csv");
	const std::wstring summaryPath((LPCTSTR)summaryPathStr);
	const bool summaryWritten = CanSim::WriteMetricsCsv(metrics, summaryPath);

	const std::vector<CanSim::StartupRecord> startupRecords =
		CanSim::BuildStartupRecords(engine.GetNetwork());
	CString startupPathStr = configPathStr + _T(".startup.csv");
	const std::wstring startupPath((LPCTSTR)startupPathStr);
	const bool startupWritten = CanSim::WriteStartupCsv(startupRecords, startupPath);

	CString result;
	result.Format(_T("Simulation complete.\r\nLog rows: %d\r\nLog file: %s%s\r\nSummary file: %s%s\r\nStartup file: %s%s"),
		static_cast<int>(logger.Rows().size()),
		(LPCTSTR)logPathStr, logWritten ? _T("") : _T(" (write failed)"),
		(LPCTSTR)summaryPathStr, summaryWritten ? _T("") : _T(" (write failed)"),
		(LPCTSTR)startupPathStr, startupWritten ? _T("") : _T(" (write failed)"));
	SetResultText(result);
}

