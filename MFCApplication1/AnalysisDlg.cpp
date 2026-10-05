// AnalysisDlg.cpp : analysis windows (feature 028)
#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "AnalysisDlg.h"
#include "FrameEditDlg.h" // WM_APP_* numbers
#include "afxdialogex.h"

#include <algorithm>
#include <cmath>

namespace
{
	const COLORREF kGraphBack = RGB(255, 255, 255);
	const COLORREF kGraphFrame = RGB(160, 160, 160);
	const COLORREF kGridLine = RGB(225, 225, 225);
	const COLORREF kAxisText = RGB(60, 60, 60);
	const COLORREF kBarColor = RGB(30, 110, 200);
	const COLORREF kLineColor = RGB(30, 110, 200);
	const COLORREF kAverageColor = RGB(240, 140, 0);
	const int kTextPx = 12;

	CString LoadRcString(UINT id)
	{
		CString s;
		s.LoadString(id);
		return s;
	}

	CString FormatTick(double value)
	{
		CString s;
		if (std::fabs(value - std::round(value)) < 1e-9)
			s.Format(_T("%.0f"), value);
		else
			s.Format(_T("%g"), value);
		return s;
	}

	CString Fixed1(double value)
	{
		CString s;
		s.Format(_T("%.1f"), value);
		return s;
	}

	CString EcuName(int node)
	{
		CString s;
		s.Format(_T("ECU %02d"), node);
		return s;
	}

	// Ticks that start at 0 and reach at least hi (for counts and loads).
	std::vector<double> TicksFromZero(double hi, int approxCount)
	{
		std::vector<double> ticks = CanSim::NiceTicks(0.0, (std::max)(hi, 1.0), approxCount);
		if (ticks.size() >= 2 && ticks.back() < hi)
			ticks.push_back(ticks.back() + (ticks[1] - ticks[0]));
		return ticks;
	}
}

// ---- CAnalysisGraph ----

BEGIN_MESSAGE_MAP(CAnalysisGraph, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

bool CAnalysisGraph::Create(CWnd* parent, UINT id)
{
	const CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW));
	return CWnd::Create(cls, _T(""), WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), parent, id) != FALSE;
}

void CAnalysisGraph::SetBars(const CanSim::Histogram& histogram, const CString& xLabel, const CString& yLabel)
{
	m_mode = histogram.Counts.empty() ? Mode::None : Mode::Bars;
	m_histogram = histogram;
	m_xLabel = xLabel;
	m_yLabel = yLabel;
	if (GetSafeHwnd() != nullptr)
		Invalidate();
}

void CAnalysisGraph::SetLine(const std::vector<double>& pct, long long sectionUs, double averagePct,
	const CString& xLabel, const CString& yLabel)
{
	m_mode = pct.empty() ? Mode::None : Mode::Line;
	m_pct = pct;
	m_sectionUs = sectionUs;
	m_averagePct = averagePct;
	m_xLabel = xLabel;
	m_yLabel = yLabel;
	if (GetSafeHwnd() != nullptr)
		Invalidate();
}

void CAnalysisGraph::SetNoData(const CString& text)
{
	m_mode = Mode::None;
	m_noDataText = text;
	if (GetSafeHwnd() != nullptr)
		Invalidate();
}

BOOL CAnalysisGraph::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE; // everything is painted in OnPaint (no flicker)
}

void CAnalysisGraph::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	if (client.IsRectEmpty())
		return;
	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
	CBitmap* oldBmp = mem.SelectObject(&bmp);
	Draw(mem, client, dc.GetDeviceCaps(LOGPIXELSX));
	dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldBmp);
}

