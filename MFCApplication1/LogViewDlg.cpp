// LogViewDlg.cpp : simulation log window (feature 025)
#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "LogViewDlg.h"
#include "FrameEditDlg.h" // WM_APP_* numbers
#include "afxdialogex.h"

#include <algorithm>

namespace
{
	const UINT_PTR kDragTimerId = 1;
	const UINT kDragTimerMs = 50;
	const int kWheelLines = 3;
	const std::size_t kMaxCopyBytes = 16 * 1024 * 1024; // research decision 3
	const COLORREF kSelectionBack = RGB(51, 153, 255);
	const COLORREF kSelectionText = RGB(255, 255, 255);

	CString LoadRcString(UINT id)
	{
		CString s;
		VERIFY(s.LoadString(id));
		return s;
	}

	std::wstring Widen(const std::string& s, std::size_t from, std::size_t count)
	{
		if (from >= s.size())
			return std::wstring();
		const std::size_t n = (std::min)(count, s.size() - from);
		return std::wstring(s.begin() + static_cast<std::ptrdiff_t>(from),
			s.begin() + static_cast<std::ptrdiff_t>(from + n));
	}
}

// ---------------------------------------------------------------------------
// CLogTextView

BEGIN_MESSAGE_MAP(CLogTextView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_GETDLGCODE()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_CAPTURECHANGED()
	ON_WM_TIMER()
	ON_WM_CONTEXTMENU()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CLogTextView::Create(CWnd* parent, const CanSim::LogFileIndex* index)
{
	m_index = index;
	const CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_IBEAM));
	CRect client;
	parent->GetClientRect(&client);
	if (!CWnd::CreateEx(WS_EX_CLIENTEDGE, cls, _T(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
			client, parent, AFX_IDW_PANE_FIRST))
		return false;

	// Same font as the log view area of the main dialog (feature 009).
	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSY);
	m_font.CreateFont(-MulDiv(13, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		FIXED_PITCH | FF_MODERN, _T("MS Gothic"));
	CFont* old = dc.SelectObject(&m_font);
	TEXTMETRIC tm = {};
	dc.GetTextMetrics(&tm);
	dc.SelectObject(old);
	m_lineHeight = (std::max)(1, static_cast<int>(tm.tmHeight));
	m_charWidth = (std::max)(1, static_cast<int>(tm.tmAveCharWidth));
	UpdateScrollBars();
	return true;
}

int CLogTextView::VisibleRows() const
{
	CRect client;
	GetClientRect(&client);
	return (std::max)(1, client.Height() / m_lineHeight);
}

int CLogTextView::VisibleCols() const
{
	CRect client;
	GetClientRect(&client);
	return (std::max)(1, client.Width() / m_charWidth);
}

long long CLogTextView::MaxTop() const
{
	const long long lines = m_index != nullptr ? m_index->LineCount() : 0;
	return (std::max)(0LL, lines - VisibleRows());
}

int CLogTextView::MaxLeft() const
{
	const int width = (m_index != nullptr ? m_index->MaxLineLength() : 0) + 1; // room for the caret at the end
	return (std::max)(0, width - VisibleCols());
}

// Reads a few screens around the line at a time so that scrolling one line
// does not open the file again.
const std::string* CLogTextView::LineAt(long long line)
{
	if (m_index == nullptr || line < 0 || line >= m_index->LineCount())
		return nullptr;
	if (m_cacheFirst < 0 || line < m_cacheFirst || line >= m_cacheFirst + static_cast<long long>(m_cache.size()))
	{
		const int rows = VisibleRows();
		m_cacheFirst = (std::max)(0LL, line - rows);
		m_cache = m_index->ReadLines(m_cacheFirst, rows * 3 + 2);
		if (line >= m_cacheFirst + static_cast<long long>(m_cache.size()))
			return nullptr; // the file could not be read any more
	}
	return &m_cache[static_cast<std::size_t>(line - m_cacheFirst)];
}

int CLogTextView::LineLength(long long line)
{
	const std::string* text = LineAt(line);
	return text != nullptr ? static_cast<int>(text->size()) : 0;
}

