
// MFCApplication1Dlg.h : ヘッダー ファイル
//

#pragma once

#include "CanSim/CanTypes.h"
#include "CanSim/FrameEvents.h"
#include "CanSim/ReplayClock.h"

#include <memory>
#include <vector>


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
	afx_msg void OnFileLoadProject();
	afx_msg void OnFileSaveProject();
	afx_msg void OnEditSettings();
	afx_msg void OnEditFrames();
	afx_msg void OnEditFrameDisplay();
	afx_msg void OnSimStart();
	afx_msg void OnSimStop();

public:
	// Number of ECUs drawn on the bus diagram (feature 003). Clamped to 0..16.
	// Callers must only call this after a change was accepted: a cancelled or
	// failed change must leave the diagram untouched (003 FR-010).
	void SetEcuCount(int ecuCount);

private:
	void DrawBusDiagram(CDC& dc);
	CRect GetDiagramRect() const;
	bool AskSavePath(CString& path);
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
	CString m_projectPath; // empty until the project is loaded or saved

	// Run state (004 FR-019). Running only lasts while a frame replay is shown.
	enum class RunState { Idle, Running };
	RunState m_runState = RunState::Idle;
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
};
