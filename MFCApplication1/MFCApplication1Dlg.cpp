
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
#include "CanSim/BusDiagramLayout.h"
#include "CanSim/ProjectFile.h"
#include "CanSim/FrameEvents.h"
#include "SettingsDlg.h"

#include <algorithm>

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
	ON_WM_ERASEBKGND()
	ON_WM_INITMENUPOPUP()
	ON_WM_TIMER()
	ON_COMMAND(ID_FILE_LOAD_PROJECT, &CMFCApplication1Dlg::OnFileLoadProject)
	ON_COMMAND(ID_FILE_SAVE_PROJECT, &CMFCApplication1Dlg::OnFileSaveProject)
	ON_COMMAND(ID_EDIT_SETTINGS, &CMFCApplication1Dlg::OnEditSettings)
	ON_COMMAND(ID_EDIT_FRAME_DISPLAY, &CMFCApplication1Dlg::OnEditFrameDisplay)
	ON_COMMAND(ID_SIM_START, &CMFCApplication1Dlg::OnSimStart)
	ON_COMMAND(ID_SIM_STOP, &CMFCApplication1Dlg::OnSimStop)
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

	// Start with the default project: 2 ECUs, 500 kbps, 10 s, frame display off (004 FR-006).
	SetEcuCount(m_settings.EcuCount);

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
		CPaintDC dc(this);
		DrawBusDiagram(dc);
	}
}

// ---- feature 003: CAN bus diagram ----

namespace
{
	const COLORREF kDiagramBack = RGB(255, 255, 255);
	const COLORREF kBusColor = RGB(0, 0, 0);
	const COLORREF kTopEcuFill = RGB(225, 213, 231);
	const COLORREF kTopEcuFrame = RGB(150, 115, 166);
	const COLORREF kBottomEcuFill = RGB(255, 230, 204);
	const COLORREF kBottomEcuFrame = RGB(215, 155, 0);
	const COLORREF kTextColor = RGB(0, 0, 0);
	// Frame replay highlight (004 contracts/ui-menu-dialogs.md)
	const COLORREF kSenderFill = RGB(240, 110, 110);
	const COLORREF kSenderFrame = RGB(170, 0, 0);
	const COLORREF kReceiverFill = RGB(170, 225, 170);
	const COLORREF kReceiverFrame = RGB(40, 140, 40);
	const COLORREF kActiveBusColor = RGB(30, 90, 220);
	const int kActiveBusExtraPx = 2;
	const int kCaptionHeightPx = 26;
	const int kEcuLabelHeightPx = 15;
	const int kEcuCornerPx = 10;

	int ScalePx(int px, int dpi)
	{
		return MulDiv(px, dpi, 96);
	}

	CRect ScaleRect(const CanSim::DiagramRect& r, int dpi)
	{
		return CRect(ScalePx(r.Left, dpi), ScalePx(r.Top, dpi), ScalePx(r.Right, dpi), ScalePx(r.Bottom, dpi));
	}
}

CRect CMFCApplication1Dlg::GetDiagramRect() const
{
	CClientDC dc(const_cast<CMFCApplication1Dlg*>(this));
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	return CRect(0, 0, ScalePx(CanSim::kDiagramWidth, dpi), ScalePx(CanSim::kDiagramHeight, dpi));
}

void CMFCApplication1Dlg::SetEcuCount(int ecuCount)
{
	if (ecuCount < 0)
		ecuCount = 0;
	if (ecuCount > CanSim::kMaxEcuCount)
		ecuCount = CanSim::kMaxEcuCount;
	m_ecuCount = ecuCount;
	if (GetSafeHwnd() != nullptr)
		InvalidateRect(GetDiagramRect(), FALSE);
}