void CAnalysisGraph::Draw(CDC& dc, const CRect& client, int dpi)
{
	auto px = [dpi](int v) { return MulDiv(v, dpi, 96); };
	dc.FillSolidRect(client, kGraphBack);
	dc.SetBkMode(TRANSPARENT);

	CFont font;
	font.CreateFont(-px(kTextPx), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Meiryo UI"));
	CFont* oldFont = dc.SelectObject(&font);
	dc.SetTextColor(kAxisText);

	// frame around the whole graph
	CBrush frameBrush(kGraphFrame);
	dc.FrameRect(client, &frameBrush);

	if (m_mode == Mode::None)
	{
		CRect r(client);
		dc.DrawText(m_noDataText, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		dc.SelectObject(oldFont);
		return;
	}

	// plot area: room for the values and the axis names
	// (the top margin holds the y axis name above the highest value)
	const CRect plot(client.left + px(70), client.top + px(32), client.right - px(20), client.bottom - px(46));
	if (plot.Width() < px(40) || plot.Height() < px(30))
	{
		dc.SelectObject(oldFont);
		return;
	}

	// value ranges
	double xLo = 0.0, xHi = 1.0, yHi = 1.0;
	std::vector<double> yTicks;
	if (m_mode == Mode::Bars)
	{
		xLo = static_cast<double>(m_histogram.StartUs);
		xHi = xLo + static_cast<double>(m_histogram.WidthUs) * static_cast<double>(m_histogram.Counts.size());
		long long maxCount = 1;
		for (long long c : m_histogram.Counts)
			maxCount = (std::max)(maxCount, c);
		yTicks = TicksFromZero(static_cast<double>(maxCount), static_cast<int>((std::min)(5LL, maxCount)));
		yHi = yTicks.back();
	}
	else
	{
		xLo = 0.0;
		xHi = static_cast<double>(m_sectionUs) * static_cast<double>(m_pct.size()) / 1000.0; // ms
		yTicks = { 0, 20, 40, 60, 80, 100 };
		yHi = 100.0;
	}
	auto xToPx = [&](double x) { return plot.left + static_cast<int>(std::lround((x - xLo) / (xHi - xLo) * plot.Width())); };
	auto yToPx = [&](double y) { return plot.bottom - static_cast<int>(std::lround(y / yHi * plot.Height())); };

	// grid and y values
	CPen gridPen(PS_SOLID, 1, kGridLine);
	CPen* oldPen = dc.SelectObject(&gridPen);
	for (double t : yTicks)
	{
		const int y = yToPx(t);
		dc.MoveTo(plot.left, y);
		dc.LineTo(plot.right, y);
		CRect r(client.left, y - px(9), plot.left - px(6), y + px(9));
		dc.DrawText(FormatTick(t), r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}
	// x values
	for (double t : CanSim::NiceTicks(xLo, xHi, (std::max)(2, plot.Width() / px(90))))
	{
		const int x = xToPx(t);
		dc.MoveTo(x, plot.bottom);
		dc.LineTo(x, plot.bottom + px(4));
		CRect r(x - px(50), plot.bottom + px(5), x + px(50), plot.bottom + px(23));
		dc.DrawText(FormatTick(t), r, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
	}

	// data
	if (m_mode == Mode::Bars)
	{
		const std::size_t n = m_histogram.Counts.size();
		for (std::size_t i = 0; i < n; ++i)
		{
			if (m_histogram.Counts[i] == 0)
				continue;
			const double left = xLo + static_cast<double>(m_histogram.WidthUs) * static_cast<double>(i);
			int x0 = xToPx(left);
			int x1 = xToPx(left + static_cast<double>(m_histogram.WidthUs));
			if (x1 - x0 > 2)
				x1 -= 1; // a thin gap between bars
			const int y = yToPx(static_cast<double>(m_histogram.Counts[i]));
			dc.FillSolidRect(x0, y, (std::max)(1, x1 - x0), plot.bottom - y, kBarColor);
		}
	}
	else
	{
		// one point per column at most, keeping the peak of each column
		const std::vector<double> values = CanSim::MaxPerColumn(m_pct, plot.Width());
		CPen linePen(PS_SOLID, (std::max)(1, px(1)), kLineColor);
		dc.SelectObject(&linePen);
		const std::size_t n = values.size();
		for (std::size_t i = 0; i < n; ++i)
		{
			const int x = plot.left + static_cast<int>((static_cast<double>(i) + 0.5) * plot.Width() / static_cast<double>(n));
			const int y = yToPx((std::min)(values[i], 100.0));
			if (i == 0)
				dc.MoveTo(x, y);
			else
				dc.LineTo(x, y);
		}
		// average as a dashed line with its value
		CPen avgPen(PS_DASH, 1, kAverageColor);
		dc.SelectObject(&avgPen);
		const int ya = yToPx(m_averagePct);
		dc.MoveTo(plot.left, ya);
		dc.LineTo(plot.right, ya);
		dc.SetTextColor(kAverageColor);
		CString avg;
		avg.Format(_T("%s %s%%"), (LPCTSTR)LoadRcString(IDS_BUS_AVERAGE), (LPCTSTR)Fixed1(m_averagePct));
		CRect ar(plot.right - px(160), ya - px(20), plot.right - px(4), ya - px(2));
		dc.SetBkMode(OPAQUE); // on white, so the load line does not run through the text
		dc.SetBkColor(kGraphBack);
		dc.DrawText(avg, ar, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(kAxisText);
	}

	// axes and their names
	CPen axisPen(PS_SOLID, 1, kAxisText);
	dc.SelectObject(&axisPen);
	dc.MoveTo(plot.left, plot.top);
	dc.LineTo(plot.left, plot.bottom);
	dc.LineTo(plot.right, plot.bottom);
	CRect xr(plot.left, plot.bottom + px(24), plot.right, client.bottom - px(2));
	dc.DrawText(m_xLabel, xr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	CRect yr(client.left + px(6), client.top + px(4), plot.left + px(200), plot.top - px(12));
	dc.DrawText(m_yLabel, yr, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

	dc.SelectObject(oldPen);
	dc.SelectObject(oldFont);
}

// ---- CAnalysisDlgBase ----

BEGIN_MESSAGE_MAP(CAnalysisDlgBase, CDialogEx)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

CAnalysisDlgBase::CAnalysisDlgBase(UINT idd, CWnd* owner, UINT closedMessage, const CString& projectName)
	: CDialogEx(idd, owner), m_projectName(projectName), m_idd(idd), m_ownerWnd(owner), m_closedMessage(closedMessage)
{
}

bool CAnalysisDlgBase::Open()
{
	if (!Create(m_idd, m_ownerWnd))
		return false;
	ShowWindow(SW_SHOW);
	return true;
}

BOOL CAnalysisDlgBase::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_owner = m_ownerWnd != nullptr ? m_ownerWnd->GetSafeHwnd() : nullptr;
	{
		CClientDC dc(this);
		m_dpi = dc.GetDeviceCaps(LOGPIXELSX);
	}

	// Centred on the owner and kept on its monitor (as the log window, 025).
	const int width = Px(m_initialSize96.cx);
	const int height = Px(m_initialSize96.cy);
	CRect ownerRect;
	if (m_ownerWnd != nullptr)
		m_ownerWnd->GetWindowRect(&ownerRect);
	else
		GetWindowRect(&ownerRect);
	CRect rect(CPoint(ownerRect.CenterPoint().x - width / 2, ownerRect.CenterPoint().y - height / 2), CSize(width, height));
	MONITORINFO mi = { sizeof(MONITORINFO) };
	if (::GetMonitorInfo(::MonitorFromWindow(m_owner != nullptr ? m_owner : GetSafeHwnd(), MONITOR_DEFAULTTONEAREST), &mi))
	{
		const CRect work(mi.rcWork);
		if (rect.right > work.right)
			rect.OffsetRect(work.right - rect.right, 0);
		if (rect.bottom > work.bottom)
			rect.OffsetRect(0, work.bottom - rect.bottom);
		if (rect.left < work.left)
			rect.OffsetRect(work.left - rect.left, 0);
		if (rect.top < work.top)
			rect.OffsetRect(0, work.top - rect.top);
	}

	CreateContent();
	MoveWindow(rect);
	CRect client;
	GetClientRect(&client);
	LayoutContent(client.Width(), client.Height());
	return TRUE;
}

bool CAnalysisDlgBase::CreateList(CListCtrl& list, const CString& columns, const std::vector<int>& widths96, int numericFrom)
{
	if (!list.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		CRect(0, 0, 10, 10), this, IDC_LIST_ANALYSIS))
		return false;
	list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
	list.SetFont(GetFont());
	int pos = 0;
	for (int i = 0; ; ++i)
	{
		CString name;
		if (!AfxExtractSubString(name, columns, i, _T('|')) || (name.IsEmpty() && i > 0))
			break;
		const int width = i < static_cast<int>(widths96.size()) ? widths96[static_cast<std::size_t>(i)] : 100;
		list.InsertColumn(i, name, i >= numericFrom ? LVCFMT_RIGHT : LVCFMT_LEFT, Px(width));
		++pos;
	}
	return pos > 0;
}

void CAnalysisDlgBase::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && GetDlgItem(IDC_LIST_ANALYSIS) != nullptr)
		LayoutContent(cx, cy);
}

void CAnalysisDlgBase::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	lpMMI->ptMinTrackSize.x = Px(m_minSize96.cx);
	lpMMI->ptMinTrackSize.y = Px(m_minSize96.cy);
	CDialogEx::OnGetMinMaxInfo(lpMMI);
}

// Enter does not close the window; Esc and the close box do.
void CAnalysisDlgBase::OnOK()
{
}

void CAnalysisDlgBase::OnCancel()
{
	DestroyWindow();
}

void CAnalysisDlgBase::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	if (m_owner != nullptr && ::IsWindow(m_owner))
		::SendMessage(m_owner, m_closedMessage, 0, 0);
	delete this;
}

// ---- CLatencyDlg ----

BEGIN_MESSAGE_MAP(CLatencyDlg, CAnalysisDlgBase)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_ANALYSIS, &CLatencyDlg::OnItemChanged)
END_MESSAGE_MAP()

