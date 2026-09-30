// FrameEntryDlg.cpp
#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "FrameEntryDlg.h"
#include "afxdialogex.h"

#include "CanSim/FrameEditor.h"
#include "CanSim/FrameTiming.h"

namespace
{
	CString LoadRc(UINT id)
	{
		CString s;
		s.LoadString(id);
		return s;
	}

	std::string ToUtf8(const CString& text)
	{
		return std::string(CW2A(text, CP_UTF8));
	}

	std::string ToAscii(const CString& text)
	{
		return std::string(CW2A(text));
	}

	// Number of characters (code points) of a UTF-16 string.
	int CountChars(const CString& text)
	{
		int count = 0;
		for (int i = 0; i < text.GetLength(); ++i)
		{
			const wchar_t c = text[i];
			if (c < 0xDC00 || c > 0xDFFF) // skip the low half of a surrogate pair
				++count;
		}
		return count;
	}

	UINT FieldControl(CanSim::FrameField field)
	{
		switch (field)
		{
		case CanSim::FrameField::Name: return IDC_EDIT_FRAME_NAME;
		case CanSim::FrameField::Id: return IDC_EDIT_FRAME_ID;
		case CanSim::FrameField::Dlc: return IDC_EDIT_FRAME_DLC;
		case CanSim::FrameField::Period: return IDC_EDIT_FRAME_PERIOD;
		default: return IDC_EDIT_FRAME_OFFSET;
		}
	}

	UINT FieldLabel(CanSim::FrameField field)
	{
		switch (field)
		{
		case CanSim::FrameField::Name: return IDS_FIELD_NAME;
		case CanSim::FrameField::Id: return IDS_FIELD_ID;
		case CanSim::FrameField::Dlc: return IDS_FIELD_DLC;
		case CanSim::FrameField::Period: return IDS_FIELD_PERIOD;
		default: return IDS_FIELD_OFFSET;
		}
	}

	CString DescribeIssue(const CanSim::FrameIssue& issue)
	{
		using CanSim::FrameProblem;
		if (issue.Problem == FrameProblem::ListFull)
			return LoadRc(IDS_PROBLEM_LIST_FULL);

		CString reason;
		switch (issue.Problem)
		{
		case FrameProblem::Empty: reason = LoadRc(IDS_PROBLEM_EMPTY); break;
		case FrameProblem::NotANumber: reason = LoadRc(IDS_PROBLEM_NOT_NUMBER); break;
		case FrameProblem::TooLong: reason = LoadRc(IDS_PROBLEM_TOO_LONG); break;
		case FrameProblem::OffsetNotBelowPeriod: reason = LoadRc(IDS_PROBLEM_OFFSET); break;
		case FrameProblem::Duplicate:
			reason.Format(LoadRc(IDS_PROBLEM_DUPLICATE), issue.DuplicateNodeId, issue.DuplicateIndex + 1);
			break;
		default: // OutOfRange
			reason = LoadRc(issue.Field == CanSim::FrameField::Id ? IDS_PROBLEM_RANGE_ID
				: issue.Field == CanSim::FrameField::Dlc ? IDS_PROBLEM_RANGE_DLC
				: IDS_PROBLEM_RANGE_PERIOD);
			break;
		}
		return LoadRc(FieldLabel(issue.Field)) + _T(": ") + reason;
	}

	CString FormatMs(long long us)
	{
		CString s;
		if (us % 1000 == 0)
			s.Format(_T("%I64d"), us / 1000);
		else
			s.Format(_T("%.3f"), static_cast<double>(us) / 1000.0);
		return s;
	}
}

CFrameEntryDlg::CFrameEntryDlg(const CanSim::Network& definitions, int nodeId, int editIndex,
	long long bitRateBps, CWnd* pParent)
	: CDialogEx(IDD_FRAME_ENTRY, pParent)
	, m_definitions(definitions)
	, m_nodeId(nodeId)
	, m_editIndex(editIndex)
	, m_bitRateBps(bitRateBps)
{
}

BEGIN_MESSAGE_MAP(CFrameEntryDlg, CDialogEx)
	ON_EN_CHANGE(IDC_EDIT_FRAME_ID, &CFrameEntryDlg::OnIdOrDlcChanged)
	ON_EN_CHANGE(IDC_EDIT_FRAME_DLC, &CFrameEntryDlg::OnIdOrDlcChanged)
END_MESSAGE_MAP()