void CLogTextView::OnPaint()
{
	CPaintDC paint(this);
	CRect client;
	GetClientRect(&client);

	CDC mem;
	mem.CreateCompatibleDC(&paint);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&paint, client.Width(), client.Height());
	CBitmap* oldBitmap = mem.SelectObject(&bitmap);
	CFont* oldFont = mem.SelectObject(&m_font);
	mem.FillSolidRect(client, RGB(255, 255, 255));
	mem.SetBkMode(TRANSPARENT);

	TextPos selStart = m_anchor < m_caret ? m_anchor : m_caret;
	TextPos selEnd = m_anchor < m_caret ? m_caret : m_anchor;
	const bool selecting = HasSelection();
	const int cols = VisibleCols() + 1;
	const int rows = VisibleRows() + 1;

	for (int r = 0; r < rows; ++r)
	{
		const long long line = m_top + r;
		const std::string* text = LineAt(line);
		if (text == nullptr)
			break;
		const int y = r * m_lineHeight;
		const std::wstring visible = Widen(*text, static_cast<std::size_t>(m_left), static_cast<std::size_t>(cols));
		mem.SetTextColor(RGB(0, 0, 0));
		::TextOutW(mem.GetSafeHdc(), 0, y, visible.c_str(), static_cast<int>(visible.size()));

		if (selecting && line >= selStart.Line && line <= selEnd.Line)
		{
			// The line break of a selected line is shown as one selected cell.
			const int from = line == selStart.Line ? selStart.Col : 0;
			const int to = line == selEnd.Line ? selEnd.Col : static_cast<int>(text->size()) + 1;
			const int a = (std::max)(from, m_left);
			const int b = (std::min)(to, m_left + cols);
			if (b > a)
			{
				CRect sel((a - m_left) * m_charWidth, y, (b - m_left) * m_charWidth, y + m_lineHeight);
				mem.FillSolidRect(sel, kSelectionBack);
				const std::wstring part = Widen(*text, static_cast<std::size_t>(a), static_cast<std::size_t>(b - a));
				mem.SetTextColor(kSelectionText);
				::TextOutW(mem.GetSafeHdc(), sel.left, y, part.c_str(), static_cast<int>(part.size()));
			}
		}
	}

	// caret
	if (GetFocus() == this && m_caret.Line >= m_top && m_caret.Line < m_top + rows && m_caret.Col >= m_left)
	{
		const int x = (m_caret.Col - m_left) * m_charWidth;
		const int y = static_cast<int>(m_caret.Line - m_top) * m_lineHeight;
		mem.FillSolidRect(x, y, 1, m_lineHeight, RGB(0, 0, 0));
	}

	paint.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldFont);
	mem.SelectObject(oldBitmap);
}

BOOL CLogTextView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE; // OnPaint fills everything (no flicker)
}

void CLogTextView::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	ScrollTo(m_top, m_left);
}

void CLogTextView::UpdateScrollBars()
{
	if (GetSafeHwnd() == nullptr || m_index == nullptr)
		return;
	SCROLLINFO v = { sizeof(SCROLLINFO) };
	v.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	v.nMin = 0;
	v.nMax = static_cast<int>((std::max)(0LL, m_index->LineCount() - 1));
	v.nPage = static_cast<UINT>(VisibleRows());
	v.nPos = static_cast<int>(m_top);
	SetScrollInfo(SB_VERT, &v, TRUE);

	SCROLLINFO h = { sizeof(SCROLLINFO) };
	h.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	h.nMin = 0;
	h.nMax = m_index->MaxLineLength(); // + 1 column for the caret at the end
	h.nPage = static_cast<UINT>(VisibleCols());
	h.nPos = m_left;
	SetScrollInfo(SB_HORZ, &h, TRUE);
}

void CLogTextView::ScrollTo(long long top, int left)
{
	m_top = (std::max)(0LL, (std::min)(top, MaxTop()));
	m_left = (std::max)(0, (std::min)(left, MaxLeft()));
	UpdateScrollBars();
	Invalidate(FALSE);
}