CLatencyDlg::CLatencyDlg(CWnd* owner, const CString& projectName, const CanSim::LatencyAnalysis& analysis)
	: CAnalysisDlgBase(IDD_ANALYSIS_LATENCY, owner, WM_APP_LATENCY_CLOSED, projectName), m_analysis(analysis)
{
	m_initialSize96 = CSize(900, 640);
	m_minSize96 = CSize(520, 400); // T007 asked for 480 x 360; below 520 the note line is cut off
}

void CLatencyDlg::CreateContent()
{
	CString title;
	title.Format(LoadRcString(IDS_LAT_TITLE), (LPCTSTR)m_projectName);
	SetWindowText(title);

	CreateList(m_list, LoadRcString(IDS_LAT_COLUMNS), { 90, 90, 80, 100, 100, 100, 100 }, 2);
	for (std::size_t i = 0; i < m_analysis.Rows.size(); ++i)
	{
		const CanSim::LatencyRow& row = m_analysis.Rows[i];
		const int item = static_cast<int>(i);
		m_list.InsertItem(item, i == 0 ? LoadRcString(IDS_LAT_ALL) : EcuName(row.TxNode));
		m_list.SetItemText(item, 1, i == 0 ? CString() : EcuName(row.RxNode));
		CString text;
		text.Format(_T("%lld"), row.Count);
		m_list.SetItemText(item, 2, text);
		text.Format(_T("%lld"), row.MinUs);
		m_list.SetItemText(item, 3, text);
		m_list.SetItemText(item, 4, Fixed1(row.MeanUs));
		text.Format(_T("%lld"), row.MaxUs);
		m_list.SetItemText(item, 5, text);
		text.Format(_T("%lld"), row.P95Us);
		m_list.SetItemText(item, 6, text);
	}

	CString note;
	note.Format(LoadRcString(IDS_LAT_NOTE), static_cast<int>(m_analysis.OverwriteCount));
	m_note.Create(note, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, CRect(0, 0, 10, 10), this, IDC_STATIC_ANALYSIS);
	m_note.SetFont(GetFont());

	m_graph.Create(this, IDC_GRAPH_ANALYSIS);
	m_graph.SetNoData(LoadRcString(IDS_ANALYSIS_NO_DATA));
	if (!m_analysis.Rows.empty())
	{
		m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		ShowRow(0);
	}
	m_list.SetFocus();
}

