// AnalysisDlg.h : analysis windows (features 028, 029)
// "Analysis > Receive completion time" and "Analysis > CAN bus load" (028),
// "Analysis > Transmit wait time" and "Analysis > Transmit jitter" (029). All are
// modeless like the simulation log window (feature 025): the main dialog
// creates them with new + Open, every way of closing destroys the window, and
// the object deletes itself after telling the owner. They only show results
// computed by CanSim/LogAnalysis.

#pragma once

#include "CanSim/LogAnalysis.h"

#include <atomic>
#include <future>
#include <vector>

// Graph drawn with GDI: a histogram (bars), the bus load per section (line)
// or the jitter of one CAN ID over time (deviation, feature 029).
class CAnalysisGraph : public CWnd
{
public:
	bool Create(CWnd* parent, UINT id);

	void SetBars(const CanSim::Histogram& histogram, const CString& xLabel, const CString& yLabel);
	void SetLine(const std::vector<double>& pct, long long sectionUs, double averagePct,
		const CString& xLabel, const CString& yLabel);
	void SetDeviation(const std::vector<CanSim::JitterPoint>& points, const CString& xLabel, const CString& yLabel);
	void SetNoData(const CString& text);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()

private:
	enum class Mode { None, Bars, Line, Deviation };
	void Draw(CDC& dc, const CRect& client, int dpi);

	Mode m_mode = Mode::None;
	CString m_noDataText;
	CString m_xLabel;
	CString m_yLabel;
	CanSim::Histogram m_histogram;
	std::vector<double> m_pct;
	long long m_sectionUs = 10000;
	double m_averagePct = 0.0;
	std::vector<CanSim::JitterPoint> m_points;
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

// Feature 029 revision 1: state shared between the analysis job (a worker
// thread reading the log and computing) and the progress window.
struct AnalysisJobState
{
	std::atomic<long long> Done{ 0 };   // bytes read
	std::atomic<long long> Total{ 0 };  // size of the log
	std::atomic<int> Phase{ 0 };        // 0 reading, 1 computing
	std::atomic<bool> Cancel{ false };  // set by the Stop button
};

// Feature 029 revision 1: modal "Analysing" window with a progress bar and a
// Stop button. It closes itself when the job has finished (IDOK), or after
// Stop once the job has noticed it (IDCANCEL); the main window stays
// disabled meanwhile.
class CAnalysisProgressDlg : public CDialogEx
{
public:
	CAnalysisProgressDlg(CWnd* owner, AnalysisJobState& state, std::future<int>& job);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ANALYSIS_PROGRESS };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

private:
	AnalysisJobState& m_state;
	std::future<int>& m_job;
	int m_shownPhase = -1;
	bool m_cancelled = false;
	UINT_PTR m_timer = 0;
};

// Feature 029: transmit wait time per CAN ID (table + histogram, as CLatencyDlg).
class CTxWaitDlg : public CAnalysisDlgBase
{
public:
	CTxWaitDlg(CWnd* owner, const CString& projectName, const CanSim::TxWaitAnalysis& analysis);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ANALYSIS_TXWAIT };
#endif

protected:
	virtual void CreateContent();
	virtual void LayoutContent(int cx, int cy);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	void ShowRow(int index);

	CanSim::TxWaitAnalysis m_analysis;
	CListCtrl m_list;
	CStatic m_note;
	CAnalysisGraph m_graph;
};

// Feature 029: transmit jitter per CAN ID (table + deviation over time).
class CJitterDlg : public CAnalysisDlgBase
{
public:
	CJitterDlg(CWnd* owner, const CString& projectName, const CanSim::JitterAnalysis& analysis);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ANALYSIS_JITTER };
#endif

protected:
	virtual void CreateContent();
	virtual void LayoutContent(int cx, int cy);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	void ShowRow(int index);

	CanSim::JitterAnalysis m_analysis;
	CListCtrl m_list;
	CStatic m_note;
	CAnalysisGraph m_graph;
};