void CLogTextView::OnVScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
	const long long rows = VisibleRows();
	long long top = m_top;
	switch (nSBCode)
	{
	case SB_LINEUP:   top -= 1; break;
	case SB_LINEDOWN: top += 1; break;
	case SB_PAGEUP:   top -= rows; break;
	case SB_PAGEDOWN: top += rows; break;
	case SB_TOP:      top = 0; break;
	case SB_BOTTOM:   top = MaxTop(); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
	{
		// nPos is only 16 bits; logs have millions of lines.
		SCROLLINFO si = { sizeof(SCROLLINFO) };
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(SB_VERT, &si);
		top = si.nTrackPos;
		break;
	}
	default:
		return;
	}
	ScrollTo(top, m_left);
}

void CLogTextView::OnHScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
	const int cols = VisibleCols();
	int left = m_left;
	switch (nSBCode)
	{
	case SB_LINELEFT:  left -= 1; break;
	case SB_LINERIGHT: left += 1; break;
	case SB_PAGELEFT:  left -= cols; break;
	case SB_PAGERIGHT: left += cols; break;
	case SB_LEFT:      left = 0; break;
	case SB_RIGHT:     left = MaxLeft(); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
	{
		SCROLLINFO si = { sizeof(SCROLLINFO) };
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(SB_HORZ, &si);
		left = si.nTrackPos;
		break;
	}
	default:
		return;
	}
	ScrollTo(m_top, left);
}

BOOL CLogTextView::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
	ScrollTo(m_top - static_cast<long long>(zDelta) * kWheelLines / WHEEL_DELTA, m_left);
	return TRUE;
}

UINT CLogTextView::OnGetDlgCode()
{
	return DLGC_WANTARROWS | DLGC_WANTCHARS;
}

CLogTextView::TextPos CLogTextView::HitTest(CPoint point)
{
	TextPos pos;
	const long long lines = m_index != nullptr ? m_index->LineCount() : 0;
	if (lines == 0)
		return pos;
	long long row = point.y >= 0 ? point.y / m_lineHeight : -((m_lineHeight - 1 - point.y) / m_lineHeight);
	pos.Line = (std::max)(0LL, (std::min)(m_top + row, lines - 1));
	// Left of the view the column goes below m_left, so a drag there scrolls left too.
	const int col = m_left + (point.x >= 0 ? (point.x + m_charWidth / 2) / m_charWidth
	                                       : (point.x - (m_charWidth - 1)) / m_charWidth);
	pos.Col = (std::max)(0, (std::min)(col, LineLength(pos.Line)));
	return pos;
}

void CLogTextView::MoveCaret(const TextPos& pos, bool extend)
{
	m_caret = pos;
	if (!extend)
		m_anchor = pos;
	EnsureCaretVisible();
	Invalidate(FALSE);
}

void CLogTextView::EnsureCaretVisible()
{
	long long top = m_top;
	int left = m_left;
	const long long rows = VisibleRows();
	const int cols = VisibleCols();
	if (m_caret.Line < top)
		top = m_caret.Line;
	else if (m_caret.Line >= top + rows)
		top = m_caret.Line - rows + 1;
	if (m_caret.Col < left)
		left = m_caret.Col;
	else if (m_caret.Col >= left + cols)
		left = m_caret.Col - cols + 1;
	ScrollTo(top, left);
}