void CLatencyDlg::LayoutContent(int cx, int cy)
{
	const int margin = Px(8);
	const int noteHeight = Px(22);
	const int inner = (std::max)(0, cy - 2 * margin - noteHeight - margin);
	const int listHeight = inner * 2 / 5;
	m_list.MoveWindow(margin, margin, (std::max)(0, cx - 2 * margin), listHeight);
	const int noteTop = margin + listHeight + Px(4);
	m_note.MoveWindow(margin, noteTop, (std::max)(0, cx - 2 * margin), noteHeight);
	const int graphTop = noteTop + noteHeight + Px(4);
	m_graph.MoveWindow(margin, graphTop, (std::max)(0, cx - 2 * margin), (std::max)(0, cy - margin - graphTop));
}

void CLatencyDlg::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	const NMLISTVIEW* nm = reinterpret_cast<const NMLISTVIEW*>(pNMHDR);
	if ((nm->uChanged & LVIF_STATE) != 0 && (nm->uNewState & LVIS_SELECTED) != 0 && (nm->uOldState & LVIS_SELECTED) == 0)
		ShowRow(nm->iItem);
	*pResult = 0;
}

void CLatencyDlg::ShowRow(int index)
{
	if (index < 0 || static_cast<std::size_t>(index) >= m_analysis.Rows.size())
		return;
	const CanSim::LatencyRow& row = m_analysis.Rows[static_cast<std::size_t>(index)];
	if (row.Count == 0)
		m_graph.SetNoData(LoadRcString(IDS_ANALYSIS_NO_DATA));
	else
		m_graph.SetBars(CanSim::MakeHistogram(row.Values), LoadRcString(IDS_LAT_AXIS_X), LoadRcString(IDS_LAT_AXIS_Y));
}

