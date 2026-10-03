// AnalysisDlg.h : analysis windows (feature 028)
// "Analysis > Receive completion time" and "Analysis > CAN bus load". Both are
// modeless like the simulation log window (feature 025): the main dialog
// creates them with new + Open, every way of closing destroys the window, and
// the object deletes itself after telling the owner. They only show results
// computed by CanSim/LogAnalysis.

#pragma once

#include "CanSim/LogAnalysis.h"

#include <vector>

// Graph drawn with GDI: a histogram (bars) or the bus load per section (line).
class CAnalysisGraph : public CWnd
{
public:
	bool Create(CWnd* parent, UINT id);

	void SetBars(const CanSim::Histogram& histogram, const CString& xLabel, const CString& yLabel);
	void SetLine(const std::vector<double>& pct, long long sectionUs, double averagePct,
		const CString& xLabel, const CString& yLabel);
	void SetNoData(const CString& text);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()

private:
	enum class Mode { None, Bars, Line };
	void Draw(CDC& dc, const CRect& client, int dpi);

	Mode m_mode = Mode::None;
	CString m_noDataText;
	CString m_xLabel;
	CString m_yLabel;
	CanSim::Histogram m_histogram;
	std::vector<double> m_pct;
	long long m_sectionUs = 10000;
	double m_averagePct = 0.0;
};

// Placement, closing and the closed notice shared by the two windows.
class CAnalysisDlgBase : public CDialogEx
{
public:
	CAnalysisDlgBase(UINT idd, CWnd* owner, UINT closedMessage, const CString& projectName);
	bool Open();

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	virtual void PostNcDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	DECLARE_MESSAGE_MAP()

	virtual void CreateContent() = 0;
	virtual void LayoutContent(int cx, int cy) = 0;
	int Dpi() const { return m_dpi; }
	int Px(int px96) const { return MulDiv(px96, m_dpi, 96); }
	bool CreateList(CListCtrl& list, const CString& columns, const std::vector<int>& widths96, int numericFrom);

	CString m_projectName;
	CSize m_initialSize96 = CSize(900, 600);
	CSize m_minSize96 = CSize(480, 360);

private:
	UINT m_idd;
	CWnd* m_ownerWnd;
	HWND m_owner = nullptr; // kept for the notice after the window is gone
	UINT m_closedMessage;
	int m_dpi = 96;
};

class CLatencyDlg : public CAnalysisDlgBase
{
public:
	CLatencyDlg(CWnd* owner, const CString& projectName, const CanSim::LatencyAnalysis& analysis);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ANALYSIS_LATENCY };
#endif

protected:
	virtual void CreateContent();
	virtual void LayoutContent(int cx, int cy);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	void ShowRow(int index);

	CanSim::LatencyAnalysis m_analysis;
	CListCtrl m_list;
	CStatic m_note;
	CAnalysisGraph m_graph;
};

class CBusLoadDlg : public CAnalysisDlgBase
{
public:
	CBusLoadDlg(CWnd* owner, const CString& projectName, const CanSim::BusLoadAnalysis& analysis);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ANALYSIS_BUSLOAD };
#endif

protected:
	virtual void CreateContent();
	virtual void LayoutContent(int cx, int cy);

private:
	CanSim::BusLoadAnalysis m_analysis;
	CStatic m_summary;
	CAnalysisGraph m_graph;
	CListCtrl m_list;
};
