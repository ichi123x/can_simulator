// LogViewDlg.h
// "Log > Simulation log" window (feature 025): shows the frame log of the
// current project read-only. The log may have millions of lines, so the text
// view draws only the visible lines, read through CanSim::LogFileIndex.
//
// The window is modeless like the frame editor (feature 010): the owner
// creates it with new + Open; every way of closing it destroys the window,
// and the object deletes itself after sending WM_APP_LOG_VIEWER_CLOSED.

#pragma once

#include "CanSim/LogFileIndex.h"

#include <string>
#include <vector>

// Read-only text view: vertical scroll by line, horizontal by character,
// character selection with the mouse or Shift + keys, Ctrl+A / Ctrl+C.
class CLogTextView : public CWnd
{
public:
	bool Create(CWnd* parent, const CanSim::LogFileIndex* index);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

private:
	struct TextPos
	{
		long long Line = 0;
		int Col = 0;
		bool operator==(const TextPos& o) const { return Line == o.Line && Col == o.Col; }
		bool operator<(const TextPos& o) const { return Line < o.Line || (Line == o.Line && Col < o.Col); }
	};

	int VisibleRows() const;      // whole rows that fit (at least 1)
	int VisibleCols() const;
	long long MaxTop() const;
	int MaxLeft() const;
	const std::string* LineAt(long long line);  // through the cache; nullptr outside the file
	int LineLength(long long line);
	TextPos HitTest(CPoint point);
	void ScrollTo(long long top, int left);
	void UpdateScrollBars();
	void MoveCaret(const TextPos& pos, bool extend);
	void EnsureCaretVisible();
	bool HasSelection() const { return !(m_anchor == m_caret); }
	void SelectAll();
	void CopySelection();
	void StopDragging();

	const CanSim::LogFileIndex* m_index = nullptr;
	CFont m_font;
	int m_lineHeight = 16;
	int m_charWidth = 8;
	long long m_top = 0;   // first visible line
	int m_left = 0;        // first visible column
	TextPos m_anchor;      // selection start
	TextPos m_caret;       // selection end / caret
	bool m_dragging = false;
	UINT_PTR m_dragTimer = 0;
	CPoint m_lastMouse;
	long long m_cacheFirst = -1;           // lines held in m_cache start here
	std::vector<std::string> m_cache;
};

class CLogViewDlg : public CDialogEx
{
public:
	CLogViewDlg(CWnd* owner, const CString& logPath);

	// Builds the line index and creates the window. False when the log cannot
	// be read (nothing is created; the caller deletes the object).
	bool Open();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LOG_VIEW };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	virtual void PostNcDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	DECLARE_MESSAGE_MAP()

private:
	CWnd* m_ownerWnd;
	HWND m_owner = nullptr; // kept for the notice after the window is gone
	CString m_logPath;
	CanSim::LogFileIndex m_index;
	CLogTextView m_view;
};