BOOL CFrameEntryDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_FRAME_NAME))->SetLimitText(CanSim::kMaxFrameNameChars);
	SetWindowText(LoadRc(m_editIndex < 0 ? IDS_FRAME_TITLE_ADD : IDS_FRAME_TITLE_EDIT));

	const CanSim::Node* node = CanSim::FindNodeDefinition(m_definitions, m_nodeId);
	if (m_editIndex >= 0 && node != nullptr && m_editIndex < static_cast<int>(node->Frames.size()))
	{
		const CanSim::FrameDefinition& f = node->Frames[static_cast<std::size_t>(m_editIndex)];
		CString text;
		SetDlgItemText(IDC_EDIT_FRAME_NAME, CString(CA2W(f.Name.c_str(), CP_UTF8)));
		text.Format(_T("0x%03X"), f.CanId);
		SetDlgItemText(IDC_EDIT_FRAME_ID, text);
		text.Format(_T("%u"), f.Dlc);
		SetDlgItemText(IDC_EDIT_FRAME_DLC, text);
		SetDlgItemText(IDC_EDIT_FRAME_PERIOD, FormatMs(f.PeriodUs));
		SetDlgItemText(IDC_EDIT_FRAME_OFFSET, FormatMs(f.InitialOffsetUs));
	}
	else
	{
		SetDlgItemText(IDC_EDIT_FRAME_OFFSET, _T("0")); // 006 FR-005: offset starts at 0
	}
	UpdateBitLength();
	return TRUE;
}

void CFrameEntryDlg::OnIdOrDlcChanged()
{
	UpdateBitLength();
}

// Bit length (stuff bits included) and transmission time of the frame being
// typed, data = Dlc x 0xFF (006 FR-011). "-" while ID or DLC is invalid.
void CFrameEntryDlg::UpdateBitLength()
{
	CString idText, dlcText;
	GetDlgItemText(IDC_EDIT_FRAME_ID, idText);
	GetDlgItemText(IDC_EDIT_FRAME_DLC, dlcText);
	idText.Trim();
	dlcText.Trim();
	if (idText.Left(2).CompareNoCase(_T("0x")) == 0)
		idText = idText.Mid(2);

	const bool idOk = !idText.IsEmpty() && idText.GetLength() <= 3 &&
		idText.SpanIncluding(_T("0123456789abcdefABCDEF")) == idText;
	const bool dlcOk = !dlcText.IsEmpty() && dlcText.GetLength() <= 1 &&
		dlcText.SpanIncluding(_T("0123456789")) == dlcText;
	const unsigned id = idOk ? static_cast<unsigned>(_tcstoul(idText, nullptr, 16)) : 0x800;
	const unsigned dlc = dlcOk ? static_cast<unsigned>(_ttoi(dlcText)) : 9;

	CString shown = _T("-");
	if (id <= 0x7FF && dlc <= 8)
	{
		const CanSim::FrameTimingResult timing =
			CanSim::ComputeFrameTiming(id, dlc, CanSim::MakeFrameData(dlc), m_bitRateBps);
		CString us;
		if (timing.TxDurationNs % 1000 == 0)
			us.Format(_T("%I64d"), timing.TxDurationNs / 1000);
		else
			us.Format(_T("%.3f"), static_cast<double>(timing.TxDurationNs) / 1000.0);
		shown.Format(LoadRc(IDS_FRAME_BITS), timing.TotalBits, (LPCTSTR)us);
	}
	SetDlgItemText(IDC_STATIC_FRAME_BITS, shown);
}

void CFrameEntryDlg::OnOK()
{
	CString name, id, dlc, period, offset;
	GetDlgItemText(IDC_EDIT_FRAME_NAME, name);
	GetDlgItemText(IDC_EDIT_FRAME_ID, id);
	GetDlgItemText(IDC_EDIT_FRAME_DLC, dlc);
	GetDlgItemText(IDC_EDIT_FRAME_PERIOD, period);
	GetDlgItemText(IDC_EDIT_FRAME_OFFSET, offset);

	CanSim::FrameInput input;
	input.Name = ToUtf8(name);
	input.NameChars = CountChars(name);
	input.IdText = ToAscii(id);
	input.DlcText = ToAscii(dlc);
	input.PeriodMsText = ToAscii(period);
	input.OffsetMsText = ToAscii(offset);

	CanSim::FrameDefinition frame;
	const std::vector<CanSim::FrameIssue> issues =
		CanSim::ValidateFrame(m_definitions, m_nodeId, m_editIndex, input, frame);
	if (!issues.empty())
	{
		// 006 FR-006: show every problem, keep the input, focus the first one.
		CString lines;
		for (const CanSim::FrameIssue& issue : issues)
			lines += DescribeIssue(issue) + _T("\n");
		CString message;
		message.Format(LoadRc(IDS_FRAME_INVALID), (LPCTSTR)lines);
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		CWnd* first = GetDlgItem(FieldControl(issues.front().Field));
		first->SetFocus();
		static_cast<CEdit*>(first)->SetSel(0, -1);
		return;
	}

	Frame = frame;
	CDialogEx::OnOK();
}