void CLogTextView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	const long long lines = m_index != nullptr ? m_index->LineCount() : 0;
	if (ctrl && nChar == 'C')
	{
		CopySelection();
		return;
	}
	if (ctrl && nChar == 'A')
	{
		SelectAll();
		return;
	}
	if (lines == 0)
		return;

	TextPos pos = m_caret;
	const long long rows = VisibleRows();
	switch (nChar)
	{
	case VK_UP:    pos.Line -= 1; break;
	case VK_DOWN:  pos.Line += 1; break;
	case VK_PRIOR: pos.Line -= rows; break;
	case VK_NEXT:  pos.Line += rows; break;
	case VK_LEFT:
		if (pos.Col > 0)
			pos.Col -= 1;
		else if (pos.Line > 0)
		{
			pos.Line -= 1;
			pos.Col = LineLength(pos.Line);
		}
		break;
	case VK_RIGHT:
		if (pos.Col < LineLength(pos.Line))
			pos.Col += 1;
		else if (pos.Line + 1 < lines)
		{
			pos.Line += 1;
			pos.Col = 0;
		}
		break;
	case VK_HOME:
		if (ctrl)
			pos.Line = 0;
		pos.Col = 0;
		break;
	case VK_END:
		if (ctrl)
			pos.Line = lines - 1;
		pos.Col = LineLength((std::max)(0LL, (std::min)(pos.Line, lines - 1)));
		break;
	default:
		// Read-only: typing, Delete, Ctrl+V and Ctrl+X do nothing.
		CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
		return;
	}
	pos.Line = (std::max)(0LL, (std::min)(pos.Line, lines - 1));
	pos.Col = (std::max)(0, (std::min)(pos.Col, LineLength(pos.Line)));
	MoveCaret(pos, shift);
}

void CLogTextView::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
	SetFocus();
	const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	MoveCaret(HitTest(point), shift);
	m_dragging = true;
	m_lastMouse = point;
	SetCapture();
	m_dragTimer = SetTimer(kDragTimerId, kDragTimerMs, nullptr);
}

void CLogTextView::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	if (!m_dragging)
		return;
	m_lastMouse = point;
	MoveCaret(HitTest(point), true);
}

void CLogTextView::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/)
{
	if (m_dragging)
		ReleaseCapture(); // OnCaptureChanged stops the drag
}

void CLogTextView::OnCaptureChanged(CWnd* pWnd)
{
	StopDragging();
	CWnd::OnCaptureChanged(pWnd);
}

void CLogTextView::StopDragging()
{
	m_dragging = false;
	if (m_dragTimer != 0)
	{
		KillTimer(m_dragTimer);
		m_dragTimer = 0;
	}
}

// While dragging outside the view, keep scrolling towards the pointer.
void CLogTextView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != kDragTimerId || !m_dragging)
	{
		CWnd::OnTimer(nIDEvent);
		return;
	}
	CRect client;
	GetClientRect(&client);
	if (!client.PtInRect(m_lastMouse))
		MoveCaret(HitTest(m_lastMouse), true);
}

void CLogTextView::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
	if (point.x == -1 && point.y == -1) // Shift+F10 / menu key
	{
		CRect client;
		GetClientRect(&client);
		point = client.TopLeft();
		ClientToScreen(&point);
	}
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING | (HasSelection() ? MF_ENABLED : MF_GRAYED), ID_LOG_COPY, LoadRcString(IDS_LOG_MENU_COPY));
	menu.AppendMenu(MF_STRING, ID_LOG_SELECT_ALL, LoadRcString(IDS_LOG_MENU_SELECT_ALL));
	const UINT cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
	if (cmd == ID_LOG_COPY)
		CopySelection();
	else if (cmd == ID_LOG_SELECT_ALL)
		SelectAll();
}

void CLogTextView::SelectAll()
{
	const long long lines = m_index != nullptr ? m_index->LineCount() : 0;
	if (lines == 0)
		return;
	m_anchor = TextPos();
	TextPos end;
	end.Line = lines - 1;
	end.Col = LineLength(end.Line);
	m_caret = end;
	Invalidate(FALSE); // the view stays where it is
}

