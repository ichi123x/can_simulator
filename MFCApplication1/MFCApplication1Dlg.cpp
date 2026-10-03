
// MFCApplication1Dlg.cpp : 実装ファイル
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include "afxdialogex.h"

#include "CanSim/FrameEditor.h"
#include "CanSim/ConfigLoader.h"
#include "CanSim/SimulationEngine.h"
#include "CanSim/Logger.h"
#include "CanSim/MetricsAggregator.h"
#include "CanSim/StartupControl.h"
#include "CanSim/BusDiagramLayout.h"
#include "CanSim/ProjectFile.h"
#include "CanSim/FrameEvents.h"
#include "SettingsDlg.h"
#include "FrameEditDlg.h"
#include "LogViewDlg.h"
#include "AnalysisDlg.h"
#include "CanSim/LogFileIndex.h"

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
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_FILE_LOAD_PROJECT, &CMFCApplication1Dlg::OnFileLoadProject)
	ON_COMMAND(ID_FILE_SAVE_PROJECT, &CMFCApplication1Dlg::OnFileSaveProject)
	ON_COMMAND(ID_FILE_SAVE_OVERWRITE, &CMFCApplication1Dlg::OnFileSaveOverwrite)
	ON_COMMAND(ID_EDIT_SETTINGS, &CMFCApplication1Dlg::OnEditSettings)
	ON_COMMAND(ID_EDIT_FRAMES, &CMFCApplication1Dlg::OnEditFrames)
	ON_COMMAND(ID_SIM_START, &CMFCApplication1Dlg::OnSimStart)
	ON_COMMAND(ID_SIM_PAUSE, &CMFCApplication1Dlg::OnSimPause)
	ON_COMMAND(ID_SIM_STOP, &CMFCApplication1Dlg::OnSimStop)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
	ON_MESSAGE(WM_APP_FRAME_EDITOR_CLOSED, &CMFCApplication1Dlg::OnFrameEditorClosed)
	ON_MESSAGE(WM_APP_ECU_LABEL_CHANGED, &CMFCApplication1Dlg::OnEcuLabelChanged)
	ON_MESSAGE(WM_APP_LOAD_STARTUP_PROJECT, &CMFCApplication1Dlg::OnLoadStartupProject)
	ON_COMMAND(ID_LOG_VIEW_SIMLOG, &CMFCApplication1Dlg::OnLogViewSimLog)
	ON_COMMAND(ID_LOG_CLEAR, &CMFCApplication1Dlg::OnLogClear)
	ON_MESSAGE(WM_APP_LOG_VIEWER_CLOSED, &CMFCApplication1Dlg::OnLogViewerClosed)
	ON_COMMAND(ID_ANALYSIS_LATENCY, &CMFCApplication1Dlg::OnAnalysisLatency)
	ON_COMMAND(ID_ANALYSIS_BUSLOAD, &CMFCApplication1Dlg::OnAnalysisBusLoad)
	ON_MESSAGE(WM_APP_LATENCY_CLOSED, &CMFCApplication1Dlg::OnLatencyClosed)
	ON_MESSAGE(WM_APP_BUSLOAD_CLOSED, &CMFCApplication1Dlg::OnBusLoadClosed)
	ON_WM_DESTROY()
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

	// 024: "CAN Bus Simulator" from the .rc caption, followed by the project file label
	GetWindowText(m_baseTitle);
	SetProjectPath(CString());

	// 014 FR-009: the toolbar takes the top of the client area; the window
	// grows by its height so that the diagram and the log view keep their size.
	{
		CRect window;
		GetWindowRect(&window);
		SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() + ContentTop(), SWP_NOMOVE | SWP_NOZORDER);
	}

	// Start with the default project: 2 ECUs, 500 kbps, 10 s, frame display off (004 FR-006).
	SetEcuCount(m_settings.EcuCount);

	// 009: log view font and the scroll bar at the right edge of the grey area.
	{
		CClientDC dc(this);
		const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
		m_logFont.CreateFont(-MulDiv(13, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			FIXED_PITCH | FF_MODERN, _T("MS Gothic"));
	}
	const CRect logRect = GetLogRect();
	const CRect textRect = GetLogTextRect();
	GetDlgItem(IDC_SCROLL_LOG)->MoveWindow(textRect.right, logRect.top, logRect.right - textRect.right, logRect.Height());
	UpdateLogScroll();

	// 024: a project given on the command line is loaded after the window is shown
	if (!m_startupProject.IsEmpty())
		PostMessage(WM_APP_LOAD_STARTUP_PROJECT);

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
		DrawSimToolbar(dc);
		DrawBusDiagram(dc);
		DrawLogView(dc);
		DrawLogFrame(dc);
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
	// 016: receivers are blue, the frame and the stub line in the bus colour
	const COLORREF kReceiverFill = RGB(160, 195, 245);
	const COLORREF kReceiverFrame = RGB(30, 90, 220);
	const COLORREF kActiveBusColor = RGB(30, 90, 220);
	const int kActiveBusExtraPx = 2;
	const int kCaptionHeightPx = 26;
	const int kEcuLabelHeightPx = 15;
	const int kEcuCornerPx = 10;
	// Feature 011: an ECU just double-clicked, and the borders of the diagram
	// and the log view.
	const COLORREF kGrayFill = RGB(205, 205, 205);
	const COLORREF kGrayFrame = RGB(128, 128, 128);
	const COLORREF kGrayText = RGB(96, 96, 96);
	const COLORREF kAreaFrameColor = RGB(160, 160, 160);

	// Same conversion as the ECU hit test (feature 010).
	int ScalePx(int px, int dpi)
	{
		return CanSim::ScaleToDpi(px, dpi);
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
	const int top = ContentTop();
	return CRect(0, top, ScalePx(CanSim::kDiagramWidth, dpi), top + ScalePx(CanSim::kDiagramHeight, dpi));
}

// 014: the diagram and the log view are moved down by the toolbar. Their
// CanSim geometry stays relative to this content origin.
int CMFCApplication1Dlg::ContentTop() const
{
	CClientDC dc(const_cast<CMFCApplication1Dlg*>(this));
	return ScalePx(CanSim::kToolbarHeight, dc.GetDeviceCaps(LOGPIXELSX));
}

void CMFCApplication1Dlg::SetEcuCount(int ecuCount)
{
	if (ecuCount < 0)
		ecuCount = 0;
	if (ecuCount > CanSim::kMaxEcuCount)
		ecuCount = CanSim::kMaxEcuCount;
	m_ecuCount = ecuCount;
	if (m_grayEcu > ecuCount)
		ClearGray(); // 011 FR-007: the grey ECU is no longer shown
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

	// 011 FR-008: border inside the diagram area, drawn before the contents
	{
		CPen borderPen(PS_SOLID, (std::max)(1, ScalePx(1, dpi)), kAreaFrameColor);
		CPen* oldPen = mem.SelectObject(&borderPen);
		CGdiObject* oldBrush = mem.SelectStockObject(NULL_BRUSH);
		mem.Rectangle(ScaleRect(layout.Frame, dpi));
		mem.SelectObject(oldBrush);
		mem.SelectObject(oldPen);
	}

	// Frames on the bus since the previous refresh (005 FR-009). Nothing is
	// highlighted when the frame display is off (005 FR-010) or no
	// simulation is running. 017: the toolbar check box switches it at any
	// time; m_active is collected either way, so the change shows at once.
	const CanSim::ActiveFrames* active = nullptr;
	if (InRun() && m_settings.FrameDisplay)
		active = &m_active;
	const bool busBusy = active != nullptr && active->BusBusy;

	// Bus line (003 FR-002); thicker and blue while a frame is on the bus
	CRect busRect = ScaleRect(layout.BusLine, dpi);
	if (busBusy)
		busRect.InflateRect(0, ScalePx(kActiveBusExtraPx, dpi));
	mem.FillSolidRect(busRect, busBusy ? kActiveBusColor : kBusColor);

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
	// 022: small font for the ECU label
	CFont ecuLabelFont;
	ecuLabelFont.CreateFont(-ScalePx(10, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));

	CPen linePen(PS_SOLID, (std::max)(1, ScalePx(1, dpi)), kBusColor);
	CPen activeLinePen(PS_SOLID, ScalePx(3, dpi), kActiveBusColor);
	for (const CanSim::EcuPlacement& ecu : layout.Ecus)
	{
		const bool isSender = active != nullptr &&
			std::find(active->Senders.begin(), active->Senders.end(), ecu.Number) != active->Senders.end();
		const bool isReceiver = active != nullptr &&
			std::find(active->Receivers.begin(), active->Receivers.end(), ecu.Number) != active->Receivers.end();

		CPen* oldPen = mem.SelectObject(isSender || isReceiver ? &activeLinePen : &linePen); // 016 FR-002
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
			frameWidth = 2;
		}
		const bool isGray = ecu.Number == m_grayEcu; // 011 FR-003
		if (isGray)
		{
			frameColor = kGrayFrame;
			fillColor = kGrayFill;
			frameWidth = 1;
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
		mem.SetTextColor(isGray ? kGrayText : kTextColor);
		// 022: with an ECU label, "ECU nn" goes to the upper row and the label
		// (10 characters at most, then "...") to the lower row in small text.
		const CanSim::Node* def = CanSim::FindNodeDefinition(m_definitions, ecu.Number);
		const std::string ecuLabel = def != nullptr ? CanSim::EcuLabelForDiagram(def->Label) : std::string();
		if (ecuLabel.empty())
		{
			CRect textRect = box;
			mem.DrawText(label, textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		else
		{
			CRect upper(box.left, box.top + ScalePx(6, dpi), box.right, box.top + ScalePx(26, dpi));
			mem.DrawText(label, upper, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
			CRect lower(box.left + ScalePx(3, dpi), box.top + ScalePx(26, dpi), box.right - ScalePx(3, dpi), box.top + ScalePx(44, dpi));
			const CString text(CA2W(ecuLabel.c_str(), CP_UTF8));
			// Small text (10 px); a long label is drawn smaller, down to 7 px,
			// so that it stays inside the box at every DPI.
			CFont* previous = mem.SelectObject(&ecuLabelFont);
			CFont smaller;
			for (int px = 9; px >= 7 && mem.GetTextExtent(text).cx > lower.Width(); --px)
			{
				mem.SelectObject(&ecuLabelFont);
				smaller.DeleteObject();
				smaller.CreateFont(-ScalePx(px, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
					DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
					DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
				mem.SelectObject(&smaller);
			}
			mem.DrawText(text, lower, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
			mem.SelectObject(previous);
		}
		mem.SetTextColor(kTextColor);
	}

	// 018: flow animation over the highlight: glows around the boxes, then
	// the particles moving from the senders to the receivers. Revision 1:
	// the paths of the last 0.3 s are kept so that they do not blink.
	if (active != nullptr)
	{
		const std::vector<std::pair<int, int>> flows = m_flowHold.Current(m_flowElapsedUs);
		if (!flows.empty())
			DrawFlowEffect(mem, layout, flows, dpi);
	}

	mem.SelectObject(oldFont);
	dc.BitBlt(area.left, area.top + ContentTop(), area.Width(), area.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldBmp);
}

void CMFCApplication1Dlg::DrawFlowEffect(CDC& mem, const CanSim::BusDiagramLayout& layout,
	const std::vector<std::pair<int, int>>& flows, int dpi)
{
	const CanSim::FlowEffect effect = CanSim::ComputeFlowEffect(layout, flows, m_flowElapsedUs);
	const COLORREF kFlowColor = RGB(255, 140, 0); // 018 revision 1: orange, visible on white and on blue
	CGdiObject* oldBrush = mem.SelectStockObject(NULL_BRUSH);
	for (const CanSim::FlowGlow& glow : effect.Glows)
	{
		const CanSim::EcuPlacement* ecu = nullptr;
		for (const CanSim::EcuPlacement& e : layout.Ecus)
			if (e.Number == glow.Ecu)
				ecu = &e;
		if (ecu == nullptr)
			continue;
		// Light orange towards orange, 1 to 4 px, as the glow gets stronger.
		const double s = glow.Strength;
		const COLORREF color = RGB(255, 190 - static_cast<int>(50 * s), 120 - static_cast<int>(120 * s));
		CPen pen(PS_SOLID, (std::max)(1, static_cast<int>((1.0 + 3.0 * s) * dpi / 96.0 + 0.5)), color);
		CPen* oldPen = mem.SelectObject(&pen);
		CRect r = ScaleRect(ecu->Box, dpi);
		r.InflateRect(ScalePx(CanSim::kFlowGlowInset, dpi), ScalePx(CanSim::kFlowGlowInset, dpi));
		const int corner = ScalePx(kEcuCornerPx + 3, dpi);
		mem.RoundRect(r, CPoint(corner, corner));
		mem.SelectObject(oldPen);
	}

	CPen edge(PS_SOLID, (std::max)(1, ScalePx(1, dpi)), RGB(255, 255, 255));
	CBrush fill(kFlowColor);
	CPen* oldPen = mem.SelectObject(&edge);
	mem.SelectObject(&fill);
	const int radius = ScalePx(CanSim::kFlowParticleRadius, dpi);
	for (const CanSim::FlowPoint& p : effect.Particles)
	{
		const int x = static_cast<int>(p.X * dpi / 96.0 + 0.5);
		const int y = static_cast<int>(p.Y * dpi / 96.0 + 0.5);
		mem.Ellipse(x - radius, y - radius, x + radius + 1, y + radius + 1);
	}
	mem.SelectObject(oldPen);
	mem.SelectObject(oldBrush);
}

// ---- feature 014: toolbar with Start / Pause / Stop ----

namespace
{
	const COLORREF kToolbarBack = RGB(45, 45, 48);
	const COLORREF kButtonFace = RGB(240, 240, 240);
	const COLORREF kButtonPressed = RGB(200, 200, 200);
	const COLORREF kButtonDisabled = RGB(95, 95, 98);
	const COLORREF kGlyphDisabled = RGB(140, 140, 140);
	// Indexed by CanSim::ToolbarButton (023: Save and Load before Start)
	const COLORREF kGlyphColors[CanSim::kToolbarButtonCount] = {
		RGB(120, 70, 180),  // Save: purple floppy
		RGB(210, 150, 30),  // Load: ochre folder
		RGB(30, 160, 70),   // Start: green triangle
		RGB(30, 110, 200),  // Pause: blue bars
		RGB(210, 40, 40),   // Stop: red square
	};
	const UINT kButtonNames[CanSim::kToolbarButtonCount] = {
		IDS_FILE_BTN_SAVE, IDS_FILE_BTN_LOAD, IDS_SIM_BTN_START, IDS_SIM_BTN_PAUSE, IDS_SIM_BTN_STOP };
	// Save overwrites the project file (toolbar only); Load is the menu command
	const UINT kButtonCommands[CanSim::kToolbarButtonCount] = {
		ID_FILE_SAVE_OVERWRITE, ID_FILE_LOAD_PROJECT, ID_SIM_START, ID_SIM_PAUSE, ID_SIM_STOP };
	const int kButtonLabelPx = 12;
	const int kButtonNamePx = 10; // 019: names under the half size buttons
	// 015: speed slider
	const int kSpeedLabelPx = 11;
	const COLORREF kSpeedCurrentText = RGB(255, 210, 60);
	const COLORREF kSpeedThumbFrame = RGB(30, 110, 200);

	// The rectangles from SimControls are already scaled to the DPI.
	CRect ButtonRect(const CanSim::DiagramRect& r)
	{
		return CRect(r.Left, r.Top, r.Right, r.Bottom);
	}
}

bool CMFCApplication1Dlg::IsToolbarButtonEnabled(int index) const
{
	return CanSim::IsToolbarButtonEnabled(index, m_runPhase, IsEditingFrames());
}

CRect CMFCApplication1Dlg::GetToolbarRect() const
{
	CRect client;
	GetClientRect(&client);
	return CRect(0, 0, client.right, ContentTop());
}

CanSim::SimCommandStates CMFCApplication1Dlg::SimStates() const
{
	return CanSim::GetSimCommandStates(m_runPhase, IsEditingFrames());
}

void CMFCApplication1Dlg::DrawSimToolbar(CDC& dc)
{
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	const CRect area = GetToolbarRect();
	if (area.IsRectEmpty())
		return;

	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, area.Width(), area.Height());
	CBitmap* oldBmp = mem.SelectObject(&bmp);
	mem.FillSolidRect(area, kToolbarBack);
	mem.SetBkMode(TRANSPARENT);

	CFont font;
	font.CreateFont(-ScalePx(kButtonLabelPx, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
	CFont* oldFont = mem.SelectObject(&font);
	// 019: the button names are smaller (half size buttons)
	CFont nameFont;
	nameFont.CreateFont(-ScalePx(kButtonNamePx, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));

	for (int i = 0; i < CanSim::kToolbarButtonCount; ++i)
	{
		const bool enabled = IsToolbarButtonEnabled(i);
		const COLORREF face = !enabled ? kButtonDisabled : (m_pressedButton == i && m_pressedInside ? kButtonPressed : kButtonFace);
		const COLORREF glyph = enabled ? kGlyphColors[i] : kGlyphDisabled;

		// Box with rounded corners
		const CRect box = ButtonRect(CanSim::ToolbarButtonBoxRect(i, dpi));
		CBrush faceBrush(face);
		CPen facePen(PS_SOLID, 1, face);
		CBrush* oldBrush = mem.SelectObject(&faceBrush);
		CPen* oldPen = mem.SelectObject(&facePen);
		const int corner = ScalePx(CanSim::kSimButtonCorner, dpi);
		mem.RoundRect(box, CPoint(corner, corner));

		// Symbol in the middle of the box
		const int g = ScalePx(CanSim::kSimButtonGlyph, dpi);
		const CPoint c = box.CenterPoint();
		const CRect glyphRect(c.x - g / 2, c.y - g / 2, c.x - g / 2 + g, c.y - g / 2 + g);
		CBrush glyphBrush(glyph);
		CPen glyphPen(PS_SOLID, 1, glyph);
		mem.SelectObject(&glyphBrush);
		mem.SelectObject(&glyphPen);
		const CanSim::ToolbarButton kind = static_cast<CanSim::ToolbarButton>(i);
		if (kind == CanSim::ToolbarButton::Save)
		{
			// Floppy: a square with a white window at the top
			mem.FillSolidRect(glyphRect, glyph);
			const int w = ScalePx(5, dpi);
			const int h = ScalePx(3, dpi);
			mem.FillSolidRect(c.x - w / 2, glyphRect.top + ScalePx(1, dpi), w, h, enabled ? RGB(255, 255, 255) : face);
		}
		else if (kind == CanSim::ToolbarButton::Load)
		{
			// Folder: the body at the bottom and a tab on its top left
			const int bodyH = ScalePx(7, dpi);
			mem.FillSolidRect(glyphRect.left, glyphRect.bottom - bodyH, g, bodyH, glyph);
			mem.FillSolidRect(glyphRect.left, glyphRect.bottom - bodyH - ScalePx(2, dpi), ScalePx(4, dpi), ScalePx(2, dpi), glyph);
		}
		else if (kind == CanSim::ToolbarButton::Start)
		{
			const POINT tri[3] = {
				{ glyphRect.left + g / 8, glyphRect.top },
				{ glyphRect.right - g / 8, c.y },
				{ glyphRect.left + g / 8, glyphRect.bottom },
			};
			mem.Polygon(tri, 3);
		}
		else if (kind == CanSim::ToolbarButton::Pause)
		{
			const int bar = ScalePx(3, dpi); // 019: half size
			const int gap = ScalePx(3, dpi);
			const int left = c.x - (bar * 2 + gap) / 2;
			mem.FillSolidRect(left, glyphRect.top, bar, g, glyph);
			mem.FillSolidRect(left + bar + gap, glyphRect.top, bar, g, glyph);
		}
		else
		{
			const int s = ScalePx(8, dpi); // 019: half size
			mem.FillSolidRect(c.x - s / 2, c.y - s / 2, s, s, glyph);
		}
		mem.SelectObject(oldBrush);
		mem.SelectObject(oldPen);

		// Name under the box
		CRect label = ButtonRect(CanSim::ToolbarButtonLabelRect(i, dpi));
		mem.SetTextColor(enabled ? RGB(255, 255, 255) : kGlyphDisabled);
		CString name;
		name.LoadString(kButtonNames[i]);
		mem.SelectObject(&nameFont);
		// 023: not clipped, since "Load" has three characters, a little wider than
		// the box; centered, it only reaches into the gap next to the button
		mem.DrawText(name, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
		mem.SelectObject(&font);
	}

	// 017: frame display check box
	{
		const CRect box = ButtonRect(CanSim::FrameDisplayBoxRect(dpi));
		mem.FillSolidRect(box, RGB(160, 160, 165)); // frame of the check box
		CRect inner = box;
		inner.DeflateRect(1, 1);
		mem.FillSolidRect(inner, kButtonFace);
		if (m_settings.FrameDisplay)
		{
			const int s = box.Width();
			CPen checkPen(PS_SOLID, (std::max)(1, ScalePx(2, dpi)), kSpeedThumbFrame);
			CPen* oldCheck = mem.SelectObject(&checkPen);
			mem.MoveTo(box.left + s * 20 / 100, box.top + s * 50 / 100);
			mem.LineTo(box.left + s * 42 / 100, box.top + s * 75 / 100);
			mem.LineTo(box.left + s * 82 / 100, box.top + s * 25 / 100);
			mem.SelectObject(oldCheck);
		}
		CString name;
		name.LoadString(IDS_FRAME_DISPLAY_LABEL);
		CRect label = ButtonRect(CanSim::FrameDisplayLabelRect(dpi));
		mem.SelectObject(&font);
		mem.SetTextColor(RGB(255, 255, 255));
		mem.DrawText(name, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	// 017: simulated time at the right end
	{
		const COLORREF dim = RGB(200, 200, 205);
		CString caption, unit;
		caption.LoadString(IDS_SIM_TIME_CAPTION);
		unit.LoadString(IDS_SIM_TIME_UNIT);
		mem.SelectObject(&font);
		mem.SetTextColor(dim);
		CRect r = ButtonRect(CanSim::SimTimeCaptionRect(dpi));
		mem.DrawText(caption, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		r = ButtonRect(CanSim::SimTimeUnitRect(dpi));
		mem.DrawText(unit, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

		CFont valueFont;
		valueFont.CreateFont(-ScalePx(16, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
		mem.SelectObject(&valueFont);
		mem.SetTextColor(RGB(255, 255, 255));
		r = ButtonRect(CanSim::SimTimeValueRect(dpi));
		const CString value(CanSim::FormatSimTimeUs(m_displaySimUs).c_str());
		mem.DrawText(value, r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		mem.SelectObject(&font);
	}

	// 019: "Speed:" caption left of the slider
	{
		CString caption;
		caption.LoadString(IDS_SPEED_CAPTION);
		CRect r = ButtonRect(CanSim::SpeedCaptionRect(dpi));
		mem.SelectObject(&font);
		mem.SetTextColor(RGB(255, 255, 255));
		mem.DrawText(caption, r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	// 015: speed slider (019: at the right end, half size)
	{
		const COLORREF track = RGB(160, 160, 165);
		mem.FillSolidRect(ButtonRect(CanSim::SpeedTrackRect(dpi)), track);
		const int tickTop = ScalePx(CanSim::kSpeedTickTop, dpi);
		const int tickBottom = ScalePx(CanSim::kSpeedTickBottom, dpi);
		CFont boldFont;
		boldFont.CreateFont(-ScalePx(kSpeedLabelPx, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
		CFont labelFont;
		labelFont.CreateFont(-ScalePx(kSpeedLabelPx, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
		for (int s = CanSim::kMinSpeed; s <= CanSim::kMaxSpeed; ++s)
		{
			const int x = CanSim::SpeedTickX(s, dpi);
			mem.FillSolidRect(x, tickTop, (std::max)(1, ScalePx(1, dpi)), tickBottom - tickTop, track);
			if (!CanSim::IsSpeedLabelShown(s, m_settings.Speed))
				continue; // 019 FR-004: every 5th value and the current one only
			const bool current = s == m_settings.Speed;
			mem.SelectObject(current ? &boldFont : &labelFont);
			mem.SetTextColor(current ? kSpeedCurrentText : RGB(255, 255, 255));
			CString text;
			text.Format(s > 0 ? _T("+%d") : _T("%d"), s);
			CRect label = ButtonRect(CanSim::SpeedLabelRect(s, dpi));
			mem.DrawText(text, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		mem.SelectObject(&font);

		const CRect thumb = ButtonRect(CanSim::SpeedThumbRect(m_settings.Speed, dpi));
		const int border = (std::max)(1, ScalePx(1, dpi)); // 019: half size thumb
		mem.FillSolidRect(thumb, kSpeedThumbFrame);
		CRect inner = thumb;
		inner.DeflateRect(border, border);
		mem.FillSolidRect(inner, m_draggingSpeed ? kButtonPressed : kButtonFace);
	}

	mem.SelectObject(oldFont);
	dc.BitBlt(area.left, area.top, area.Width(), area.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldBmp);
}

void CMFCApplication1Dlg::InvalidateToolbar()
{
	if (GetSafeHwnd() != nullptr)
		InvalidateRect(GetToolbarRect(), FALSE);
}

// 014 FR-004: the menu bar and the toolbar follow the run phase and the
// frame editor (Start is greyed while editing).
void CMFCApplication1Dlg::UpdateSimControls()
{
	UpdateTopMenus();
	InvalidateToolbar();
}

int CMFCApplication1Dlg::ToolbarButtonAt(CPoint point) const
{
	CClientDC dc(const_cast<CMFCApplication1Dlg*>(this));
	return CanSim::FindToolbarButtonAt(point.x, point.y, dc.GetDeviceCaps(LOGPIXELSX));
}

// A press on an enabled button; the command is sent on release over the same
// button (014 FR-003). Returns false when the point is not on such a button.
bool CMFCApplication1Dlg::PressToolbarButton(CPoint point)
{
	const int index = ToolbarButtonAt(point);
	if (index < 0 || !IsToolbarButtonEnabled(index))
		return false;
	m_pressedButton = index;
	m_pressedInside = true;
	SetCapture();
	InvalidateToolbar();
	return true;
}

void CMFCApplication1Dlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (PressToolbarButton(point) || PressFrameDisplayToggle(point) || PressSpeedSlider(point))
		return;
	CDialogEx::OnLButtonDown(nFlags, point);
}

bool CMFCApplication1Dlg::PressFrameDisplayToggle(CPoint point)
{
	CClientDC dc(this);
	if (!CanSim::IsOnFrameDisplayToggle(point.x, point.y, dc.GetDeviceCaps(LOGPIXELSX)))
		return false;
	ToggleFrameDisplay();
	return true;
}

// 015 FR-005: a press on the slider moves the thumb to the nearest tick and
// starts a drag that follows the pointer horizontally.
bool CMFCApplication1Dlg::PressSpeedSlider(CPoint point)
{
	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	if (!CanSim::IsOnSpeedSlider(point.x, point.y, dpi))
		return false;
	m_draggingSpeed = true;
	SetCapture();
	InvalidateToolbar();
	SetSpeed(CanSim::SpeedAtX(point.x, dpi));
	return true;
}

void CMFCApplication1Dlg::EndSpeedDrag()
{
	if (!m_draggingSpeed)
		return;
	m_draggingSpeed = false;
	if (GetCapture() == this)
		ReleaseCapture();
	InvalidateToolbar();
}

// 015: the single place where the speed changes. While running, the time of
// the change becomes the new base so that the clock neither jumps nor goes
// back (FR-007); while paused it applies from the resume (FR-008); +10
// completes the run at once (FR-009, Q1: A).
void CMFCApplication1Dlg::SetSpeed(int speed)
{
	speed = (std::max)(CanSim::kMinSpeed, (std::min)(speed, CanSim::kMaxSpeed));
	if (speed == m_settings.Speed)
		return;
	switch (CanSim::GetSpeedChangeAction(m_runPhase, speed))
	{
	case CanSim::SpeedChangeAction::Rebase:
	{
		const long long t = CurrentSimTimeUs(); // at the old speed
		m_simBaseUs = t;
		QueryPerformanceCounter(&m_replayStartQpc);
		m_settings.Speed = speed;
		break;
	}
	case CanSim::SpeedChangeAction::FinishNow:
		m_settings.Speed = speed;
		EndSpeedDrag(); // no capture is left during the completion message
		InvalidateToolbar();
		FinishReplay();
		return;
	default:
		m_settings.Speed = speed;
		break;
	}
	InvalidateToolbar();
}

void CMFCApplication1Dlg::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_draggingSpeed)
	{
		CClientDC dc(this);
		SetSpeed(CanSim::SpeedAtX(point.x, dc.GetDeviceCaps(LOGPIXELSX)));
	}
	if (m_pressedButton >= 0)
	{
		const bool inside = ToolbarButtonAt(point) == m_pressedButton;
		if (inside != m_pressedInside)
		{
			m_pressedInside = inside;
			InvalidateToolbar();
		}
	}
	CDialogEx::OnMouseMove(nFlags, point);
}

void CMFCApplication1Dlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_draggingSpeed)
	{
		EndSpeedDrag();
		return;
	}
	if (m_pressedButton >= 0)
	{
		const int pressed = m_pressedButton;
		const bool over = ToolbarButtonAt(point) == pressed;
		ReleaseCapture(); // OnCaptureChanged clears the pressed button
		ReleaseToolbarButton();
		if (over && IsToolbarButtonEnabled(pressed))
			PostMessage(WM_COMMAND, MAKEWPARAM(kButtonCommands[pressed], 0), 0);
		return;
	}
	CDialogEx::OnLButtonUp(nFlags, point);
}

void CMFCApplication1Dlg::OnCaptureChanged(CWnd* pWnd)
{
	ReleaseToolbarButton();
	if (m_draggingSpeed)
	{
		m_draggingSpeed = false;
		InvalidateToolbar();
	}
	CDialogEx::OnCaptureChanged(pWnd);
}

void CMFCApplication1Dlg::ReleaseToolbarButton()
{
	if (m_pressedButton < 0)
		return;
	m_pressedButton = -1;
	m_pressedInside = false;
	if (GetCapture() == this)
		ReleaseCapture(); // e.g. the run ended while the button was held
	InvalidateToolbar();
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
	const UINT kRefreshMs = 16; // screen refresh target, about 60 per second (005 FR-008)
	// Feature 010: switching the editor to another ECU closes it, waits this
	// long so that the user notices the switch, then opens it again (FR-010).
	const UINT_PTR kSwitchTimerId = 2;
	const UINT kSwitchDelayMs = 500;
	// Feature 011: an accepted ECU double-click draws that ECU grey this long.
	const UINT_PTR kGrayTimerId = 3;
	const UINT kGrayMs = 500;
	// Top-left of the editor in 96 DPI client pixels: the left edge of the log
	// view, 8 px below the diagram, so that no ECU box is covered (FR-017).
	const int kEditorLeftPx = 150;
	const int kEditorGapPx = 8;

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
}

// Enter and Esc must not close the dialog; only the close box does (004 FR-020).
void CMFCApplication1Dlg::OnOK()
{
}

void CMFCApplication1Dlg::OnCancel()
{
	// Reached through the close box / Alt+F4. A running replay is abandoned
	// without writing any result (004 Edge Cases).
	EndRun();
	CloseFrameEditor(); // 010: a pending switch does not open a new editor
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

	const bool running = InRun();
	const UINT idleOnly = MF_BYCOMMAND | (running ? MF_GRAYED : MF_ENABLED);
	pPopupMenu->EnableMenuItem(ID_FILE_LOAD_PROJECT, idleOnly);
	pPopupMenu->EnableMenuItem(ID_EDIT_SETTINGS, idleOnly);
	pPopupMenu->EnableMenuItem(ID_EDIT_FRAMES, idleOnly);
	pPopupMenu->EnableMenuItem(ID_LOG_VIEW_SIMLOG, idleOnly);                   // 025 FR-008
	pPopupMenu->EnableMenuItem(ID_LOG_CLEAR, idleOnly);                         // 027
	pPopupMenu->EnableMenuItem(ID_ANALYSIS_LATENCY, idleOnly);                  // 028
	pPopupMenu->EnableMenuItem(ID_ANALYSIS_BUSLOAD, idleOnly);
	// 014 FR-004: same states as the toolbar buttons (Start is also greyed
	// while the frame editor is open, 010 FR-013)
	const CanSim::SimCommandStates sim = SimStates();
	pPopupMenu->EnableMenuItem(ID_SIM_START, MF_BYCOMMAND | (sim.Start ? MF_ENABLED : MF_GRAYED));
	pPopupMenu->EnableMenuItem(ID_SIM_PAUSE, MF_BYCOMMAND | (sim.Pause ? MF_ENABLED : MF_GRAYED));
	pPopupMenu->EnableMenuItem(ID_SIM_STOP, MF_BYCOMMAND | (sim.Stop ? MF_ENABLED : MF_GRAYED));
}

void CMFCApplication1Dlg::OnFileLoadProject()
{
	if (InRun())
		return;
	CloseFrameEditor(); // 010 FR-014: the editor refers to the current project
	CloseResultWindows(); // 025 FR-009: the log belongs to the current project

	CFileDialog dlg(TRUE, _T("canprj"), nullptr, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		LoadRcString(IDS_PROJECT_FILTER), this);
	if (dlg.DoModal() != IDOK)
		return;
	LoadProjectFrom(dlg.GetPathName());
}

// 024: shared by the menu, the toolbar and the project given at startup.
// On any error the current project (and the title) is left untouched.
bool CMFCApplication1Dlg::LoadProjectFrom(const CString& path)
{
	CString message;
	if (!CanSim::HasProjectExtension(std::wstring((LPCTSTR)path)))
	{
		message.Format(LoadRcString(IDS_BAD_EXTENSION), (LPCTSTR)path);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return false;
	}

	const CanSim::ConfigLoadResult loaded = CanSim::LoadConfig(std::wstring((LPCTSTR)path));
	if (!loaded.Success)
	{
		// The current project is left untouched (004 FR-010).
		message.Format(LoadRcString(IDS_LOAD_ERROR), (LPCTSTR)ToCString(loaded.ErrorMessage));
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return false;
	}

	m_settings = loaded.Project;
	InvalidateToolbar(); // 015 FR-010: the thumb shows the loaded speed
	m_definitions = loaded.Config;
	SetProjectPath(path);
	SetEcuCount(m_settings.EcuCount);
	return true;
}

// 024: the only place where the current project file changes; the title bar
// always shows it (empty path = no project file yet).
void CMFCApplication1Dlg::SetProjectPath(const CString& path)
{
	if (path.CompareNoCase(m_projectPath) != 0)
		CloseResultWindows(); // 025 FR-009: the log shown belongs to the previous file
	m_projectPath = path;
	const std::wstring title = CanSim::ComposeWindowTitle(std::wstring((LPCTSTR)m_baseTitle),
		std::wstring((LPCTSTR)LoadRcString(IDS_PROJECT_PATH_LABEL)), std::wstring((LPCTSTR)path));
	SetWindowText(title.c_str());
}

// 024: loads the project given on the command line once the window is shown,
// so that an error message appears over the main window (research decision 6).
LRESULT CMFCApplication1Dlg::OnLoadStartupProject(WPARAM, LPARAM)
{
	CString arg = m_startupProject;
	m_startupProject.Empty();
	if (arg.IsEmpty())
		return 0;

	// Relative and 8.3 paths become the absolute long path (spec FR-007).
	CString path = arg;
	const DWORD fullLength = ::GetFullPathNameW(arg, 0, nullptr, nullptr);
	if (fullLength > 0)
	{
		std::vector<wchar_t> full(fullLength);
		if (::GetFullPathNameW(arg, fullLength, full.data(), nullptr) > 0)
			path = full.data();
	}
	const DWORD longLength = ::GetLongPathNameW(path, nullptr, 0);
	if (longLength > 0)
	{
		std::vector<wchar_t> longPath(longLength);
		if (::GetLongPathNameW(path, longPath.data(), longLength) > 0)
			path = longPath.data();
	}

	LoadProjectFrom(path); // a failure keeps the default project (FR-006)
	return 0;
}

// 025: "Log > Simulation log" shows "<project file>.log.csv" read-only, or
// says why there is nothing to show (spec FR-004..FR-006).
void CMFCApplication1Dlg::OnLogViewSimLog()
{
	if (InRun())
		return; // FR-008: the menu is greyed; guard other routes too
	if (m_logViewer != nullptr)
	{
		m_logViewer->SetForegroundWindow(); // same file: only bring it to the front
		return;
	}
	if (m_projectPath.IsEmpty())
	{
		AfxMessageBox(LoadRcString(IDS_LOG_NONE_NOPROJECT), MB_OK | MB_ICONINFORMATION);
		return;
	}

	const CString logPath = CanSim::LogPathFor(std::wstring((LPCTSTR)m_projectPath)).c_str();
	const DWORD attributes = ::GetFileAttributes(logPath);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		const int slash = (std::max)(m_projectPath.ReverseFind(_T('\\')), m_projectPath.ReverseFind(_T('/')));
		CString message;
		message.Format(LoadRcString(IDS_LOG_NONE), (LPCTSTR)m_projectPath.Mid(slash + 1));
		AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
		return;
	}

	CLogViewDlg* viewer = new CLogViewDlg(this, logPath);
	if (!viewer->Open())
	{
		delete viewer; // no window was created
		CString message;
		message.Format(LoadRcString(IDS_LOG_READ_ERROR), (LPCTSTR)logPath);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return;
	}
	m_logViewer = viewer;
}

namespace
{
	// File names (without the folder) one per line, for the 027 messages.
	CString JoinFileNames(const std::vector<std::wstring>& paths)
	{
		CString names;
		for (const std::wstring& path : paths)
		{
			const std::size_t slash = path.find_last_of(L"\\/");
			if (!names.IsEmpty())
				names += _T("\n");
			names += path.substr(slash == std::wstring::npos ? 0 : slash + 1).c_str();
		}
		return names;
	}
}

// 027: "Log > Clear log" deletes the result files of the current project
// after a confirmation (Q1), all three of them (Q2), and empties the log view
// when anything was deleted (Q3).
void CMFCApplication1Dlg::OnLogClear()
{
	if (InRun())
		return; // the menu is greyed; guard other routes too
	if (m_projectPath.IsEmpty())
	{
		AfxMessageBox(LoadRcString(IDS_LOG_NONE_NOPROJECT), MB_OK | MB_ICONINFORMATION); // US2-1
		return;
	}

	const std::vector<std::wstring> existing = CanSim::ExistingResultFiles(std::wstring((LPCTSTR)m_projectPath));
	CString message;
	if (existing.empty())
	{
		const int slash = (std::max)(m_projectPath.ReverseFind(_T('\\')), m_projectPath.ReverseFind(_T('/')));
		message.Format(LoadRcString(IDS_LOG_NONE), (LPCTSTR)m_projectPath.Mid(slash + 1)); // US2-2
		AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
		return;
	}

	message.Format(LoadRcString(IDS_LOG_CLEAR_CONFIRM), (LPCTSTR)JoinFileNames(existing));
	if (AfxMessageBox(message, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return; // nothing changes

	CloseResultWindows(); // FR-007: it may be showing a file about to go
	const CanSim::LogClearResult result = CanSim::DeleteResultFiles(existing);
	if (!result.Deleted.empty())
		ClearLog(); // FR-008: the rows shown came from the deleted run

	if (result.Failed.empty())
	{
		message.Format(LoadRcString(IDS_LOG_CLEAR_DONE), (LPCTSTR)JoinFileNames(result.Deleted));
		AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		message.Format(LoadRcString(IDS_LOG_CLEAR_FAILED), (LPCTSTR)JoinFileNames(result.Failed)); // US2-3
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
	}
}

LRESULT CMFCApplication1Dlg::OnLogViewerClosed(WPARAM, LPARAM)
{
	m_logViewer = nullptr;
	return 0;
}

void CMFCApplication1Dlg::CloseLogViewer()
{
	if (m_logViewer != nullptr)
		m_logViewer->DestroyWindow(); // the closed notice clears m_logViewer
}

// 028 FR-008: every window that shows the result files of the current
// project; closed whenever those files or the project change.
void CMFCApplication1Dlg::CloseResultWindows()
{
	CloseLogViewer();
	if (m_latencyDlg != nullptr)
		m_latencyDlg->DestroyWindow(); // the closed notices clear the pointers
	if (m_busLoadDlg != nullptr)
		m_busLoadDlg->DestroyWindow();
}

// ---- feature 028: Analysis menu ----

// Reads the frame log of the current project, or tells why it cannot
// (the same notices as "Log > Simulation log", 025).
bool CMFCApplication1Dlg::LoadAnalysisRecords(std::vector<CanSim::LogRecord>& records)
{
	if (m_projectPath.IsEmpty())
	{
		AfxMessageBox(LoadRcString(IDS_LOG_NONE_NOPROJECT), MB_OK | MB_ICONINFORMATION);
		return false;
	}
	const CString logPath = CanSim::LogPathFor(std::wstring((LPCTSTR)m_projectPath)).c_str();
	const DWORD attributes = ::GetFileAttributes(logPath);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		CString message;
		message.Format(LoadRcString(IDS_LOG_NONE), (LPCTSTR)ProjectFileName());
		AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
		return false;
	}
	std::string error;
	bool ok = false;
	{
		CWaitCursor wait;
		ok = CanSim::ReadLogRecords(std::wstring((LPCTSTR)logPath), records, error);
	}
	if (!ok)
	{
		CString message;
		message.Format(LoadRcString(IDS_LOG_READ_ERROR), (LPCTSTR)logPath);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
	}
	return ok;
}

CString CMFCApplication1Dlg::ProjectFileName() const
{
	const int slash = (std::max)(m_projectPath.ReverseFind(_T('\\')), m_projectPath.ReverseFind(_T('/')));
	return m_projectPath.Mid(slash + 1);
}

void CMFCApplication1Dlg::OnAnalysisLatency()
{
	if (InRun())
		return; // the menu is greyed; guard other routes too
	if (m_latencyDlg != nullptr)
	{
		m_latencyDlg->SetForegroundWindow();
		return;
	}
	std::vector<CanSim::LogRecord> records;
	if (!LoadAnalysisRecords(records))
		return;
	CLatencyDlg* dlg = new CLatencyDlg(this, ProjectFileName(), CanSim::AnalyzeLatency(records));
	if (!dlg->Open())
	{
		delete dlg; // no window was created
		return;
	}
	m_latencyDlg = dlg;
}

void CMFCApplication1Dlg::OnAnalysisBusLoad()
{
	if (InRun())
		return;
	if (m_busLoadDlg != nullptr)
	{
		m_busLoadDlg->SetForegroundWindow();
		return;
	}
	std::vector<CanSim::LogRecord> records;
	if (!LoadAnalysisRecords(records))
		return;
	CBusLoadDlg* dlg = new CBusLoadDlg(this, ProjectFileName(), CanSim::AnalyzeBusLoad(records));
	if (!dlg->Open())
	{
		delete dlg;
		return;
	}
	m_busLoadDlg = dlg;
}

LRESULT CMFCApplication1Dlg::OnLatencyClosed(WPARAM, LPARAM)
{
	m_latencyDlg = nullptr;
	return 0;
}

LRESULT CMFCApplication1Dlg::OnBusLoadClosed(WPARAM, LPARAM)
{
	m_busLoadDlg = nullptr;
	return 0;
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
	if (InRun())
		return; // 007 FR-001: the File menu is locked while running

	CString path;
	if (!AskSavePath(path))
		return;
	SaveProjectTo(path);
}

// 023: the toolbar Save overwrites the project file without asking; only
// when there is none yet it asks where to save (Q1). The menu item above
// keeps asking every time.
void CMFCApplication1Dlg::OnFileSaveOverwrite()
{
	if (InRun())
		return; // 023 FR-006: the button is greyed; guard other routes too

	CString path = m_projectPath;
	if (path.IsEmpty() && !AskSavePath(path))
		return;
	SaveProjectTo(path);
}

// On failure the current project file stays as it was (023 FR-005).
bool CMFCApplication1Dlg::SaveProjectTo(const CString& path)
{
	if (!CanSim::SaveProject(std::wstring((LPCTSTR)path), m_settings, m_definitions))
	{
		CString message;
		const DWORD attributes = ::GetFileAttributes(path);
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0)
		{
			// 023 revision 1 B: name the file and say why it could not be written
			const int slash = (std::max)(path.ReverseFind(_T('\\')), path.ReverseFind(_T('/')));
			const CString name = path.Mid(slash + 1);
			message.Format(LoadRcString(IDS_SAVE_READONLY), (LPCTSTR)name, (LPCTSTR)path);
		}
		else
			message.Format(LoadRcString(IDS_SAVE_ERROR), (LPCTSTR)path);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		return false;
	}
	SetProjectPath(path); // 024: the title shows the file just written
	return true;
}

void CMFCApplication1Dlg::OnEditSettings()
{
	if (InRun())
		return;
	CloseFrameEditor(); // 010 FR-014: ECU count and bit rate may change
	CloseResultWindows(); // 025 FR-009

	CSettingsDlg dlg(m_settings, m_definitions.BitRateBps, this);
	if (dlg.DoModal() != IDOK)
		return; // cancelled: nothing changes (004 FR-007, 003 FR-010)

	m_settings.EcuCount = dlg.Settings.EcuCount;
	m_settings.DurationSec = dlg.Settings.DurationSec;
	m_settings.Speed = dlg.Settings.Speed;
	m_definitions.BitRateBps = dlg.BitRateBps;
	SetEcuCount(m_settings.EcuCount);
}

// Data frame editor (feature 006). Edits go straight into the project's
// definitions; they are written to the file by "Save project". Since feature
// 010 the window is modeless: already open, it is only brought to the front;
// while a switch is pending nothing happens (it opens shortly).
void CMFCApplication1Dlg::OnEditFrames()
{
	if (InRun())
		return; // 006 FR-001: not while a simulation runs
	if (m_frameEditor != nullptr)
	{
		m_frameEditor->SetForegroundWindow();
		return;
	}
	if (m_pendingEcu != 0)
		return;
	OpenFrameEditor(1); // 010 FR-008: ECU 01 as before
}

// ---- feature 010: ECU double-click and the modeless editor ----

bool CMFCApplication1Dlg::IsEditingFrames() const
{
	return m_frameEditor != nullptr || m_pendingEcu != 0;
}

void CMFCApplication1Dlg::OpenFrameEditor(int ecu)
{
	// m_ecuCount (not m_settings.EcuCount) so that the node list matches the
	// diagram that was double-clicked.
	CFrameEditDlg* editor = new CFrameEditDlg(m_definitions, m_ecuCount, ecu, this);
	if (!editor->Create(IDD_FRAME_EDIT, this))
	{
		delete editor;
		return;
	}
	m_frameEditor = editor;

	// FR-017: below the diagram, kept inside the monitor's work area.
	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	CPoint pos(ScalePx(kEditorLeftPx, dpi), ContentTop() + ScalePx(CanSim::kDiagramHeight + kEditorGapPx, dpi));
	ClientToScreen(&pos);
	CRect rect;
	editor->GetWindowRect(&rect);
	MONITORINFO mi = { sizeof(mi) };
	if (::GetMonitorInfo(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi))
	{
		const CRect work(mi.rcWork);
		pos.x = (std::max)((std::min)(pos.x, work.right - rect.Width()), work.left);
		pos.y = (std::max)((std::min)(pos.y, work.bottom - rect.Height()), work.top);
	}
	editor->SetWindowPos(nullptr, pos.x, pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	editor->ShowWindow(SW_SHOW);
	UpdateSimControls(); // 014: Start is greyed while editing
}

void CMFCApplication1Dlg::CloseFrameEditor()
{
	KillTimer(kSwitchTimerId);
	m_pendingEcu = 0;
	if (m_frameEditor != nullptr)
		m_frameEditor->DestroyWindow(); // the closed notice clears m_frameEditor
	UpdateSimControls();
}

// 022: the ECU label was edited in the frame editor
LRESULT CMFCApplication1Dlg::OnEcuLabelChanged(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	InvalidateRect(GetDiagramRect(), FALSE);
	return 0;
}

LRESULT CMFCApplication1Dlg::OnFrameEditorClosed(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	m_frameEditor = nullptr;
	UpdateSimControls();
	return 0;
}

void CMFCApplication1Dlg::OnDestroy()
{
	// Closing the main window while a switch is pending must not open a new
	// editor (spec Edge Cases).
	CloseFrameEditor();
	CloseResultWindows(); // 025
	KillTimer(kGrayTimerId); // 011
	CDialogEx::OnDestroy();
}

// ---- feature 011: grey ECU after an accepted double-click ----

// Only the last ECU is grey; a new double-click (even on the same ECU)
// restarts the 500 ms (FR-004, spec Edge Cases).
void CMFCApplication1Dlg::StartGray(int ecu)
{
	m_grayEcu = ecu;
	SetTimer(kGrayTimerId, kGrayMs, nullptr);
	InvalidateRect(GetDiagramRect(), FALSE);
	UpdateWindow();
}

void CMFCApplication1Dlg::ClearGray()
{
	KillTimer(kGrayTimerId);
	if (m_grayEcu == 0)
		return;
	m_grayEcu = 0;
	if (GetSafeHwnd() != nullptr)
		InvalidateRect(GetDiagramRect(), FALSE);
}

void CMFCApplication1Dlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// 014: the second click of a quick double click on the toolbar is a press
	if (point.y < ContentTop())
	{
		if (!PressToolbarButton(point) && !PressFrameDisplayToggle(point))
			PressSpeedSlider(point);
		return;
	}
	CDialogEx::OnLButtonDblClk(nFlags, point);
	if (InRun())
		return; // FR-006

	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	const int ecu = CanSim::FindEcuAt(CanSim::ComputeBusDiagramLayout(m_ecuCount), point.x, point.y - ContentTop(), dpi);
	if (ecu == 0)
		return; // FR-005: not on an ECU box

	// 011 FR-001: show which ECU was taken before the editor is opened, so
	// that the grey appears at once; the 010 behaviour below is unchanged.
	StartGray(ecu);

	if (m_frameEditor != nullptr)
	{
		if (m_frameEditor->CurrentEcu() == ecu)
		{
			m_frameEditor->SetForegroundWindow(); // FR-011: same ECU, not reopened
			return;
		}
		// FR-010: close, then open the other ECU after a short pause.
		CloseFrameEditor();
		m_pendingEcu = ecu;
		SetTimer(kSwitchTimerId, kSwitchDelayMs, nullptr);
		UpdateSimControls();
		return;
	}
	if (m_pendingEcu != 0)
	{
		m_pendingEcu = ecu; // FR-012: the latest one wins, the wait is not extended
		return;
	}
	OpenFrameEditor(ecu); // FR-001, FR-002
}

// 017 FR-003/FR-004: the frame display check box. Any time, also while
// running or paused: only the highlight follows it.
void CMFCApplication1Dlg::ToggleFrameDisplay()
{
	m_settings.FrameDisplay = !m_settings.FrameDisplay;
	InvalidateRect(GetDiagramRect(), FALSE);
	InvalidateToolbar();
}

// 017 FR-007: the simulated time shown at the right end of the toolbar.
void CMFCApplication1Dlg::SetDisplaySimUs(long long us)
{
	if (us == m_displaySimUs)
		return;
	m_displaySimUs = us;
	if (GetSafeHwnd() != nullptr)
	{
		CClientDC dc(this);
		const CanSim::DiagramRect r = CanSim::SimTimeAreaRect(dc.GetDeviceCaps(LOGPIXELSX));
		InvalidateRect(CRect(r.Left, r.Top, r.Right, r.Bottom), FALSE);
	}
}

bool CMFCApplication1Dlg::WriteResults(const std::vector<CanSim::LogRow>& rows,
	const CanSim::Network& runNetwork, const CanSim::Network& endState,
	CString& logPath, CString& summaryPath, CString& startupPath)
{
	// Same file names as before feature 004: <project path>.log.csv etc.
	// 027: the names come from the same place as the ones "Clear log" deletes
	// (the first is also the one the log viewer looks for, 025).
	const std::vector<std::wstring> paths = CanSim::ResultFilePaths(std::wstring((LPCTSTR)m_projectPath));
	logPath = paths[0].c_str();
	summaryPath = paths[1].c_str();
	startupPath = paths[2].c_str();

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
	if (m_runPhase == CanSim::RunPhase::Running)
		return;
	if (m_runPhase == CanSim::RunPhase::Paused)
	{
		ResumeReplay(); // 014 FR-006: no new run while paused
		return;
	}
	if (IsEditingFrames())
		return; // 010 FR-013: the menu item is greyed; guard other routes too

	// 025 FR-009: the log shown would be replaced by this run.
	CloseResultWindows();

	// The results are written next to the project file, so it needs a place
	// first (004 FR-017).
	if (m_projectPath.IsEmpty())
	{
		CString path;
		if (!AskSavePath(path) || !SaveProjectTo(path))
			return;
	}

	// 009 FR-010: a new run starts with an empty log view. Cleared before
	// m_runRows is replaced, since the rows refer to it. 012 FR-005: the
	// displays are reset and painted before anything of the run is set up,
	// so the previous result is gone while the run is being computed.
	ResetDisplaysForRun();

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
		m_logLines = CanSim::BuildLogLines(m_runRows, m_runEndState, m_settings.DurationSec * 1000000LL);
	}

	// Speed +10 does not wait at all (005 FR-004).
	if (m_settings.Speed >= CanSim::kInstantSpeed)
	{
		FinishReplay();
		return;
	}

	// Otherwise the simulated time runs at real time x 2^Speed, with or
	// without the frame display, and even when no frame is sent at all
	// (005 FR-003, FR-006, FR-007).
	m_events = CanSim::BuildFrameEvents(m_runRows);
	m_cursor.reset(new CanSim::ActiveFrameCursor(m_events));
	m_active = CanSim::ActiveFrames();
	m_prevSimUs = 0;
	m_curSimUs = 0;
	m_simBaseUs = 0;
	QueryPerformanceFrequency(&m_qpcFrequency);
	QueryPerformanceCounter(&m_replayStartQpc);
	m_flowElapsedUs = 0; // 018: the flow animation starts a new round
	m_flowLastQpc = m_replayStartQpc;
	m_flowHold.Clear();
	m_runPhase = CanSim::RunPhase::Running;
	UpdateSimControls();
	m_replayTimer = SetTimer(kReplayTimerId, kRefreshMs, nullptr);
	InvalidateRect(GetDiagramRect(), FALSE);
}

// 012 FR-001..FR-003: empty log view scrolled to the top, no grey ECU and no
// highlight. UpdateWindow paints it now; otherwise it would only show after
// the synchronous computation of the run.
void CMFCApplication1Dlg::ResetDisplaysForRun()
{
	SetDisplaySimUs(0); // 017 FR-007: a new run starts at 0
	ClearLog();
	ClearGray();
	m_active = CanSim::ActiveFrames();
	InvalidateRect(GetDiagramRect(), FALSE);
	InvalidateRect(GetLogTextRect(), FALSE);
	UpdateWindow();
}

// 007 FR-001/FR-002, 025 FR-008: while running, File (0), Edit (1),
// Analysis (3) and Log (4) are greyed as a whole so that they cannot be
// opened by mouse or keyboard. Simulation (2) stays available.
void CMFCApplication1Dlg::UpdateTopMenus()
{
	CMenu* menu = GetMenu();
	if (menu == nullptr)
		return;
	const UINT state = MF_BYPOSITION | (InRun() ? MF_GRAYED : MF_ENABLED);
	menu->EnableMenuItem(0, state);
	menu->EnableMenuItem(1, state);
	menu->EnableMenuItem(3, state);
	menu->EnableMenuItem(4, state);
	DrawMenuBar();
}

void CMFCApplication1Dlg::StopReplayTimer()
{
	if (m_replayTimer != 0)
	{
		KillTimer(m_replayTimer);
		m_replayTimer = 0;
	}
}

// Leaves the running state and forgets the on-screen progress.
void CMFCApplication1Dlg::EndRun()
{
	StopReplayTimer();
	m_runPhase = CanSim::RunPhase::Idle;
	m_simBaseUs = 0;
	ReleaseToolbarButton();
	EndSpeedDrag();
	UpdateSimControls();
	m_cursor.reset();
	m_events.clear();
	m_active = CanSim::ActiveFrames();
	InvalidateRect(GetDiagramRect(), FALSE);
}

// Writes the full result of the current run and returns to Idle.
void CMFCApplication1Dlg::FinishReplay()
{
	SetDisplaySimUs(m_settings.DurationSec * 1000000LL); // 017: the end of the run
	EndRun();
	SetLogShown(m_logLines.size()); // 009 FR-005: every row once the run is complete

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

// Simulated time reached by now: based on the real elapsed time, never on the
// number of timer ticks (005 FR-005). Clamped to the simulation duration.
long long CMFCApplication1Dlg::CurrentSimTimeUs() const
{
	if (m_runPhase == CanSim::RunPhase::Paused)
		return m_curSimUs; // 014 FR-005: the clock is stopped
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	const long long ticks = now.QuadPart - m_replayStartQpc.QuadPart;
	const long long elapsedUs = (m_qpcFrequency.QuadPart > 0)
		? static_cast<long long>(static_cast<double>(ticks) * 1000000.0 / static_cast<double>(m_qpcFrequency.QuadPart))
		: 0;
	const long long endUs = m_settings.DurationSec * 1000000LL;
	return (std::min)(CanSim::ResumedSimTimeUs(m_simBaseUs, elapsedUs, m_settings.Speed), endUs);
}

void CMFCApplication1Dlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == kGrayTimerId)
	{
		ClearGray(); // 011 FR-002
		return;
	}
	if (nIDEvent == kSwitchTimerId)
	{
		// 010 FR-010: end of the pause after closing the editor.
		KillTimer(kSwitchTimerId);
		const int ecu = m_pendingEcu;
		m_pendingEcu = 0;
		if (ecu != 0 && !InRun() && m_frameEditor == nullptr)
			OpenFrameEditor(ecu);
		UpdateSimControls();
		return;
	}
	if (nIDEvent != kReplayTimerId || m_runPhase != CanSim::RunPhase::Running)
	{
		CDialogEx::OnTimer(nIDEvent);
		return;
	}

	AdvanceReplay();
}

// One refresh of the replay: brings the display up to the current simulated
// time. Returns false when the end was reached and the run is finished.
bool CMFCApplication1Dlg::AdvanceReplay()
{
	// 018 FR-006/FR-007: the flow animation clock runs in real time while the
	// run is Running only (no refresh while paused), whatever the speed.
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		if (m_qpcFrequency.QuadPart > 0)
			m_flowElapsedUs += static_cast<long long>(static_cast<double>(now.QuadPart - m_flowLastQpc.QuadPart) *
				1000000.0 / static_cast<double>(m_qpcFrequency.QuadPart));
		m_flowLastQpc = now;
	}
	// Highlight every frame that was on the bus since the previous refresh
	// (005 FR-009).
	m_curSimUs = CurrentSimTimeUs();
	m_active = m_cursor->Collect(m_prevSimUs, m_curSimUs);
	m_flowHold.Update(m_active.Flows, m_flowElapsedUs); // 018 revision 1: paths stay 0.3 s
	m_prevSimUs = m_curSimUs;
	ShowLogUpTo(m_curSimUs); // 009: rows appear on the same clock, display on or off
	SetDisplaySimUs(m_curSimUs); // 017

	if (m_curSimUs >= m_settings.DurationSec * 1000000LL)
	{
		FinishReplay();
		return false;
	}
	InvalidateRect(GetDiagramRect(), FALSE);
	return true;
}

// 014 FR-005: the display is brought up to now, then the clock stops. The
// highlight and the log stay as they are until Start (resume) or Stop.
void CMFCApplication1Dlg::OnSimPause()
{
	if (m_runPhase != CanSim::RunPhase::Running)
		return;
	if (!AdvanceReplay())
		return; // the end was reached: finished as usual
	StopReplayTimer();
	m_runPhase = CanSim::RunPhase::Paused;
	UpdateSimControls();
	UpdateWindow();
}

// 014 FR-006: continues from the simulated time of the pause at the same
// speed; nothing is recomputed and the displays are kept.
void CMFCApplication1Dlg::ResumeReplay()
{
	m_simBaseUs = m_curSimUs;
	m_prevSimUs = m_curSimUs;
	QueryPerformanceCounter(&m_replayStartQpc);
	m_flowLastQpc = m_replayStartQpc; // 018: the pause does not count
	m_runPhase = CanSim::RunPhase::Running;
	m_replayTimer = SetTimer(kReplayTimerId, kRefreshMs, nullptr);
	UpdateSimControls();
}

// Stops at the current simulated time. The run is repeated up to that time
// so that the result holds exactly the frames whose transmission completed
// by then (005 FR-012).
void CMFCApplication1Dlg::OnSimStop()
{
	if (!InRun())
		return;

	const long long stopUs = CurrentSimTimeUs();
	SetDisplaySimUs(stopUs); // 017: same value as the stop message
	EndRun();
	UpdateWindow();

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

// ---- feature 009: log view under the bus diagram ----

namespace
{
	// 96 dpi layout (009 contracts/ui-log-view.md, Doc/ログ表示エリアイメージ図.jpg).
	// The position and the border are in CanSim (ScaleLogViewRect /
	// ScaleLogFrameRect) so that checks.exe can verify them (feature 011).
	const int kLogScrollBarPx = 17;
	const int kLogLinePx = 18;
	const int kLogPaddingPx = 6;
	const int kLogWheelLines = 3;
	const COLORREF kLogBack = RGB(230, 230, 230);
	const COLORREF kLogText = RGB(0, 0, 0);

	int ScaleForDialog(const CWnd* wnd, int px)
	{
		CClientDC dc(const_cast<CWnd*>(wnd));
		return MulDiv(px, dc.GetDeviceCaps(LOGPIXELSX), 96);
	}

	CRect ToCRect(const CanSim::DiagramRect& r)
	{
		return CRect(r.Left, r.Top, r.Right, r.Bottom);
	}
}

CRect CMFCApplication1Dlg::GetLogRect() const
{
	CClientDC dc(const_cast<CMFCApplication1Dlg*>(this));
	CRect r = ToCRect(CanSim::ScaleLogViewRect(dc.GetDeviceCaps(LOGPIXELSX)));
	r.OffsetRect(0, ContentTop()); // 014: below the toolbar
	return r;
}

CRect CMFCApplication1Dlg::GetLogTextRect() const
{
	CRect r = GetLogRect();
	r.right -= ScaleForDialog(this, kLogScrollBarPx);
	return r;
}

int CMFCApplication1Dlg::LogLineHeight() const
{
	return ScaleForDialog(this, kLogLinePx);
}

int CMFCApplication1Dlg::LogVisibleRows() const
{
	const CRect r = GetLogTextRect();
	const int rows = (r.Height() - 2 * ScaleForDialog(this, kLogPaddingPx / 2)) / LogLineHeight();
	return rows > 0 ? rows : 1;
}

// One text line; only built for the visible rows (009 research.md decision 3).
CString CMFCApplication1Dlg::FormatLogLine(const CanSim::LogLine& line) const
{
	CString text;
	if (line.Kind == CanSim::LogLineKind::State)
	{
		UINT stateId = IDS_LOG_STATE_NORMAL;
		switch (static_cast<CanSim::StartupState>(line.Index))
		{
		case CanSim::StartupState::Initializing: stateId = IDS_LOG_STATE_INIT; break;
		case CanSim::StartupState::BusSyncDetecting: stateId = IDS_LOG_STATE_SYNC; break;
		default: break;
		}
		text.Format(_T("%10I64d us  ECU %02d  %s"), line.TimeUs, line.NodeId, (LPCTSTR)LoadRcString(stateId));
		return text;
	}

	if (line.Index < 0 || static_cast<std::size_t>(line.Index) >= m_runRows.size())
		return text;
	const CanSim::LogRow& row = m_runRows[static_cast<std::size_t>(line.Index)];
	CString rx = _T("-");
	if (row.RxNodeId >= 0)
		rx.Format(_T("ECU %02d"), row.RxNodeId);
	const UINT statusId = row.Status == CanSim::kStatusOverwrite ? IDS_LOG_FRAME_DROPPED
		: row.Status == "ARBITRATION_LOST_RETRY" ? IDS_LOG_FRAME_RETRY
		: IDS_LOG_FRAME_OK;
	text.Format(_T("%10I64d us  ECU %02d -> %-6s  0x%03X  DLC %u  %s"), line.TimeUs, row.TxNodeId, (LPCTSTR)rx,
		row.CanId, row.Dlc, (LPCTSTR)LoadRcString(statusId));
	return text;
}

void CMFCApplication1Dlg::DrawLogView(CDC& dc)
{
	const CRect area = GetLogTextRect();
	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, area.Width(), area.Height());
	CBitmap* oldBmp = mem.SelectObject(&bmp);
	mem.FillSolidRect(0, 0, area.Width(), area.Height(), kLogBack); // 009 FR-001
	mem.SetBkMode(TRANSPARENT);
	mem.SetTextColor(kLogText);
	CFont* oldFont = mem.SelectObject(&m_logFont);

	const int lineHeight = LogLineHeight();
	const int padding = ScaleForDialog(this, kLogPaddingPx);
	const std::size_t visible = static_cast<std::size_t>(LogVisibleRows());
	for (std::size_t i = 0; i < visible && m_logTop + i < m_logShown; ++i)
	{
		CRect lineRect(padding, padding / 2 + static_cast<int>(i) * lineHeight, area.Width() - padding,
			padding / 2 + static_cast<int>(i + 1) * lineHeight);
		// One row is one line: cut at the right edge, never wrapped (009 Edge Cases).
		mem.DrawText(FormatLogLine(m_logLines[m_logTop + i]), lineRect,
			DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
	}

	mem.SelectObject(oldFont);
	dc.BitBlt(area.left, area.top, area.Width(), area.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldBmp);
}

// 011 FR-009: border around the grey area and the scroll bar, outside both so
// that redrawing the rows never touches it.
void CMFCApplication1Dlg::DrawLogFrame(CDC& dc)
{
	CRect frame = ToCRect(CanSim::ScaleLogFrameRect(dc.GetDeviceCaps(LOGPIXELSX)));
	frame.OffsetRect(0, ContentTop()); // 014: below the toolbar
	CPen pen(PS_SOLID, (std::max)(1, ScaleForDialog(this, 1)), kAreaFrameColor);
	CPen* oldPen = dc.SelectObject(&pen);
	CGdiObject* oldBrush = dc.SelectStockObject(NULL_BRUSH);
	dc.Rectangle(frame);
	dc.SelectObject(oldBrush);
	dc.SelectObject(oldPen);

	// 020: "Event & log view" label just above the border, at its left edge
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	CRect label = ToCRect(CanSim::ScaleLogLabelRect(dpi));
	label.OffsetRect(0, ContentTop());
	DrawAreaLabel(dc, label, IDS_LOG_VIEW_LABEL, dpi);
}

// 020: name of an area in small dark grey text.
void CMFCApplication1Dlg::DrawAreaLabel(CDC& dc, const CRect& rect, UINT textId, int dpi)
{
	CFont font;
	font.CreateFont(-MulDiv(12, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
	CFont* oldFont = dc.SelectObject(&font);
	const int oldMode = dc.SetBkMode(TRANSPARENT);
	const COLORREF oldColor = dc.SetTextColor(RGB(90, 90, 90));
	CString text;
	text.LoadString(textId);
	CRect r = rect;
	dc.DrawText(text, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	dc.SetTextColor(oldColor);
	dc.SetBkMode(oldMode);
	dc.SelectObject(oldFont);
}

void CMFCApplication1Dlg::ClearLog()
{
	m_logLines.clear();
	m_logShown = 0;
	m_logTop = 0;
	UpdateLogScroll();
	InvalidateRect(GetLogTextRect(), FALSE);
}

// Shows every row whose time has come (009 FR-005).
void CMFCApplication1Dlg::ShowLogUpTo(long long simUs)
{
	std::size_t shown = m_logShown;
	while (shown < m_logLines.size() && m_logLines[shown].TimeUs <= simUs)
		++shown;
	if (shown != m_logShown)
		SetLogShown(shown);
}

// While the last row is visible, adding rows scrolls so that the newest row
// stays at the bottom (009 FR-007). While older rows are being read, the view
// does not move (009 FR-009).
void CMFCApplication1Dlg::SetLogShown(std::size_t shown)
{
	const std::size_t visible = static_cast<std::size_t>(LogVisibleRows());
	const bool following = m_logTop + visible >= m_logShown;
	m_logShown = (std::min)(shown, m_logLines.size());
	if (following)
		m_logTop = m_logShown > visible ? m_logShown - visible : 0;
	UpdateLogScroll();
	InvalidateRect(GetLogTextRect(), FALSE);
}

void CMFCApplication1Dlg::ScrollLogTo(long long top)
{
	const long long visible = LogVisibleRows();
	const long long maxTop = (std::max)(0LL, static_cast<long long>(m_logShown) - visible);
	top = (std::max)(0LL, (std::min)(top, maxTop));
	if (static_cast<std::size_t>(top) == m_logTop)
		return;
	m_logTop = static_cast<std::size_t>(top);
	UpdateLogScroll();
	InvalidateRect(GetLogTextRect(), FALSE);
}

// 009 FR-008: the scroll bar covers every shown row; it is disabled while the
// rows fit into the area.
void CMFCApplication1Dlg::UpdateLogScroll()
{
	CScrollBar* bar = static_cast<CScrollBar*>(GetDlgItem(IDC_SCROLL_LOG));
	if (bar == nullptr)
		return;
	const int visible = LogVisibleRows();
	SCROLLINFO si = {};
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	si.nMin = 0;
	si.nMax = m_logShown > 0 ? static_cast<int>(m_logShown) - 1 : 0;
	si.nPage = static_cast<UINT>(visible);
	si.nPos = static_cast<int>(m_logTop);
	bar->SetScrollInfo(&si, TRUE);
	bar->EnableScrollBar(static_cast<int>(m_logShown) > visible ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH);
}

// The log view stays usable while a simulation runs (it is not part of the
// File / Edit menus locked by 007).
void CMFCApplication1Dlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar == nullptr || pScrollBar->GetDlgCtrlID() != IDC_SCROLL_LOG)
	{
		CDialogEx::OnVScroll(nSBCode, nPos, pScrollBar);
		return;
	}
	const long long top = static_cast<long long>(m_logTop);
	const long long page = LogVisibleRows();
	switch (nSBCode)
	{
	case SB_LINEUP: ScrollLogTo(top - 1); break;
	case SB_LINEDOWN: ScrollLogTo(top + 1); break;
	case SB_PAGEUP: ScrollLogTo(top - page); break;
	case SB_PAGEDOWN: ScrollLogTo(top + page); break;
	case SB_TOP: ScrollLogTo(0); break;
	case SB_BOTTOM: ScrollLogTo(static_cast<long long>(m_logShown)); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
	{
		// nPos is 16-bit; the tracking position from GetScrollInfo is not.
		SCROLLINFO si = {};
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS;
		pScrollBar->GetScrollInfo(&si);
		ScrollLogTo(si.nTrackPos);
		break;
	}
	default:
		break;
	}
}

BOOL CMFCApplication1Dlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CPoint client = pt;
	ScreenToClient(&client);
	if (GetLogRect().PtInRect(client))
	{
		ScrollLogTo(static_cast<long long>(m_logTop) - (zDelta / WHEEL_DELTA) * kLogWheelLines);
		return TRUE;
	}
	return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}