// ---- CBusLoadDlg ----

CBusLoadDlg::CBusLoadDlg(CWnd* owner, const CString& projectName, const CanSim::BusLoadAnalysis& analysis)
	: CAnalysisDlgBase(IDD_ANALYSIS_BUSLOAD, owner, WM_APP_BUSLOAD_CLOSED, projectName), m_analysis(analysis)
{
	m_initialSize96 = CSize(960, 680);
	m_minSize96 = CSize(520, 400); // T010
}

void CBusLoadDlg::CreateContent()
{
	CString title;
	title.Format(LoadRcString(IDS_BUS_TITLE), (LPCTSTR)m_projectName);
	SetWindowText(title);

	const long long sectionMs = m_analysis.SectionUs / 1000;
	CString from, to, range;
	from.Format(_T("%lld"), static_cast<long long>(m_analysis.MaxSection) * sectionMs);
	to.Format(_T("%lld"), static_cast<long long>(m_analysis.MaxSection + 1) * sectionMs);
	range.Format(_T("%lld"), m_analysis.RangeUs / 1000);
	CString summary;
	summary.Format(LoadRcString(IDS_BUS_SUMMARY), (LPCTSTR)Fixed1(m_analysis.AveragePct), (LPCTSTR)Fixed1(m_analysis.MaxPct),
		(LPCTSTR)from, (LPCTSTR)to, (LPCTSTR)range);
	m_summary.Create(summary, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, CRect(0, 0, 10, 10), this, IDC_STATIC_ANALYSIS);
	m_summary.SetFont(GetFont());

	m_graph.Create(this, IDC_GRAPH_ANALYSIS);
	if (m_analysis.TransmissionCount == 0)
		m_graph.SetNoData(LoadRcString(IDS_ANALYSIS_NO_DATA));
	else
		m_graph.SetLine(m_analysis.SectionPct, m_analysis.SectionUs, m_analysis.AveragePct,
			LoadRcString(IDS_BUS_AXIS_X), LoadRcString(IDS_BUS_AXIS_Y));

	CreateList(m_list, LoadRcString(IDS_BUS_COLUMNS), { 100, 140, 120 }, 1);
	for (std::size_t i = 0; i < m_analysis.Ecus.size(); ++i)
	{
		const CanSim::EcuBusUsage& e = m_analysis.Ecus[i];
		const int item = static_cast<int>(i);
		m_list.InsertItem(item, EcuName(e.Node));
		CString text;
		text.Format(_T("%lld"), e.FrameCount);
		m_list.SetItemText(item, 1, text);
		m_list.SetItemText(item, 2, Fixed1(e.UsagePct));
	}
	m_list.SetFocus();
}

void CBusLoadDlg::LayoutContent(int cx, int cy)
{
	const int margin = Px(8);
	const int summaryHeight = Px(22);
	const int width = (std::max)(0, cx - 2 * margin);
	m_summary.MoveWindow(margin, margin, width, summaryHeight);
	const int top = margin + summaryHeight + Px(4);
	const int rest = (std::max)(0, cy - margin - top - Px(6));
	const int graphHeight = rest * 3 / 5;
	m_graph.MoveWindow(margin, top, width, graphHeight);
	const int listTop = top + graphHeight + Px(6);
	m_list.MoveWindow(margin, listTop, width, (std::max)(0, cy - margin - listTop));
}
