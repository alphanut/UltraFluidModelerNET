// Copyright (C) SAS NET Azure Rangers
// All rights reserved.

// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "RibbonListButton.h"
#include "OutputWnd.h"
#include "PropertiesWnd.h"
#include "CalendarBar.h"
#include "ClassView.h"
#include "FileView.h"
#include "Resource.h"
#include "Element.h"
#include "Modeler1Doc.h"
#include "Modeler1View.h"
#include "ElementContainer.h"
#include "..\resource.h"

class AFX_EXT_CLASS COutlookBar : public CMFCOutlookBar
{
	virtual BOOL AllowShowOnPaneMenu() const { return TRUE; }
	virtual void GetPaneName(CString& strName) const { BOOL bNameValid = strName.LoadString(IDS_OUTLOOKBAR); ASSERT(bNameValid); if (!bNameValid) strName.Empty(); }
};

class AFX_EXT_CLASS CMainFrame : public CMDIFrameWndEx
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	void LogDebug(CString message);

// Internals Operations
protected:
	void CreateLists();
	BOOL CreateRibbonBar();
	void CreateDocumentColors();
	void InitMainButton();
	void InitTabButtons();
	BOOL CreateDockingWindows();
	void SetDockingWindowIcons(BOOL bHiColorIcons);
	int FindFocusedOutlookWnd(CMFCOutlookBarTabCtrl** ppOutlookWnd);

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//virtual void AdjustClientArea();

// Operations
public:
	void SetView(CModeler1View * pView);
	void UpdatePropertiesFromObject(std::shared_ptr<CElement> pElement);
	void InitClassView();
	void UpdateClassViewFromObject(std::shared_ptr<CElement> pElement);
	void InitFileView();
	void UpdateFileViewFromObject(std::shared_ptr<CElement> pElement);
	COLORREF GetColorFromColorButton(int nButtonID);
	int GetWidthFromLineWidth(int nButtonID);
	void UpdateRibbonUI(CModeler1View * pView);
	void OnActionElements(CModeler1View* pView);
	void BuildElementsCombo(CModeler1View* pView);
	void SelectElementsCombo(CModeler1View* pView);

// Attributes
public:
	CElementManager* m_pManager;
	void SetManager(CElementManager* pManager);
	CElementManager* GetManager() const
	{
		return m_pManager;
	}

	CModeler1View* GetView() const
	{
		return m_pModelerView;
	}

public:
	// Clipboard objects
	CElementContainer m_clipboard;

// Extra
public:
	CModeler1View* GetActiveView();

protected:
	CMFCRibbonBar     m_wndRibbonBar;
	CMFCRibbonApplicationButton m_MainButton;
	CMFCToolBarImages m_PanelImages;
	CMFCRibbonStatusBar  m_wndStatusBar;
	COutputWnd        m_wndOutput;
	CPropertiesWnd    m_wndProperties;
	CMFCShellTreeCtrl m_wndTree;
	CCalendarBar      m_wndCalendar;
	CClassViewBar	  m_wndClassView;
	CFileViewBar	  m_wndFileView;
	CMFCRibbonFontComboBox* m_pFontCombo;
	CMFCRibbonComboBox* m_pFontSizeCombo;
	CMFCRibbonComboBox* m_pElementsCombo;

private:
	CModeler1View * m_pModelerView;
	// Document colors for demo:
	CList<COLORREF,COLORREF> m_lstMainColors;
	CList<COLORREF,COLORREF> m_lstAdditionalColors;
	CList<COLORREF,COLORREF> m_lstStandardColors;
	CStringArray m_arInfraShapes;
	CStringArray m_arInfraDev;

// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnWindowManager();
	afx_msg void OnApplicationLook(UINT id);
	afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	afx_msg void OnViewProperties();
	afx_msg void OnUpdateViewProperties(CCmdUI* pCmdUI);
	afx_msg void OnViewClassView();
	afx_msg void OnUpdateViewClassView(CCmdUI* pCmdUI);
	afx_msg void OnViewFileView();
	afx_msg void OnUpdateViewFileView(CCmdUI* pCmdUI);
	afx_msg void OnViewBackground();
	afx_msg void OnUpdateViewBackground(CCmdUI* pCmdUI);
	//afx_msg void OnPaint();
	//afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnDatabaseSettings();
	afx_msg void OnDatabaseSearch();
	DECLARE_MESSAGE_MAP()

	CMFCOutlookBarTabCtrl* FindOutlookParent(CWnd* pWnd);
	CMFCOutlookBarTabCtrl* m_pCurrOutlookWnd;
	CMFCOutlookBarPane*    m_pCurrOutlookPage;
};


