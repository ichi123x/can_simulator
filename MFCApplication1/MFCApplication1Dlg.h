
// MFCApplication1Dlg.h : ヘッダー ファイル
//

#pragma once

#include "CanSim/CanTypes.h"
#include "CanSim/FrameEvents.h"
#include "CanSim/ReplayClock.h"
#include "CanSim/LogLines.h"
#include "CanSim/SimControls.h"
#include "CanSim/FlowEffect.h"
#include "CanSim/LogAnalysis.h"

#include <functional>
#include <memory>
#include <vector>

class CFrameEditDlg;
class CLogViewDlg;
class CLatencyDlg;
class CBusLoadDlg;
class CTxWaitDlg;
class CJitterDlg;


// CMFCApplication1Dlg ダイアログ
class CMFCApplication1Dlg : public CDialogEx
{
// コンストラクション
public:
	CMFCApplication1Dlg(CWnd* pParent = nullptr);	// 標準コンストラクター

// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCAPPLICATION1_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV サポート


// 実装
protected:
	HICON m_hIcon;

	// 生成された、メッセージ割り当て関数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	// ---- feature 004: menu driven dialog ----
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnFileLoadProject();
	afx_msg void OnFileSaveProject();
	afx_msg void OnFileSaveOverwrite();   // 023: toolbar Save
	afx_msg void OnEditSettings();
	afx_msg void OnEditFrames();
	afx_msg void OnSimStart();
	afx_msg void OnSimStop();
	afx_msg void OnSimPause();      // feature 014
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	// ---- feature 010: double-click an ECU to edit its frames ----
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg LRESULT OnFrameEditorClosed(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnEcuLabelChanged(WPARAM wParam, LPARAM lParam); // feature 022
	afx_msg LRESULT OnLoadStartupProject(WPARAM wParam, LPARAM lParam); // feature 024
	afx_msg void OnLogViewSimLog();                                     // feature 025
	afx_msg void OnLogClear();                                          // feature 027
	afx_msg LRESULT OnLogViewerClosed(WPARAM wParam, LPARAM lParam);    // feature 025
	afx_msg void OnAnalysisLatency();                                   // feature 028
	afx_msg void OnAnalysisBusLoad();
	afx_msg LRESULT OnLatencyClosed(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnBusLoadClosed(WPARAM wParam, LPARAM lParam);
	afx_msg void OnAnalysisTxWait();                                    // feature 029
	afx_msg void OnAnalysisJitter();
	afx_msg LRESULT OnTxWaitClosed(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnJitterClosed(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();

public:
	// Number of ECUs drawn on the bus diagram (feature 003). Clamped to 0..16.
	// Callers must only call this after a change was accepted: a cancelled or
	// failed change must leave the diagram untouched (003 FR-010).
	void SetEcuCount(int ecuCount);

	// Feature 024: project file given on the command line; set before DoModal
	// and loaded once the window is shown.
	void SetStartupProject(const CString& path) { m_startupProject = path; }

private:
	void DrawBusDiagram(CDC& dc);
	CRect GetDiagramRect() const;
	bool AskSavePath(CString& path);
	bool SaveProjectTo(const CString& path); // 023: writes; on success it becomes the project file
	bool LoadProjectFrom(const CString& path); // 024: menu, toolbar and startup share the loading
	void SetProjectPath(const CString& path);  // 024: the only place m_projectPath changes; updates the title
	bool WriteResults(const std::vector<CanSim::LogRow>& rows, const CanSim::Network& runNetwork,
		const CanSim::Network& endState, CString& logPath, CString& summaryPath, CString& startupPath);
	void FinishReplay();
	void StopReplayTimer();
	void EndRun();
	void UpdateTopMenus();
	long long CurrentSimTimeUs() const;

	int m_ecuCount = 2; // 004 FR-006 initial value

	// Project (004 data-model.md): settings + every node/frame definition.
	CanSim::ProjectSettings m_settings;
	CanSim::Network m_definitions;
	CString m_projectPath; // empty until the project is loaded or saved; change it with SetProjectPath
	CString m_baseTitle;      // 024: caption from the .rc ("CAN Bus Simulator")
	CString m_startupProject; // 024: command line argument, loaded once

	// Run state (004 FR-019). Running only lasts while a frame replay is shown;
	// Paused keeps the run with its clock stopped (feature 014).
	CanSim::RunPhase m_runPhase = CanSim::RunPhase::Idle;
	bool InRun() const { return m_runPhase != CanSim::RunPhase::Idle; } // Running or Paused
	CanSim::Network m_runNetwork;             // network of the current run
	std::vector<CanSim::LogRow> m_runRows;    // full result of the current run
	CanSim::Network m_runEndState;            // engine state after the full run
	// Running a simulation on screen (feature 005): the result is computed at
	// Start, then the simulation time advances with the real elapsed time x
	// 2^Speed until the end, whether the frame display is on or off.
	std::vector<CanSim::FrameEvent> m_events;
	std::unique_ptr<CanSim::ActiveFrameCursor> m_cursor;
	CanSim::ActiveFrames m_active;   // frames to highlight at this refresh
	LARGE_INTEGER m_replayStartQpc = {};
	LARGE_INTEGER m_qpcFrequency = {};
	long long m_prevSimUs = 0;
	long long m_curSimUs = 0;
	UINT_PTR m_replayTimer = 0;

	// ---- feature 009: log view under the bus diagram ----
	CRect GetLogRect() const;      // whole grey area (client coordinates)
	CRect GetLogTextRect() const;  // grey area minus the scroll bar
	int LogLineHeight() const;
	int LogVisibleRows() const;
	void DrawLogView(CDC& dc);
	CString FormatLogLine(const CanSim::LogLine& line) const;
	void ClearLog();
	void ShowLogUpTo(long long simUs);
	void SetLogShown(std::size_t shown);
	void ScrollLogTo(long long top);
	void UpdateLogScroll();

	std::vector<CanSim::LogLine> m_logLines; // every row of the last run, in time order
	std::size_t m_logShown = 0;              // rows already shown (from the front)
	std::size_t m_logTop = 0;                // first visible row
	CFont m_logFont;

	// ---- feature 010: modeless data frame editor ----
	bool IsEditingFrames() const; // editor open, or waiting to reopen it
	void OpenFrameEditor(int ecu);
	void CloseFrameEditor();      // also cancels a pending switch

	CFrameEditDlg* m_frameEditor = nullptr; // set back to nullptr by the closed notice

	// ---- feature 025: modeless simulation log viewer ----
	void CloseLogViewer();
	CLogViewDlg* m_logViewer = nullptr;     // set back to nullptr by the closed notice
	void CloseResultWindows();              // 028: the log window and both analysis windows
	bool RunAnalysis(const std::function<void(const std::vector<CanSim::LogRecord>&)>& analyze); // 029 revision 1
	CString ProjectFileName() const;
	CLatencyDlg* m_latencyDlg = nullptr;    // 028, cleared by the closed notices
	CBusLoadDlg* m_busLoadDlg = nullptr;
	CTxWaitDlg* m_txWaitDlg = nullptr;      // feature 029
	CJitterDlg* m_jitterDlg = nullptr;
	int m_pendingEcu = 0;                   // ECU to open when the switch timer fires

	// ---- feature 011: grey ECU after a double-click, borders ----
	void StartGray(int ecu);
	void ClearGray();
	void DrawLogFrame(CDC& dc);
	void DrawAreaLabel(CDC& dc, const CRect& rect, UINT textId, int dpi); // feature 020

	int m_grayEcu = 0; // ECU drawn grey for a moment after a double-click, 0 = none

	// ---- feature 012 ----
	void ResetDisplaysForRun(); // empty log view and diagram, painted at once

	// ---- feature 014: pause / resume and the toolbar ----
	int ContentTop() const;                      // toolbar height; the diagram and the log view start here
	CRect GetToolbarRect() const;
	CanSim::SimCommandStates SimStates() const;  // Start / Pause / Stop for the menu and the toolbar
	void DrawSimToolbar(CDC& dc);
	void InvalidateToolbar();
	void UpdateSimControls();                    // menu bar + toolbar after a change of phase or editing
	int ToolbarButtonAt(CPoint point) const;     // CanSim::ToolbarButton (0..4) or -1
	bool IsToolbarButtonEnabled(int index) const;
	bool PressToolbarButton(CPoint point);
	void ReleaseToolbarButton();
	bool AdvanceReplay();                        // one refresh; false when the run finished
	void ResumeReplay();

	long long m_simBaseUs = 0;   // simulated time at the last resume (0 for a new run)
	int m_pressedButton = -1;    // toolbar button (CanSim::ToolbarButton) being pressed, -1 = none
	bool m_pressedInside = false; // the pointer is still over the pressed button

	// ---- feature 015: speed slider on the toolbar ----
	bool PressSpeedSlider(CPoint point);
	void EndSpeedDrag();
	void SetSpeed(int speed);   // the only place where m_settings.Speed changes at run time

	bool m_draggingSpeed = false;

	// ---- feature 017: frame display check box and simulated time ----
	void ToggleFrameDisplay();
	bool PressFrameDisplayToggle(CPoint point);
	void SetDisplaySimUs(long long us);

	long long m_displaySimUs = 0; // shown at the right end of the toolbar

	// ---- feature 018: flow animation over the frame display ----
	void DrawFlowEffect(CDC& mem, const CanSim::BusDiagramLayout& layout,
		const std::vector<std::pair<int, int>>& flows, int dpi);

	long long m_flowElapsedUs = 0;   // animation clock: real time while Running only
	LARGE_INTEGER m_flowLastQpc = {};
	CanSim::FlowPathHold m_flowHold;  // paths kept 0.3 s after they were last seen (revision 1)
};