void CMFCApplication1Dlg::DrawBusDiagram(CDC& dc)
{
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	const CRect area(0, 0, ScalePx(CanSim::kDiagramWidth, dpi), ScalePx(CanSim::kDiagramHeight, dpi));

	// Draw into a memory DC first to avoid flicker.
	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, area.Width(), area.Height());
	CBitmap* oldBmp = mem.SelectObject(&bmp);

	mem.FillSolidRect(area, kDiagramBack); // 003 FR-001: always white
	mem.SetBkMode(TRANSPARENT);
	mem.SetTextColor(kTextColor);

	const CanSim::BusDiagramLayout layout = CanSim::ComputeBusDiagramLayout(m_ecuCount);

	// Frame being replayed (004 FR-013). Nothing is highlighted when the
	// frame display is off (004 FR-014) or no replay is running.
	const CanSim::FrameEvent* current = nullptr;
	if (m_runState == RunState::Running && m_settings.FrameDisplay && m_playIndex < m_events.size())
		current = &m_events[m_playIndex];

	// Bus line (003 FR-002); thicker and blue while a frame is on the bus
	CRect busRect = ScaleRect(layout.BusLine, dpi);
	if (current != nullptr)
		busRect.InflateRect(0, ScalePx(kActiveBusExtraPx, dpi));
	mem.FillSolidRect(busRect, current != nullptr ? kActiveBusColor : kBusColor);

	// Caption "CAN BUS" above the left end of the bus
	CFont captionFont;
	captionFont.CreateFont(-ScalePx(kCaptionHeightPx, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, _T("Arial"));
	CFont* oldFont = mem.SelectObject(&captionFont);
	CRect captionRect(ScalePx(CanSim::kCaptionLeft, dpi), 0,
		ScalePx(CanSim::kCaptionRight + 40, dpi), ScalePx(layout.BusLine.Top - 2, dpi));
	mem.DrawText(_T("CAN BUS"), captionRect, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

	// ECUs (003 FR-003..FR-007): nothing is drawn when the count is 0.
	CFont labelFont;
	labelFont.CreateFont(-ScalePx(kEcuLabelHeightPx, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, _T("Arial"));
	mem.SelectObject(&labelFont);

	CPen linePen(PS_SOLID, (std::max)(1, ScalePx(1, dpi)), kBusColor);
	CPen activeLinePen(PS_SOLID, ScalePx(3, dpi), kActiveBusColor);
	for (const CanSim::EcuPlacement& ecu : layout.Ecus)
	{
		const bool isSender = current != nullptr && current->TxNodeId == ecu.Number;
		bool isReceiver = false;
		if (current != nullptr)
			for (int rx : current->RxNodeIds)
				isReceiver = isReceiver || rx == ecu.Number;

		CPen* oldPen = mem.SelectObject(isSender ? &activeLinePen : &linePen);
		const int x = ScalePx(ecu.LineX, dpi);
		mem.MoveTo(x, ScalePx(ecu.LineY1, dpi));
		mem.LineTo(x, ScalePx(ecu.LineY2, dpi));

		COLORREF frameColor = ecu.OnTop ? kTopEcuFrame : kBottomEcuFrame;
		COLORREF fillColor = ecu.OnTop ? kTopEcuFill : kBottomEcuFill;
		int frameWidth = 1;
		if (isSender)
		{
			frameColor = kSenderFrame;
			fillColor = kSenderFill;
			frameWidth = 3;
		}
		else if (isReceiver)
		{
			frameColor = kReceiverFrame;
			fillColor = kReceiverFill;
		}
		CPen framePen(PS_SOLID, (std::max)(1, ScalePx(frameWidth, dpi)), frameColor);
		CBrush fillBrush(fillColor);
		mem.SelectObject(&framePen);
		CBrush* oldBrush = mem.SelectObject(&fillBrush);
		const CRect box = ScaleRect(ecu.Box, dpi);
		const int corner = ScalePx(kEcuCornerPx, dpi);
		mem.RoundRect(box, CPoint(corner, corner));
		mem.SelectObject(oldBrush);
		mem.SelectObject(oldPen);

		CString label;
		label.Format(_T("ECU %02d"), ecu.Number);
		CRect textRect = box;
		mem.DrawText(label, textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	mem.SelectObject(oldFont);
	dc.BitBlt(area.left, area.top, area.Width(), area.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldBmp);
}

// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR CMFCApplication1Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// ---- feature 004: menu driven dialog ----

namespace
{
	const UINT_PTR kReplayTimerId = 1;
	const UINT kReplayStepMs = 250; // one frame per step, idle time skipped (research.md decision 4)

	CString LoadRcString(UINT id)
	{
		CString s;
		s.LoadString(id);
		return s;
	}

	CString ToCString(const std::string& ascii)
	{
		return CString(ascii.c_str());
	}

	bool HasProjectExtension(const CString& path)
	{
		const int dot = path.ReverseFind(_T('.'));
		return dot >= 0 && path.Mid(dot).CompareNoCase(_T(".canprj")) == 0;
	}
}

// Enter and Esc must not close the dialog; only the close box does (004 FR-020).
void CMFCApplication1Dlg::OnOK()
{
}

void CMFCApplication1Dlg::OnCancel()
{
	// Reached through the close box / Alt+F4. A running replay is abandoned
	// without writing any result (004 Edge Cases).
	StopReplayTimer();
	m_runState = RunState::Idle;
	CDialogEx::OnCancel();
}

BOOL CMFCApplication1Dlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
		return TRUE;
	return CDialogEx::PreTranslateMessage(pMsg);
}

// The whole client area below the menu bar is white, whatever the system
// theme is (004 FR-004).
BOOL CMFCApplication1Dlg::OnEraseBkgnd(CDC* pDC)
{
	CRect client;
	GetClientRect(&client);
	pDC->FillSolidRect(client, RGB(255, 255, 255));
	return TRUE;
}

// Dialogs do not route ON_UPDATE_COMMAND_UI, so the menu state is set here
// (004 FR-012, FR-019; contracts/ui-menu-dialogs.md).
void CMFCApplication1Dlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CDialogEx::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
	if (bSysMenu)
		return;

	const bool running = m_runState == RunState::Running;
	const UINT idleOnly = MF_BYCOMMAND | (running ? MF_GRAYED : MF_ENABLED);
	const UINT runningOnly = MF_BYCOMMAND | (running ? MF_ENABLED : MF_GRAYED);
	pPopupMenu->EnableMenuItem(ID_FILE_LOAD_PROJECT, idleOnly);
	pPopupMenu->EnableMenuItem(ID_EDIT_SETTINGS, idleOnly);
	pPopupMenu->EnableMenuItem(ID_SIM_START, idleOnly);
	pPopupMenu->EnableMenuItem(ID_SIM_STOP, runningOnly);
	pPopupMenu->CheckMenuItem(ID_EDIT_FRAME_DISPLAY,
		MF_BYCOMMAND | (m_settings.FrameDisplay ? MF_CHECKED : MF_UNCHECKED));
}

void CMFCApplication1Dlg::OnFileLoadProject()
{
	if (m_runState == RunState::Running)
		return;

	CFileDialog dlg(TRUE, _T("canprj"), nullptr, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		LoadRcString(IDS_PROJECT_FILTER), this);
	if (dlg.DoModal() != IDOK)
		return;

	const CString path = dlg.GetPathName();
	CString message;
	if (!HasProjectExtension(path))
	{
		message.Format(LoadRcString(IDS_BAD_EXTENSION), (LPCTSTR)path);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return;
	}

	const CanSim::ConfigLoadResult loaded = CanSim::LoadConfig(std::wstring((LPCTSTR)path));
	if (!loaded.Success)
	{
		// The current project is left untouched (004 FR-010).
		message.Format(LoadRcString(IDS_LOAD_ERROR), (LPCTSTR)ToCString(loaded.ErrorMessage));
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return;
	}

	m_settings = loaded.Project;
	m_definitions = loaded.Config;
	m_projectPath = path;
	SetEcuCount(m_settings.EcuCount);
}

bool CMFCApplication1Dlg::AskSavePath(CString& path)
{
	CFileDialog dlg(FALSE, _T("canprj"), nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		LoadRcString(IDS_PROJECT_FILTER), this);
	if (dlg.DoModal() != IDOK)
		return false;
	path = dlg.GetPathName();
	return true;
}

void CMFCApplication1Dlg::OnFileSaveProject()
{
	CString path;
	if (!AskSavePath(path))
		return;
	if (!CanSim::SaveProject(std::wstring((LPCTSTR)path), m_settings, m_definitions))
	{
		CString message;
		message.Format(LoadRcString(IDS_SAVE_ERROR), (LPCTSTR)path);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return;
	}
	m_projectPath = path;
}

void CMFCApplication1Dlg::OnEditSettings()
{
	if (m_runState == RunState::Running)
		return;

	CSettingsDlg dlg(m_settings, m_definitions.BitRateBps, this);
	if (dlg.DoModal() != IDOK)
		return; // cancelled: nothing changes (004 FR-007, 003 FR-010)

	m_settings.EcuCount = dlg.Settings.EcuCount;
	m_settings.DurationSec = dlg.Settings.DurationSec;
	m_definitions.BitRateBps = dlg.BitRateBps;
	SetEcuCount(m_settings.EcuCount);
}

void CMFCApplication1Dlg::OnEditFrameDisplay()
{
	m_settings.FrameDisplay = !m_settings.FrameDisplay;

	// Switching it off during a replay ends the replay at once and writes the
	// full result: nothing may be highlighted any more (004 FR-014) and the
	// result does not depend on the display (004 FR-015).
	if (!m_settings.FrameDisplay && m_runState == RunState::Running)
		FinishReplay();
}

bool CMFCApplication1Dlg::WriteResults(const std::vector<CanSim::LogRow>& rows,
	const CanSim::Network& runNetwork, const CanSim::Network& endState,
	CString& logPath, CString& summaryPath, CString& startupPath)
{
	// Same file names as before feature 004: <project path>.log.csv etc.
	logPath = m_projectPath + _T(".log.csv");
	summaryPath = m_projectPath + _T(".summary.csv");
	startupPath = m_projectPath + _T(".startup.csv");

	CanSim::Logger logger;
	for (const CanSim::LogRow& row : rows)
		logger.AddRow(row);

	bool ok = logger.WriteCsv(std::wstring((LPCTSTR)logPath));
	ok = CanSim::WriteMetricsCsv(CanSim::ComputeMetrics(rows, runNetwork), std::wstring((LPCTSTR)summaryPath)) && ok;
	ok = CanSim::WriteStartupCsv(CanSim::BuildStartupRecords(endState), std::wstring((LPCTSTR)startupPath)) && ok;
	if (!ok)
	{
		CString message;
		message.Format(LoadRcString(IDS_WRITE_ERROR), (LPCTSTR)m_projectPath);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
	}
	return ok;
}

void CMFCApplication1Dlg::OnSimStart()
{
	if (m_runState == RunState::Running)
		return;

	// The results are written next to the project file, so it needs a place
	// first (004 FR-017).
	if (m_projectPath.IsEmpty())
	{
		CString path;
		if (!AskSavePath(path))
			return;
		if (!CanSim::SaveProject(std::wstring((LPCTSTR)path), m_settings, m_definitions))
		{
			CString message;
			message.Format(LoadRcString(IDS_SAVE_ERROR), (LPCTSTR)path);
			AfxMessageBox(message, MB_OK | MB_ICONWARNING);
			return;
		}
		m_projectPath = path;
	}

	{
		// The run is synchronous; at most 600 s of simulated time
		// (research.md decision 4).
		CWaitCursor wait;
		m_runNetwork = CanSim::BuildRunNetwork(m_settings, m_definitions);
		CanSim::SimulationEngine engine(m_runNetwork, m_settings.DurationSec * 1000000LL);
		CanSim::Logger logger;
		engine.Run(logger);
		m_runRows = logger.Rows();
		m_runEndState = engine.GetNetwork();
	}

	if (m_settings.FrameDisplay)
	{
		m_events = CanSim::BuildFrameEvents(m_runRows);
		if (!m_events.empty())
		{
			m_playIndex = 0;
			m_runState = RunState::Running;
			m_replayTimer = SetTimer(kReplayTimerId, kReplayStepMs, nullptr);
			InvalidateRect(GetDiagramRect(), FALSE);
			return;
		}
	}

	FinishReplay();
}

void CMFCApplication1Dlg::StopReplayTimer()
{
	if (m_replayTimer != 0)
	{
		KillTimer(m_replayTimer);
		m_replayTimer = 0;
	}
}

// Writes the full result of the current run and returns to Idle.
void CMFCApplication1Dlg::FinishReplay()
{
	StopReplayTimer();
	m_runState = RunState::Idle;
	m_events.clear();
	InvalidateRect(GetDiagramRect(), FALSE);

	CString logPath, summaryPath, startupPath;
	CWaitCursor wait;
	if (WriteResults(m_runRows, m_runNetwork, m_runEndState, logPath, summaryPath, startupPath))
	{
		CString message;
		message.Format(LoadRcString(IDS_SIM_COMPLETE), (LPCTSTR)logPath, (LPCTSTR)summaryPath,
			(LPCTSTR)startupPath);
		AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
	}
}

void CMFCApplication1Dlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != kReplayTimerId || m_runState != RunState::Running)
	{
		CDialogEx::OnTimer(nIDEvent);
		return;
	}

	++m_playIndex;
	if (m_playIndex >= m_events.size())
	{
		FinishReplay();
		return;
	}
	InvalidateRect(GetDiagramRect(), FALSE);
}

// Stops the replay at the frame currently shown. The run is repeated up to
// that frame's Tx_Start so that the result holds exactly the frames whose
// transmission completed by then (004 FR-018, SC-008).
void CMFCApplication1Dlg::OnSimStop()
{
	if (m_runState != RunState::Running)
		return;

	StopReplayTimer();
	const long long stopUs = m_events[m_playIndex].TxStartUs;
	m_runState = RunState::Idle;
	m_events.clear();
	InvalidateRect(GetDiagramRect(), FALSE);

	CWaitCursor wait;
	CanSim::SimulationEngine engine(m_runNetwork, stopUs);
	CanSim::Logger logger;
	engine.Run(logger);

	CString logPath, summaryPath, startupPath;
	if (WriteResults(logger.Rows(), m_runNetwork, engine.GetNetwork(), logPath, summaryPath, startupPath))
	{
		CString stopText;
		stopText.Format(_T("%I64d"), stopUs);
		CString message;
		message.Format(LoadRcString(IDS_SIM_STOPPED), (LPCTSTR)stopText, (LPCTSTR)logPath,
			(LPCTSTR)summaryPath, (LPCTSTR)startupPath);
		AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
	}
}