// Copies the selection as CRLF-separated text; refuses above kMaxCopyBytes.
void CLogTextView::CopySelection()
{
	if (!HasSelection() || m_index == nullptr)
		return;
	const TextPos from = m_anchor < m_caret ? m_anchor : m_caret;
	const TextPos to = m_anchor < m_caret ? m_caret : m_anchor;

	std::wstring text;
	const int kChunk = 4096;
	CWaitCursor wait;
	for (long long first = from.Line; first <= to.Line; first += kChunk)
	{
		const int count = static_cast<int>((std::min)(static_cast<long long>(kChunk), to.Line - first + 1));
		const std::vector<std::string> lines = m_index->ReadLines(first, count);
		if (static_cast<int>(lines.size()) != count)
			return; // the file could not be read
		for (int i = 0; i < count; ++i)
		{
			const long long line = first + i;
			const std::string& s = lines[static_cast<std::size_t>(i)];
			const std::size_t a = line == from.Line ? static_cast<std::size_t>(from.Col) : 0;
			const std::size_t b = line == to.Line ? static_cast<std::size_t>(to.Col) : s.size();
			if (b > a)
				text += Widen(s, a, b - a);
			if (line != to.Line)
				text += L"\r\n";
			if (text.size() * sizeof(wchar_t) > kMaxCopyBytes)
			{
				AfxMessageBox(LoadRcString(IDS_LOG_COPY_TOO_LARGE), MB_OK | MB_ICONINFORMATION);
				return;
			}
		}
	}

	if (!OpenClipboard())
		return;
	EmptyClipboard();
	const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
	HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (mem != nullptr)
	{
		void* p = ::GlobalLock(mem);
		if (p != nullptr)
		{
			memcpy(p, text.c_str(), bytes);
			::GlobalUnlock(mem);
			if (::SetClipboardData(CF_UNICODETEXT, mem) == nullptr)
				::GlobalFree(mem);
		}
		else
			::GlobalFree(mem);
	}
	CloseClipboard();
}

void CLogTextView::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);
	Invalidate(FALSE); // show the caret
}

void CLogTextView::OnKillFocus(CWnd* pNewWnd)
{
	CWnd::OnKillFocus(pNewWnd);
	Invalidate(FALSE);
}

void CLogTextView::OnDestroy()
{
	StopDragging();
	CWnd::OnDestroy();
}

// ---------------------------------------------------------------------------
// CLogViewDlg

BEGIN_MESSAGE_MAP(CLogViewDlg, CDialogEx)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

CLogViewDlg::CLogViewDlg(CWnd* owner, const CString& logPath)
	: CDialogEx(IDD_LOG_VIEW, owner), m_ownerWnd(owner), m_logPath(logPath)
{
}

bool CLogViewDlg::Open()
{
	{
		CWaitCursor wait; // reading a large log takes a moment
		if (!m_index.Build(std::wstring((LPCTSTR)m_logPath)))
			return false;
	}
	if (!Create(IDD_LOG_VIEW, m_ownerWnd))
		return false;
	ShowWindow(SW_SHOW);
	return true;
}

BOOL CLogViewDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_owner = m_ownerWnd != nullptr ? m_ownerWnd->GetSafeHwnd() : nullptr;

	CString title;
	title.Format(LoadRcString(IDS_LOG_VIEW_TITLE), (LPCTSTR)m_logPath);
	SetWindowText(title);

	// 900 x 600 px at 96 dpi, centred on the owner and kept on its monitor.
	int dpi = 96;
	{
		CClientDC dc(this);
		dpi = dc.GetDeviceCaps(LOGPIXELSX);
	}
	const int width = MulDiv(900, dpi, 96);
	const int height = MulDiv(600, dpi, 96);
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
	MoveWindow(rect);

	m_view.Create(this, &m_index);
	m_view.SetFocus();
	return FALSE; // the focus was set to the text view
}

void CLogViewDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (m_view.GetSafeHwnd() != nullptr)
		m_view.MoveWindow(0, 0, cx, cy);
}

void CLogViewDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	lpMMI->ptMinTrackSize.x = 300;
	lpMMI->ptMinTrackSize.y = 200;
	CDialogEx::OnGetMinMaxInfo(lpMMI);
}

// Enter does not close the window; Esc and the close box do.
void CLogViewDlg::OnOK()
{
}

void CLogViewDlg::OnCancel()
{
	DestroyWindow();
}

void CLogViewDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	if (m_owner != nullptr && ::IsWindow(m_owner))
		::SendMessage(m_owner, WM_APP_LOG_VIEWER_CLOSED, 0, 0);
	delete this;
}
