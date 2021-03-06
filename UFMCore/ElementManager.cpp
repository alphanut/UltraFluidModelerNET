// Copyright (C) SAS NET Azure Rangers
// All rights reserved.

#include "pch.h"
#include "UFMCore\Modeler1.h"
#include "UFMCore\ElementManager.h"
#include "UFMCore\Modeler1View.h"
#include "UFMCore\MainFrm.h"
#include "UFMCore\DrawingContext.h"
#include "UFMCore\DrawingElements.h"
#include "UFMCore\TabbedView.h"
#include "UFMCore\Modeler1SourceView.h"
#include "UFMCore\XMLData.h"
#include "SharedViews\Dialogs\CDialogSaveDatabase.h"
#include "SharedViews\Dialogs\CDialogLoadDatabase.h"
#include "SharedViews\SQL\SQLiteTools.h"

//
// CElementManager
//

IMPLEMENT_SERIAL(CElementManager, CObject, VERSIONABLE_SCHEMA | 19)

CElementManager::CElementManager()
{
	m_objectId = L"";
	m_lastPoint = CPoint(0, 0);
	m_paperColor = RGB(255, 255, 255); //RGB(242, 242, 200); //RGB(255, 255, 255); //RGB(188, 251, 255);
	m_size = CSize(3000, 3000);

	// Initialize Current UI interaction members
	m_bDrawing = FALSE;
	// Current selected drawing tool = SELECT
	m_type = ElementType::type_select;
	// Current selected shape type from Ribbon = type_unknown
	m_shapeType = ShapeType::unknown;
	m_nDragHandle = 1;
	m_fZoomFactor = 1.0f;
	
	m_bSavingCode = false;
	m_bSizingALine = false;

	m_selectType = SelectType::intuitive;
	m_elementGroup = _T("ElementGroup");

	m_connectorInUse = ConnectorType::connector2;
	m_bDrawRectForConnectionPoint = false;

	m_pDialog = nullptr;
	m_bTextDialogOpen = false;

	m_ShowBackground = true;

	m_diagramId = 0;
	m_diagramName = _T("");

	// Initiate the connection with the Property Window
	ConnectToPropertyGrid();

	std::wstring imagePath = L"Images\\Custom\\background2.png";
	m_ptrImageBackground = make_shared<Image>(CStringW(imagePath.c_str()));

}

CElementManager::CElementManager(const CElementManager& elementManager)
{
	// Gabari Drawing objects
	//m_objectsGabari = elementManager.m_objectsGabari;
	// Drawing objects
	//m_objects = elementManager.m_objects;
	// Selection objects
	//m_selection = elementManager.m_selection;
	// Clipboard objects
	//m_clipboard = elementManager.m_clipboard;
	// Grouped Objects
	m_groups = elementManager.m_groups;
	m_elementGroup = elementManager.m_elementGroup;

	m_paperColor = elementManager.m_paperColor;
	// Page size in logical coordinates
	m_size = elementManager.m_size;
	m_lastPoint = elementManager.m_lastPoint;
	// Current working object
	m_objectId = elementManager.m_objectId;
	// Current Select action
	m_selectMode = elementManager.m_selectMode;
	// Cursor hanlde
	m_nDragHandle = elementManager.m_nDragHandle;
	// Zoom float factor (default 1.0f)
	m_fZoomFactor = elementManager.m_fZoomFactor;

	// Attributes Current UI interaction members
	// Is in drawing...
	m_bDrawing = elementManager.m_bDrawing;
	// Current selected drawing tool
	m_type = elementManager.m_type;
	// Current selected shape type from Ribbon
	m_shapeType = elementManager.m_shapeType;
	m_bSavingCode = elementManager.m_bSavingCode;
	m_selectType = elementManager.m_selectType;

	// Selection 1st point
	m_selectPoint = elementManager.m_selectPoint;
	// Selection Rect
	m_selectionRect = elementManager.m_selectionRect;
	m_clickPoint = elementManager.m_clickPoint;
	m_bSelectionHasStarted = elementManager.m_bSelectionHasStarted;
	pSelectionElement = elementManager.pSelectionElement;
	m_bSizingALine = elementManager.m_bSizingALine;
}

void CElementManager::ConnectToPropertyGrid()
{
	CWnd * p = AfxGetMainWnd();
	CMainFrame * pmf = (CMainFrame *)p;
	pmf->SetManager(this);
}

CElementManager::~CElementManager(void)
{
}

void CElementManager::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		//
		// Set version of file format
		//
		ar.SetObjectSchema(19);

		//CString elementGroup = W2T((LPTSTR)m_elementGroup.c_str());
		//ar << elementGroup;

		ar << m_size;
		ar << m_paperColor;
		ar << m_lastPoint;
	}
	else
	{
		int version = ar.GetObjectSchema();

		if (version >= 9)
		{
			//CString elementGroup;
			//ar >> elementGroup;
			//this->m_elementGroup = T2W((LPTSTR)(LPCTSTR)elementGroup);
		}

		ar >> m_size;
		ar >> m_paperColor;
		ar >> m_lastPoint;
	}

	m_objects.Serialize(this, ar);
}

void CElementManager::FromJSON(const web::json::object& object)
{
	int size_cx = object.at(U("Size_cx")).as_integer();
	int size_cy = object.at(U("Size_cy")).as_integer();
	m_size = CSize(size_cx, size_cy);
	m_paperColor = (COLORREF) (object.at(U("PaperColor")).as_integer());

	web::json::value p = object.at(U("Objects"));
	for (auto iter = p.as_array().begin(); iter != p.as_array().end(); ++iter)
	{
		if (!iter->is_null())
		{
			shared_ptr<CElement> pElement = nullptr;
			pElement = CElement::FromJSON(iter->as_object());
			m_objects.m_objects.push_back(pElement);
		}
	}
}

web::json::value CElementManager::AsJSON() const
{
	web::json::value res = web::json::value::object();
	res[U("Size_cx")] = web::json::value::number(m_size.cx);
	res[U("Size_cy")] = web::json::value::number(m_size.cy);
	res[U("PaperColor")] = web::json::value::number((int)m_paperColor);

	web::json::value Objects = web::json::value::array(m_objects.m_objects.size());
	int idx = 0;
	for (auto iter = m_objects.m_objects.begin(); iter != m_objects.m_objects.end(); iter++)
	{
		shared_ptr<CElement> pElement = *iter;
		Objects[idx++] = pElement->AsJSON();
	}

	res[U("Objects")] = Objects;
	return res;
}

void CElementManager::RemoveSelectedObjects(CModeler1View * pView)
{
	m_objects.Remove(m_selection);
	SelectNone();

	// Update ClassView & FileView
	UpdateClassView();//pNewElement);
	UpdateFileView();//pNewElement);

	Invalidate(pView);
}

void CElementManager::OnFont(CModeler1View * pView)
{
	USES_CONVERSION;

	CMFCRibbonBar* pRibbon = ((CMainFrame*) pView->GetTopLevelFrame())->GetRibbonBar();

	CMFCRibbonFontComboBox* pFontCombo = DYNAMIC_DOWNCAST(CMFCRibbonFontComboBox, pRibbon->FindByID(ID_FONT_FONT));
	if (pFontCombo == NULL)
	{
		return;
	}

	CString fontName = pFontCombo->GetEditText();
	if (pFontCombo->FindItem(fontName) == -1)
	{
		// Restore current name:
		pFontCombo->SelectItem(10);
		CString strWarning;
		strWarning.Format(_T("The font %s does not exits on your system"), fontName);
		AfxMessageBox(strWarning, MB_OK | MB_ICONWARNING);
		return;
	}

	const CMFCFontInfo* pDesc = pFontCombo->GetFontDesc();

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_fontName = T2W((LPTSTR)(LPCTSTR)fontName);
	UpdatePropertyGrid(pView, pElement);
	// Redraw the element
	//InvalObj(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;
		pObj->m_fontName = T2W((LPTSTR)(LPCTSTR)fontName);
		// Redraw the element
		InvalObj(pView, pObj);
	}

}

void CElementManager::OnFontSize(CModeler1View * pView)
{
	CMFCRibbonBar* pRibbon = ((CMainFrame*) pView->GetTopLevelFrame())->GetRibbonBar();

	CMFCRibbonComboBox* pFontCombo = DYNAMIC_DOWNCAST(CMFCRibbonComboBox, pRibbon->FindByID(ID_FONT_FONTSIZE));
	if (pFontCombo == NULL)
	{
		return;
	}

	CString fontSize = pFontCombo->GetEditText();
	int iFontSize = _ttoi(fontSize);

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_fontSize = iFontSize;
	UpdatePropertyGrid(pView, pElement);
	// Redraw the element
	//InvalObj(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;
		pObj->m_fontSize = iFontSize;
		// Redraw the element
		InvalObj(pView, pObj);
	}

}

void CElementManager::OnEditCut(CModeler1View * pView)
{
	// the clipboard is cleared
	//m_clipboard.RemoveAll();
	CMainFrame* pMainFrame = (CMainFrame*)AfxGetMainWnd();
	pMainFrame->m_clipboard.RemoveAll();

	// the current selection is cleared
	RemoveSelectedObjects(pView);
}

void CElementManager::OnEditCopy(CModeler1View * pView)
{
	//m_clipboard.RemoveAll();
	////m_selection.ChangeInnerAttributes();
	//m_clipboard.Clone(m_selection);
	////m_clipboard.ChangeInnerAttributes();
	CMainFrame* pMainFrame = (CMainFrame*)AfxGetMainWnd();
	pMainFrame->m_clipboard.RemoveAll();
	pMainFrame->m_clipboard.Clone(m_selection);
}

void CElementManager::OnEditPaste(CModeler1View * pView)
{
	//m_objects.Clone(m_clipboard);
	//for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_clipboard.m_objects.begin(); itSel != m_clipboard.m_objects.end(); itSel++)
	//{
	//	std::shared_ptr<CElement> pElement = *itSel;
	//	Select(pElement);
	//}

	CMainFrame* pMainFrame = (CMainFrame*)AfxGetMainWnd();

	m_objects.Clone(pMainFrame->m_clipboard);
	for( vector<std::shared_ptr<CElement>>::const_iterator itSel = pMainFrame->m_clipboard.m_objects.begin() ; itSel!= pMainFrame->m_clipboard.m_objects.end() ; itSel++ )
	{
		std::shared_ptr<CElement> pElement = *itSel;
		Select(pElement);
	}

	m_selectMode = SelectMode::move;

	//m_clipboard.RemoveAll();

	Invalidate(pView);
}

void CElementManager::MoveToFront(CModeler1View * pView)
{
	m_objects.MoveToFront(m_selection);
	Invalidate(pView);
}

void CElementManager::MoveForward(CModeler1View * pView)
{
	m_objects.MoveForward(m_selection);
	Invalidate(pView);
}

void CElementManager::MoveBackward(CModeler1View * pView)
{
	m_objects.MoveBackward(m_selection);
	Invalidate(pView);
}

void CElementManager::MoveToBack(CModeler1View * pView)
{
	m_objects.MoveToBack(m_selection);
	Invalidate(pView);
}

bool CElementManager::HasSelection()
{
	if( m_selection.GetCount() > 0 )
		return true;
	else
		return false;
}

bool CElementManager::IsSelected(CElement* pElement)
{
	std::shared_ptr<CElement> ptr = m_selection.FindElement(pElement->m_objectId);
	if (ptr != nullptr)
		return true;
	else
		return false;
}

bool CElementManager::IsSelected(std::shared_ptr<CElement> pElement)
{
	std::shared_ptr<CElement> ptr = m_selection.FindElement(pElement->m_objectId);
	if (ptr != nullptr)
		return true;
	else
		return false;
}

void CElementManager::SelectNone()
{
	CMainFrame* pMainFrame = (CMainFrame*)AfxGetMainWnd();
	CModeler1View* pView = pMainFrame->GetActiveView();

	for (shared_ptr<CElement> pElement : m_selection.m_objects)
	{
		pElement->m_bDrawCaret = false;
	}

	m_selection.RemoveAll();
	//HideCaret(pView->m_hWnd);
	HideAllEditControls();
}

bool CElementManager::Select(std::shared_ptr<CElement> pElement)
{
	m_selection.AddTail(pElement);
		
	if (m_pDialog != nullptr)
	{
		if (IsTextDialogOpen() == true) 
		{
			//m_pDialog->ShowWindow(SW_SHOW);
			m_pDialog->UpdateData(TRUE);
			if (pElement->IsLine() == true)
			{
				m_pDialog->m_Text.SetWindowText(pElement->m_textConnector1.c_str());
				m_pDialog->m_Text2.SetWindowText(pElement->m_textConnector2.c_str());
				m_pDialog->m_Text2.SetReadOnly(FALSE);
			}
			else
			{
				m_pDialog->m_Text.SetWindowText(pElement->m_text.c_str());
				m_pDialog->m_Text2.SetWindowText(_T(""));
				m_pDialog->m_Text2.SetReadOnly(TRUE);
			}
			m_pDialog->m_pElement = pElement;
			m_bTextDialogOpen = true;
		}
	}

	if (m_selection.GetCount() == 1 && pElement->IsText()) //m_shapeType == ShapeType::text)
	{
		pElement->m_bDrawCaret = true;
	}
	
	return true;
}

bool CElementManager::Deselect(std::shared_ptr<CElement> pElement)
{
	m_selection.Remove(pElement);
	pElement->m_bDrawCaret = false;
	//HideCaret(pElement->m_pView->m_hWnd);
	return true;
}

void CElementManager::ViewToManager(CModeler1View * pView, CPoint & point, CElement* pElement)
{
	// CScrollView changes the viewport origin and mapping mode.
	// It's necessary to convert the point from device coordinates
	// to logical coordinates, such as are stored in the document.
	//CClientDC dc(pView);
	//pView->OnPrepareDC(&dc, NULL);
	//dc.DPtoLP(&point);

	CString str;
	CPoint scrollPoint = pView->GetScrollPosition();
	str.Format(_T("scroll point {%d,%d}"), scrollPoint.x, scrollPoint.y);
	//pView->LogDebug(str);

	point.operator+=(scrollPoint);

	CClientDC dc(pView);
	Graphics graphics(dc.m_hDC);

	if (pElement != nullptr)
	{
		Matrix matrix;
		CPoint pt = pElement->m_rect.CenterPoint();
		PointF point;
		point.X = pt.x;
		point.Y = pt.y;
		matrix.RotateAt(pElement->m_rotateAngle, point);
		graphics.SetTransform(&matrix);
	}
	
	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);
	Point points[1] = {Point(point.x, point.y)};
	// Transform the points in the array from world to page coordinates.
	graphics.TransformPoints(
		CoordinateSpaceWorld, 
		CoordinateSpaceDevice, 
		points, 
		1);
	point.x = points[0].X;
	point.y = points[0].Y;
}

void CElementManager::ViewToManager(CModeler1View * pView, CRect & rect, CElement* pElement)
{
	// CScrollView changes the viewport origin and mapping mode.
	// It's necessary to convert the point from device coordinates
	// to logical coordinates, such as are stored in the document.
	//CClientDC dc(pView);
	//pView->OnPrepareDC(&dc, NULL);
	//dc.DPtoLP(&rect);

	CClientDC dc(pView);
	Graphics graphics(dc.m_hDC);

	if (pElement != nullptr)
	{
		Matrix matrix;
		CPoint pt = pElement->m_rect.CenterPoint();
		PointF point;
		point.X = pt.x;
		point.Y = pt.y;
		matrix.RotateAt(pElement->m_rotateAngle, point);
		graphics.SetTransform(&matrix);
	}

	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);
	CPoint point1 = rect.TopLeft();
	CPoint point2 = rect.BottomRight();
	Point points[2] = { Point((int) point1.x, (int) point1.y), 
						Point((int) point2.x, (int) point2.y) };
	// Transform the points in the array from world to page coordinates.
	graphics.TransformPoints(
		CoordinateSpaceWorld, 
		CoordinateSpaceDevice, 
		points, 
		2);

	point1.x = points[0].X;
	point1.y = points[0].Y;
	point2.x = points[1].X;
	point2.y = points[1].Y;
	rect.SetRect(point1, point2);
}

void CElementManager::ManagerToView(CModeler1View * pView, CPoint & point, CElement* pElement)
{
	//CClientDC dc(pView);
	//pView->OnPrepareDC(&dc, NULL);
	//dc.LPtoDP(&point);

	CClientDC dc(pView);
	Graphics graphics(dc.m_hDC);

	if (pElement != nullptr)
	{
		Matrix matrix;
		CPoint pt = pElement->m_rect.CenterPoint();
		PointF point;
		point.X = pt.x;
		point.Y = pt.y;
		matrix.RotateAt(pElement->m_rotateAngle, point);
		graphics.SetTransform(&matrix);
	}

	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);
	Point points[1] = {Point(point.x, point.y)};
	// Transform the points in the array from world to page coordinates.
	graphics.TransformPoints(
		CoordinateSpaceDevice, 
		CoordinateSpaceWorld, 
		points, 
		1);
	point.x = points[0].X;
	point.y = points[0].Y;
}

void CElementManager::ManagerToView(CModeler1View * pView, CRect & rect, CElement* pElement)
{
	//CClientDC dc(pView);
	//pView->OnPrepareDC(&dc, NULL);
	//dc.LPtoDP(&rect);

	CClientDC dc(pView);
	Graphics graphics(dc.m_hDC);

	if (pElement != nullptr)
	{
		Matrix matrix;
		CPoint pt = pElement->m_rect.CenterPoint();
		PointF point;
		point.X = pt.x;
		point.Y = pt.y;
		matrix.RotateAt(pElement->m_rotateAngle, point);
		graphics.SetTransform(&matrix);
	}

	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);
	CPoint point1 = rect.TopLeft();
	CPoint point2 = rect.BottomRight();
	Point points[2] = { Point((int) point1.x, (int) point1.y), 
						Point((int) point2.x, (int) point2.y) };
	// Transform the points in the array from world to page coordinates.
	graphics.TransformPoints(
		CoordinateSpaceDevice, 
		CoordinateSpaceWorld, 
		points, 
		2);

	point1.x = points[0].X;
	point1.y = points[0].Y;
	point2.x = points[1].X;
	point2.y = points[1].Y;
	rect.SetRect(point1, point2);
}

void CElementManager::PrepareDC(CModeler1View * pView, CDC* pDC, CPrintInfo* pInfo)
{
	return;

	// mapping mode is MM_ANISOTROPIC
	// these extents setup a mode similar to MM_LOENGLISH
	// MM_LOENGLISH is in .01 physical inches
	// these extents provide .01 logical inches

	CSize size = GetSize();

	pDC->SetMapMode(MM_ANISOTROPIC);
	pDC->SetViewportExt(size.cx, size.cy);
	pDC->SetWindowExt(size.cx, size.cy);

	// set the origin of the coordinate system to the center of the page
	CPoint ptOrg;
	ptOrg.x = 0; //GetDocument()->GetSize().cx / 2;
	ptOrg.y = 0; //GetDocument()->GetSize().cy / 2;

	// ptOrg is in logical coordinates
	pDC->OffsetWindowOrg(0, 0); //-ptOrg.x,ptOrg.y);
}

void CElementManager::DrawConnector(Graphics& graphics, std::shared_ptr<CElement> pLineElement, ConnectorType connector)
{
	shared_ptr<CElement> pElement1; // = pLineElement->m_pConnector->m_pElement1;
	if (connector == ConnectorType::connector1)
	{
		pElement1 = pLineElement->m_pConnector->m_pElement1;
	}
	else
	{
		pElement1 = pLineElement->m_pConnector->m_pElement2;
	}

	CPoint point1;
	if (pElement1 == nullptr)
	{
		point1 = pLineElement->m_rect.TopLeft();
	}
	else
	{
		//point1 = pElement1->m_rect.CenterPoint();
		int handle;// = pLineElement->m_connectorDragHandle1;
		if (connector == ConnectorType::connector1)
		{
			handle = pLineElement->m_connectorDragHandle1;
		}
		else
		{
			handle = pLineElement->m_connectorDragHandle2;
		}


		if (handle == 0)
		{
			point1 = pElement1->m_rect.TopLeft();
		}
		else
		{
			point1 = pElement1->GetHandle(handle);
		}

		CPoint point2;
		point2.x = point1.x;
		point2.y = point1.y;
		CRect rect(point1, point2);
		//rect.NormalizeRect();

		Color color;
		if (connector == ConnectorType::connector1)
		{
			color = Color::Yellow;
		}
		else
		{
			color = Color::Blue;
		}


		// Here it's buggy for zomm in/out...
		// Corrected ! Solution was to call ResetTransform because the graphics was already customized !

		graphics.ResetTransform();
		graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);

		SolidBrush colorBrush(color); // Color::DarkOrange);
		graphics.FillRectangle(&colorBrush, rect.left - 3, rect.top - 3, 7, 7);
		Pen  colorPen(Color::Black);
		graphics.DrawRectangle(&colorPen, rect.left - 3, rect.top - 3, 7, 7);

		std::wstring imagePath = L"Images\\Custom\\Connect2.png";
		Image image(CStringW(imagePath.c_str()));
		CPoint p1(rect.left, rect.top);
		CPoint p2(p1.x + image.GetWidth(), p1.y + image.GetHeight());
		graphics.DrawImage(&image, rect.left + 10, rect.top - 3, image.GetWidth(), image.GetHeight());
	}
}

void CElementManager::DrawBackground(CModeler1View* pView, CDC* pDC)
{
	Graphics graphics(pDC->m_hDC);
	CSize size = GetSize();
	//SolidBrush brush(Color(255, 255, 255, 255));
	Color colorLine(255, GetRValue(GetPaperColor()), GetGValue(GetPaperColor()), GetBValue(GetPaperColor()));
	SolidBrush brush(colorLine);
	Rect fillRect(0, 0, size.cx, size.cy);
	graphics.FillRectangle(&brush, fillRect);
}

void CElementManager::DrawPaperLines(CModeler1View* pView, CDC* pDC)
{
	if (m_ShowBackground == false)
	{
		return;
	}
	
	Graphics graphics(pDC->m_hDC);
	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);
	CSize size = GetSize();

	for (int x = 0; x < size.cx; x+= m_ptrImageBackground->GetWidth())
	{
		for (int y = 0; y < size.cy; y+= m_ptrImageBackground->GetHeight())
		{
			Point p1(x,y);
			graphics.DrawImage(m_ptrImageBackground.get(), p1);
		}
	}
}

void CElementManager::Draw(CModeler1View * pView, CDC * pDC)
{
	// GUID for Windows controls
	static unsigned int g_id = 0;

	g_id++;

	// Initialize GDI+ graphics context
	Graphics graphics(pDC->m_hDC);
	// just like that
	//graphics.ScaleTransform(0.75f, 0.75f);
	//graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);

	// Iterate on Line elements
	// if connector1 exists, its draghandle 1 is connector1.centeroint else nothing (its inner value)
	// if connector2 exists, its draghandle 2 is connector2.centeroint else nothing (its inner value)
	// Then the m_rect value is m_rect = CRrect(point1, point2);
	if (m_bSizingALine == false)
	{
		for (vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin(); i != GetObjects().end(); i++)
		{
			std::shared_ptr<CElement> pElement = *i;

			if (pElement->IsLine() == false)
			{
				continue;
			}

			//pElement->m_rect.NormalizeRect();

			shared_ptr<CElement> pElement1 = pElement->m_pConnector->m_pElement1;
			CPoint point1;
			if (pElement1 == nullptr)
			{
				point1 = pElement->m_rect.TopLeft();
			}
			else
			{
				//point1 = pElement1->m_rect.CenterPoint();
				int handle = pElement->m_connectorDragHandle1;
				if (handle == 0)
				{
					point1 = pElement1->m_rect.TopLeft();
				}
				else
				{
					point1 = pElement1->GetHandle(handle);
				}
			}

			shared_ptr<CElement> pElement2 = pElement->m_pConnector->m_pElement2;
			CPoint point2;
			if (pElement2 == nullptr)
			{
				point2 = pElement->m_rect.BottomRight();
			}
			else
			{
				//point2 = pElement2->m_rect.CenterPoint();
				int handle = pElement->m_connectorDragHandle2;
				if (handle == 0)
				{
					point2 = pElement2->m_rect.TopLeft();
				}
				else
				{
					point2 = pElement2->GetHandle(handle);
				}
			}

			CRect rect(point1, point2);
			pElement->m_rect = rect;
		}
	}

	// TODO: add draw code for native data here
	for( vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin() ; i!=GetObjects().end() ; i++ )
	{
		std::shared_ptr<CElement> pElement = *i;	

		// FIXME: Update the view for Property Window
		pElement->m_pView = pView;
		
		// Construct the graphic context for each element
		CDrawingContext ctxt(pElement.get());
		ctxt.m_pGraphics = &graphics;

		//graphics.ResetTransform();
		//graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);
		Matrix matrix;
		CPoint pt = pElement->m_rect.CenterPoint();
		PointF point;
		point.X = pt.x;
		point.Y = pt.y;
		matrix.RotateAt(pElement->m_rotateAngle, point);
		graphics.SetTransform(&matrix);
		//graphics.RotateTransform(pElement->m_rotateAngle, MatrixOrder::MatrixOrderAppend);
		graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);

		//pElement->Draw(pView, pDC);
		pElement->Draw(ctxt);

		// HACK 14072012 : no more SimpleTextElement / Just other text elements except for caption old element
		// caption property is deprecated. be carefull. DO NOT USE m_caption any more !!!
		// informations: 
		//    for m_type that are simple shapes  ; it means it is not about text shapes,
		//    the m_text property can be optional and if it exists,
		//    it should be appended to the rendering area by creating a dedicated object. 
		//    We call it CSimpleTextElement.
		if( pElement->m_text.empty() == false && 
			(pElement->IsText() == false)
			)
		{
			//std::shared_ptr<CElement> pTextElement(new CSimpleTextElement());
			//pTextElement->m_rect = pElement->m_rect;
			//pTextElement->m_text = pElement->m_text;
			//pTextElement->m_textAlign = pElement->m_textAlign;
			//pTextElement->Draw(ctxt);
			std::shared_ptr<CElement> pTextElement(new CTextElement());
			//pTextElement->m_name = pElement->m_name;
			//pTextElement->m_objectId = pElement->m_objectId;
			pTextElement->m_caption = pElement->m_caption;
			pTextElement->m_text = pElement->m_text;
			pTextElement->m_code = pElement->m_code;
			pTextElement->m_image = pElement->m_image;
			pTextElement->m_lineWidth = pElement->m_lineWidth;
			pTextElement->m_pManager = pElement->m_pManager;
			pTextElement->m_pView = pElement->m_pView;
			pTextElement->m_rect = pElement->m_rect;
			pTextElement->m_bColorFill = pElement->m_bColorFill;
			pTextElement->m_bColorLine = pElement->m_bColorLine;
			pTextElement->m_bLineWidth = pElement->m_bLineWidth;
			pTextElement->m_bSolidColorFill = pElement->m_bSolidColorFill;
			pTextElement->m_colorFill = pElement->m_colorFill;
			pTextElement->m_colorLine = pElement->m_colorLine;
			pTextElement->m_textAlign = pElement->m_textAlign;
			pTextElement->m_fontName = pElement->m_fontName;
			pTextElement->m_bFixed = pElement->m_bFixed;
			pTextElement->m_bBold = pElement->m_bBold;
			pTextElement->m_bItalic = pElement->m_bItalic;
			pTextElement->m_bUnderline = pElement->m_bUnderline;
			pTextElement->m_bStrikeThrough = pElement->m_bStrikeThrough;
			//pTextElement->m_code = pElement->m_code;
			pTextElement->m_fontSize = pElement->m_fontSize;
			pTextElement->m_colorText = pElement->m_colorText;
			pTextElement->m_leftMargin = pElement->m_leftMargin;
			pTextElement->m_topMargin = pElement->m_topMargin;
			pTextElement->m_rotateAngle = pElement->m_rotateAngle;

			pTextElement->m_rect.NormalizeRect();
			pTextElement->Draw(ctxt);
		}


		//
		// Draw the connectors
		//

		if (pElement->IsLine() == true) //pElement->m_shapeType == ShapeType::line_right)
		{
			//
			// Draw first text connector1
			//

			std::shared_ptr<CElement> pTextElement(new CTextElement());

			CRect rect = pElement->GetRectTextConnector(ConnectorType::connector1);
			rect.NormalizeRect();

			pTextElement->m_rect = rect;
			pTextElement->m_fontName = _T("Calibri");
			pTextElement->m_fontSize = 12;
			pTextElement->m_colorText = pTextElement->m_colorText;
			pTextElement->m_text = pElement->m_textConnector1;
			pTextElement->m_bColorFill = pElement->m_bColorFill;
			pTextElement->m_colorFill = pElement->m_colorFill;

			pTextElement->Draw(ctxt);

			//
			// Draw second text connector2
			//

			std::shared_ptr<CElement> pTextElement2(new CTextElement());

			CRect rect2 = pElement->GetRectTextConnector(ConnectorType::connector2);
			rect2.NormalizeRect();

			pTextElement2->m_rect = rect2;
			pTextElement2->m_fontName = _T("Calibri");
			pTextElement2->m_fontSize = 12;
			pTextElement2->m_colorText = pTextElement2->m_colorText;
			pTextElement2->m_text = pElement->m_textConnector2;
			pTextElement2->m_bColorFill = pElement->m_bColorFill;
			pTextElement2->m_colorFill = pElement->m_colorFill;

			pTextElement2->Draw(ctxt);
		}


		//
		// Draw the elements names
		//

		if (pElement->m_bShowElementName == true)
		{
			CPoint p1;
			CPoint p2;

			std::shared_ptr<CElement> pTextElement(new CTextElement());

			p1 = CPoint(pElement->m_rect.CenterPoint().x, pElement->m_rect.CenterPoint().y);
			p2 = CPoint(pElement->m_rect.CenterPoint().x + 200, pElement->m_rect.CenterPoint().y + 30);
			CRect rect(p1, p2);
			rect.NormalizeRect();

			pTextElement->m_rect = rect;
			pTextElement->m_fontName = _T("Calibri");
			pTextElement->m_fontSize = 12;
			pTextElement->m_colorText = pTextElement->m_standardShapesTextColor;
			pTextElement->m_text = pElement->m_name;

			pTextElement->Draw(ctxt);
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//
		// Text and Caret stuff
		//
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		if (pElement->IsText() == true)
		{
#ifdef OLD
			CRect rect = pElement->m_rect;

			if (pElement->m_bDrawCaret == true)
			{
				SolidBrush solidBrushTracker(Color::Azure);
				Rect rect2;
				rect2.X = pElement->m_rect.top;
				rect2.Width = pElement->m_rect.Width();
				rect2.Height = 10; // pElement->m_rect.Height();
				graphics.FillRectangle(&solidBrushTracker, rect2);

				pElement->m_pointF.X = rect.left + pElement->m_leftMargin;
				pElement->m_pointF.Y = rect.top + pElement->m_topMargin;

				ShowCaret(pView->m_hWnd);
				pElement->m_lastCaretPoint = pElement->m_pointF;
				SetCaretPos(pElement->m_lastCaretPoint.X, pElement->m_lastCaretPoint.Y);
			}

			pElement->m_pointF.X = rect.left + pElement->m_leftMargin;
			pElement->m_pointF.Y = rect.top + pElement->m_topMargin;

			string data;
			for (auto it = pElement->m_vCharElement.begin(); it != pElement->m_vCharElement.end(); ++it)
			{
				shared_ptr<CCharElement> pCharElement = *it;

				if (pCharElement->m_char == '\n')
				{
					wstring wdata(data.begin(), data.end());

					if (wdata.size() != 0)
					{
						SolidBrush solidBrush(Color::DarkViolet);
						FontFamily fontFamily(pElement->m_fontName.c_str());
						Gdiplus::Font font(&fontFamily, pElement->m_fontSize, FontStyleRegular, UnitPixel);

						PointF ptIn = pElement->m_pointF;
						RectF rectOut;

						// StringFormat object
						StringFormat stringFormat;
						stringFormat.SetLineAlignment(StringAlignment::StringAlignmentNear);
						stringFormat.SetTrimming(StringTrimming::StringTrimmingNone);
						if (pElement->m_textAlign == _T("Left"))
						{
							stringFormat.SetAlignment(StringAlignmentNear);
						}
						else if (pElement->m_textAlign == _T("Center"))
						{
							stringFormat.SetAlignment(StringAlignmentCenter);
						}
						else if (pElement->m_textAlign == _T("Right"))
						{
							stringFormat.SetAlignment(StringAlignmentFar);
						}

						if (pElement->m_textAlign == _T("Left"))
						{
							ptIn.X = rect.left + pElement->m_leftMargin;
						}
						else if (pElement->m_textAlign == _T("Center"))
						{
							ptIn.X = rect.CenterPoint().x + pElement->m_leftMargin;
						}
						else if (pElement->m_textAlign == _T("Right"))
						{
							ptIn.X = rect.right - pElement->m_leftMargin;
						}

						SizeF sizeF(rect.Width(), rect.Height());
						RectF rectDraw(ptIn, sizeF);
						graphics.MeasureString(wdata.c_str(), wdata.size(), &font, ptIn /*rectDraw*/, &stringFormat, &rectOut);
						//graphics.DrawString(wdata.c_str(), -1, &font, rectDraw, &stringFormat, &solidBrush);
						graphics.DrawString(wdata.c_str(), -1, &font, ptIn, &stringFormat, &solidBrush);

						// Store the measure
						//pCharElement->m_rectf = rectDraw;

						//CString str;
						//str.Format(_T("rectOut=%f,%f,%f,%f"), rectOut.GetLeft(), rectOut.GetTop(), rectOut.GetRight(), rectOut.GetBottom());
						//pView->LogDebug(str);

						pElement->m_pointF.X = rectOut.GetRight();
						pElement->m_pointF.Y = rectOut.GetTop();
					}

					if (pElement->m_textAlign == _T("Left"))
					{
						pElement->m_lastCaretPoint.X = rect.left + pElement->m_leftMargin;
					}
					else if (pElement->m_textAlign == _T("Center"))
					{
						pElement->m_lastCaretPoint.X = rect.CenterPoint().x + pElement->m_leftMargin;
					}
					else if (pElement->m_textAlign == _T("Right"))
					{
						pElement->m_lastCaretPoint.X = rect.right - pElement->m_leftMargin;
					}

					pElement->m_pointF.X = rect.left + pElement->m_leftMargin;
					pElement->m_pointF.Y += pElement->m_fontSize;
					pElement->m_lastCaretPoint.Y = pElement->m_pointF.Y;

					data.clear();
				}
				else
				{
					data.insert(data.end(), pCharElement->m_char);
				}
			}

			if (data.size() != 0)
			{
				SolidBrush solidBrush(Color::DarkViolet);
				FontFamily fontFamily(pElement->m_fontName.c_str());
				Gdiplus::Font font(&fontFamily, pElement->m_fontSize, FontStyleRegular, UnitPixel);

				wstring wdata(data.begin(), data.end());

				PointF ptIn = pElement->m_pointF;
				RectF rectOut;

				// StringFormat object
				StringFormat stringFormat;
				stringFormat.SetLineAlignment(StringAlignment::StringAlignmentNear);
				stringFormat.SetTrimming(StringTrimming::StringTrimmingNone);
				if (pElement->m_textAlign == _T("Left"))
				{
					stringFormat.SetAlignment(StringAlignmentNear);
				}
				else if (pElement->m_textAlign == _T("Center"))
				{
					stringFormat.SetAlignment(StringAlignmentCenter);
				}
				else if (pElement->m_textAlign == _T("Right"))
				{
					stringFormat.SetAlignment(StringAlignmentFar);
				}

				if (pElement->m_textAlign == _T("Left"))
				{
					ptIn.X = rect.left + pElement->m_leftMargin;
				}
				else if (pElement->m_textAlign == _T("Center"))
				{
					ptIn.X = rect.CenterPoint().x + pElement->m_leftMargin;
				}
				else if (pElement->m_textAlign == _T("Right"))
				{
					ptIn.X = rect.right - pElement->m_leftMargin;
				}

				SizeF sizeF(rect.Width(), rect.Height());
				RectF rectDraw(ptIn, sizeF);
				graphics.MeasureString(wdata.c_str(), wdata.size(), &font, ptIn /*rectDraw*/, &stringFormat, &rectOut);
				//graphics.DrawString(wdata.c_str(), -1, &font, rectOut /*rectDraw*/, &stringFormat, &solidBrush);
				graphics.DrawString(wdata.c_str(), -1, &font, ptIn, &stringFormat, &solidBrush);

				pElement->m_pointF.X = rectOut.GetRight();
				pElement->m_pointF.Y = rectOut.GetTop();
				pElement->m_lastCaretPoint = pElement->m_pointF;
			}

			if (IsSelected(pElement) && m_selection.GetCount() == 1)
			{
				SetCaretPos(pElement->m_lastCaretPoint.X, pElement->m_lastCaretPoint.Y);
			}
#endif

			if (IsSelected(pElement) && m_selection.GetCount() == 1)
			{
				SetCaretPos(pElement->m_lastCaretPoint.X, pElement->m_lastCaretPoint.Y);
			}
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		//
		// Draw Tracker
		//

		//if( !pDC->IsPrinting() && IsSelected(pObj) )
		//	DrawTracker(pObj, pDC, TrackerState::selected);
		if (pView != NULL && pView->m_bActive && !pDC->IsPrinting() && IsSelected(pElement))
			pElement->DrawTracker(ctxt, TrackerState::selected);
	}

	graphics.ResetTransform();
	Matrix matrix;
	//CPoint pt = pElement->m_rect.CenterPoint();
	//PointF point;
	//point.X = pt.x;
	//point.Y = pt.y;
	//matrix.RotateAt(pElement->m_rotateAngle, point);
	//graphics.SetTransform(&matrix);
	//graphics.RotateTransform(pElement->m_rotateAngle, MatrixOrder::MatrixOrderAppend);
	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);


	//m_bDrawRect = true;
	if (m_bDrawRectForConnectionPoint == true)
	{
		//m_DrawRectForConnectionPoint;
		//SolidBrush solidBrush(Color::Yellow);
		//graphics.FillRectangle(&solidBrush, m_DrawRectForConnectionPoint.left, m_DrawRectForConnectionPoint.top, m_DrawRectForConnectionPoint.Width(), m_DrawRectForConnectionPoint.Height());
		//m_DractRectTracker;
		SolidBrush solidBrushTracker(Color::Green);
		graphics.FillRectangle(&solidBrushTracker, m_DractRectHandleTrackerForConnectionPoint.left, m_DractRectHandleTrackerForConnectionPoint.top, m_DractRectHandleTrackerForConnectionPoint.Width(), m_DractRectHandleTrackerForConnectionPoint.Height());
		Pen pen(Color::Violet);
		graphics.DrawEllipse(&pen, m_DractRectHandleTrackerForConnectionPoint.left, m_DractRectHandleTrackerForConnectionPoint.top, m_DractRectHandleTrackerForConnectionPoint.Width(), m_DractRectHandleTrackerForConnectionPoint.Height());
	}


	// Last....
	// Add connector shape to the handles
	for (vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin(); i != GetObjects().end(); i++)
	{
		std::shared_ptr<CElement> pElement = *i;

		if (pElement->IsLine() == false)
		{
			continue;
		}

		if (pElement->m_bShowConnectors == true)
		{
			DrawConnector(graphics, pElement, ConnectorType::connector1);
			DrawConnector(graphics, pElement, ConnectorType::connector2);
		}
	}

}

void CElementManager::DrawEx(CModeler1View * pView, CDC * pDC)
{
	CDC dc;
	CDC* pDrawDC = pDC;
	CBitmap bitmap;
	CBitmap* pOldBitmap = nullptr;

	// only paint the rect that needs repainting
	CRect client;
	CSize size = GetSize();
	CRect fillRect(0, 0 , size.cx, size.cy);
	
	// version n�1
	// Previous : it was working since Ribbon UI...
	//pDC->GetClipBox(client);
	//ManagerToView(pView, rect);
	
	// version n�2
	// HACK?
	//pView->GetClientRect(client);
	
	// version n�3
	client = fillRect;
	
	CRect rect = client;	
	//rect.NormalizeRect();

	//CString str;
	//str.Format(_T("client=%d,%d,%d,%d"), client.left, client.top, client.right, client.bottom);
	//pView->LogDebug(str);

	if (!pDC->IsPrinting())
	{
		// draw to offscreen bitmap for fast looking repaints
		if (dc.CreateCompatibleDC(pDC))
		{
			if (bitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height()))
			{
				pView->OnPrepareDC(&dc, NULL);
				pDrawDC = &dc;

				// offset origin more because bitmap is just piece of the whole drawing
				dc.OffsetViewportOrg(rect.left, rect.top);
				pOldBitmap = dc.SelectObject(&bitmap);
				//dc.SetBrushOrg(rect.left % 8, rect.top % 8);

				// might as well clip to the same rectangle
				dc.IntersectClipRect(client);
			}
		}
	}

#ifdef DRAW_PAPER_BACKGROUND
	// paint background
	CBrush brush;
	if (!brush.CreateSolidBrush(GetPaperColor()))
		return;
	brush.UnrealizeObject();
	
	//CSize size = GetSize();
	//CRect fillRect(0, 0 , size.cx, size.cy);
	pDrawDC->FillRect(fillRect, &brush);
	//pDrawDC->FillRect(client, &brush);

	//if (!pDC->IsPrinting() && m_bGrid)
	//	DrawGrid(pDrawDC);
#endif
	
	// Background drawing routine call
	DrawBackground(pView, pDrawDC);

	DrawPaperLines(pView, pDrawDC);

	// Main drawing routine call
	Draw(pView, pDrawDC);

	if (pDrawDC != pDC)
	{
		pDC->SetViewportOrg(0, 0);
		pDC->SetWindowOrg(0,0);
		pDC->SetMapMode(MM_TEXT);
		dc.SetViewportOrg(0, 0);
		dc.SetWindowOrg(0,0);
		dc.SetMapMode(MM_TEXT);
		pDC->BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), &dc, 0, 0, SRCCOPY);
		dc.SelectObject(pOldBitmap);
	}

	// Caution, it flicks !
	//pView->LogDebug(_T("CElementManager::DrawEx"));
}

/*
	Private Function RotatePoint(pointToRotate As PointF, centerPoint As PointF, angleInDegrees As Double) As PointF
		Dim angleInRadians As Double = angleInDegrees * (Math.PI / 180)
		Dim cosTheta As Double = Math.Cos(angleInRadians)
		Dim sinTheta As Double = Math.Sin(angleInRadians)
		Return New PointF() With {.X = CInt(cosTheta * (pointToRotate.X - centerPoint.X) - sinTheta * (pointToRotate.Y - centerPoint.Y) + centerPoint.X), _
								  .Y = CInt(sinTheta * (pointToRotate.X - centerPoint.X) + cosTheta * (pointToRotate.Y - centerPoint.Y) + centerPoint.Y)}
	End Function
*/


void CElementManager::OnLButtonDown(CModeler1View* pView, UINT nFlags, const CPoint& cpoint)
{
	// Caution, it flicks !
	//pView->LogDebug(_T("CElementManager::OnLButtonDown"));

	if( m_type == ElementType::type_unknown )
	{
		//FIXME : do we need to handle not implemented objects ?
		return;
	}
	
	//m_bDrawing = true;
	
	CPoint point = cpoint;
	//m_clickPoint = point;
	ViewToManager(pView, point);
	m_clickPoint = point;
	//ManagerToView(pView, m_clickPoint);
	m_lastPoint = point;
	m_selectPoint = point;

	// Debugging
	CString str;
	str.Format(_T("m_clickPoint {%d,%d} / point {%d,%d} / cpoint {%d,%d}"), m_clickPoint.x, m_clickPoint.y, point.x, point.y, cpoint.x, cpoint.y);
	pView->LogDebug(str);

	if( m_type == ElementType::type_select )
	{

		m_selectMode = SelectMode::none;

		// Check for resizing (only allowed on single selections)
		if( HasSelection() &&  m_selection.GetCount() == 1)
		{
			std::shared_ptr<CElement> pElement = m_selection.GetHead();
			// Change cursor look because mouse click is over an object for sizing 
			m_nDragHandle = pElement->HitTest(point, pView, TRUE);
			if (m_nDragHandle != 0)
			{
				m_selectMode = SelectMode::size;
				
				CString str;
				str.Format(_T("m_nDragHandle=%d - selectMode == sized"), m_nDragHandle);
				pView->LogDebug(str);
				//pView->LogDebug(_T("selectMode == sized"));

				if (m_nDragHandle == 2)
				{
					m_connectorInUse = ConnectorType::connector2;
					//pView->LogDebug(_T("LDown Handle=5 => conector2"));
				}
				else
				{
					m_connectorInUse = ConnectorType::connector1;
					//pView->LogDebug(_T("LDown Handle!5 => conector1"));
				}
			}
			else
			{
				m_connectorInUse = ConnectorType::connector2;
				//pView->LogDebug(_T("LDown default => conector2"));
			}
		}

		if( m_selectMode == SelectMode::none )
		{
			// See if the click was on an object
			std::shared_ptr<CElement> pElement = m_objects.ObjectAt(point, m_selectType);
			if( pElement != NULL )
			{
				//if( HasSelection() )
				//{
				//	pView->LogDebug("selection cleared");
				//	SelectNone();
				//}


				pView->LogDebug(_T("object found ->") + pElement->ToString());
				if( IsSelected(pElement) == false )
				{
					if( (nFlags & MK_SHIFT) || (nFlags & MK_CONTROL))
					{
					}
					else
						SelectNone();

					if (pElement->m_bGrouping == false)
					{
						pView->LogDebug(_T("object selected ->") + pElement->ToString());
						m_objectId = pElement->m_objectId;
						Select(pElement);
					}
					else
					{
						for (vector<std::shared_ptr<CElement>>::const_iterator itSel = pElement->m_pElementGroup->m_Groups.begin(); itSel != pElement->m_pElementGroup->m_Groups.end(); itSel++)
						{
							std::shared_ptr<CElement> pObj = *itSel;
							Select(pObj);
						}
					}

					pElement->m_bMoving = true;
				}
				else
				{
					// if shift or control i spressed, unselect the element
					if ((nFlags & MK_SHIFT) || (nFlags & MK_CONTROL))
					{
						Deselect(pElement);
					}
				}

				m_selectMode = SelectMode::move;
				pView->LogDebug(_T("selectMode == move"));

				// Update UI
				UpdateUI(pView, pElement);
				// Redraw
				Invalidate(pView, pElement);
			}
			else
			{
				// See if the click was on an object
				// TRUE -> select and start move if so
				// FALSE -> Click on background, start a net-selection
				// m_selectMode = netSelect;

				if( HasSelection() )
				{
					pView->LogDebug(_T("selection cleared"));
					SelectNone();
					Invalidate(pView, pElement);
				}
				
				//m_selectPoint = point;
				m_selectMode = SelectMode::netselect;
				pView->LogDebug(_T("selectMode == netselect"));
			}
		}
	}
	// We are not in a select operation
	// -> this is a drawing operation
	// We have to create...
	// Create a Drawable Object...
	else
	{

#ifdef VERSION_COMMUNITY
		if (CFactory::g_counter > MAX_SHAPES && IsMyLocalDev() == false)
		{
			AfxMessageBox(_T("Maximum number or shapes reached !\nFor more, please buy the Architect Edition."));
			return;
		}
#endif

		pView->LogDebug(_T("selection cleared"));
		SelectNone();

		std::shared_ptr<CElement> pNewElement = CFactory::CreateElementOfType(m_type, m_shapeType);
		if( m_type == ElementType::type_unknown )
		{
			pView->LogDebug(_T("object not implemented yet ! ->") + pNewElement->ToString());
			return;
		}

		if (m_shapeType == ShapeType::selection)
		{
			m_bSelectionHasStarted = true;
			pSelectionElement = pNewElement;
		}
		
		pNewElement->m_point = point;
		// For plumbing purpose...
		pNewElement->m_pManager = this;
		pNewElement->m_pView = pView;

		// Add an object
		m_objects.AddTail(pNewElement);	
		pView->LogDebug(_T("object created ->") + pNewElement->ToString());
		
		// Store last created object
		m_objectId = pNewElement->m_objectId;

		// Select the new element
		Select(pNewElement);
		pView->LogDebug(_T("object selected ->") + pNewElement->ToString());

		m_selectMode = SelectMode::size;
		pView->LogDebug(_T("selectMode == size"));

		m_nDragHandle = 1;
		m_connectorInUse = ConnectorType::connector2;
		FindAConnectionFor(pNewElement, point, pView, ConnectorType::connector1);

		pView->GetDocument()->SetModifiedFlag();

		// Update ClassView & FileView
		UpdateClassView();//pNewElement);
		UpdateFileView();//pNewElement);

		// Update UI
		UpdateUI(pView, pNewElement);
	}

}

void CElementManager::OnLButtonDblClk(CModeler1View* pView, UINT nFlags, const CPoint& cpoint)
{
	CPoint point = cpoint;
	ViewToManager(pView, point);

	std::shared_ptr<CElement> pElement = m_objects.ObjectAt(point, m_selectType);
	if (pElement != NULL)
	{

		if (m_pDialog == nullptr)
		{
			m_pDialog = new CTextControlDialog();
			m_pDialog->Create(IDD_DIALOG_TEXT, pView);
		}

		m_pDialog->ShowWindow(SW_SHOW);
		m_pDialog->UpdateData(TRUE);
		m_pDialog->m_pElement = pElement;
		m_bTextDialogOpen = true;

		if (pElement->IsLine() == true)
		{
			m_pDialog->m_Text.SetWindowText(pElement->m_textConnector1.c_str());
			m_pDialog->m_Text2.SetWindowText(pElement->m_textConnector2.c_str());
			m_pDialog->m_Text2.SetReadOnly(FALSE);
		}
		else
		{
			m_pDialog->m_Text.SetWindowText(pElement->m_text.c_str());
			m_pDialog->m_Text2.SetWindowText(_T(""));
			m_pDialog->m_Text2.SetReadOnly(TRUE);
		}
	}
}

void CElementManager::DrawSelectionRect(CModeler1View *pView)
{
	Color colorBlack(255, 0, 0, 0);
	Pen penBlack(colorBlack);
	CRect rect = m_selectionRect;
	rect.NormalizeRect();

	shared_ptr<CElement> pElement = m_selection.GetHead();

	CClientDC dc(pView);
	Graphics graphics(dc.m_hDC);
	Matrix matrix;
	CPoint pt = rect.CenterPoint();
	PointF point;
	point.X = pt.x;
	point.Y = pt.y;
	matrix.RotateAt(pElement->m_rotateAngle, point);
	graphics.SetTransform(&matrix);
	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);

	Invalidate(pView);
	graphics.DrawRectangle(&penBlack, rect.left, rect.top, rect.Width(), rect.Height());
}

void CElementManager::OnMouseMove(CModeler1View* pView, UINT nFlags, const CPoint& cpoint)
{
	// Caution, it flicks !
	//pView->LogDebug(_T("CElementManager::OnMouseMove"));

	// a SELECT operation is started
	if( m_selectMode == SelectMode::none )
	{
	}
	
	//if( m_bDrawing == FALSE )
	//	return;

	CPoint point = cpoint;
	CPoint m_movePoint = point;
	ViewToManager(pView, point);
	CPoint lastPoint = point;

	// Debugging
	CString str;
	str.Format(_T("point {%d,%d} / {%d,%d}"), cpoint.x, cpoint.y, point.x, point.y);
	//pView->LogDebug(str);

	std::shared_ptr<CElement> pElement = m_selection.GetHead(); //m_objects.FindElement(m_objectId);
	if( pElement == NULL )
	{
		//pView->LogDebug(_T("FindElement return NULL; return;"));
		return;
	}

	if( m_type == ElementType::type_select )
	{
		if( HasSelection() )
		{
			// Change cursor look temporary just because mouse could be over a shape
			int nHandle = pElement->HitTest(point, pView, true);
			if (nHandle != 0)
			{
				SetCursor(pElement->GetHandleCursor(nHandle));
			}

			if( m_selectMode == SelectMode::move )
			{
				//pView->LogDebug("object selection moved ->" + pElement->ToString());
			
				for( vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin() ; itSel!=m_selection.m_objects.end() ; itSel++ )
				{
					std::shared_ptr<CElement> pObj = *itSel;

					CPoint delta = (CPoint)(point - m_lastPoint);		
					//std::shared_ptr<CElement> pObj = m_selection.GetHead();
					InvalObj(pView, pObj);
					pObj->m_rect += delta;
					pObj->m_point = pObj->m_rect.TopLeft();
					InvalObj(pView, pObj);
				}
				
				pView->GetDocument()->SetModifiedFlag();
			}

			if( m_selectMode == SelectMode::size )
			{
				if( m_nDragHandle != 0)
				{
					//pView->LogDebug("object selection sized ->" + pElement->ToString());

					std::shared_ptr<CElement> pObj = m_selection.GetHead();
					pObj->MoveHandleTo(m_nDragHandle, point, pView);
					//FindAConnectionFor(pElement, point, pView, ConnectorType::connector2);
					FindAConnectionFor(pElement, point, pView, m_connectorInUse);
					InvalObj(pView, pObj);

					pView->GetDocument()->SetModifiedFlag();
				}
			}

		}
	}
	else
	{
		if( m_selectMode == SelectMode::size )
		{
			//pView->LogDebug("obect is under drawing ->" + pElement->ToString());
		
			pElement->m_last = point;
			pElement->InvalidateObj();
			//FindAConnectionFor(pElement, point, pView, ConnectorType::connector2);
			FindAConnectionFor(pElement, point, pView, m_connectorInUse);
			InvalObj(pView, pElement);

			pView->GetDocument()->SetModifiedFlag();
		}
	}	

	m_lastPoint = point;

	// Check for mouse cursor -> sizing/moving
 	if( m_selectMode == SelectMode::size )
	{
		//if (m_nDragHandle != 0)
 		{
			SetCursor(m_selection.GetHead()->GetHandleCursor(m_nDragHandle));
		}
	}

	if( m_type == ElementType::type_select && m_selection.GetCount() == 0)
	{
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	}
}


void CElementManager::OnLButtonUp(CModeler1View* pView, UINT nFlags, const CPoint& cpoint)
{
	CPoint point = cpoint;
	ViewToManager(pView, point);

	// Caution, it flicks !
	//pView->LogDebug(_T("CElementManager::OnLButtonUp"));

	//if( m_selectMode == SelectMode::move || m_selectMode == SelectMode::size )
	{
		// Selection Moving or Sizing is finished. 
		// Nothing to do.
		m_selectMode = SelectMode::none;
	}

	//m_bDrawing = FALSE;

	std::shared_ptr<CElement> pElement = m_objects.FindElement(m_objectId);
	if (pElement == NULL)
		return;

	if (m_type == ElementType::type_select)
	{
		if (HasSelection() && m_selection.GetCount() == 1)
		{
			// Nothing to do...
			pView->LogDebug(_T("object selection finished ->") + pElement->ToString());
		}
	}
	else
	{
		// Finish a drawing operation
		pElement->m_last = point;
		pElement->InvalidateObj();
		pElement->CheckForKeepingAMinimumSize();
		// Switch the view in Select Mode
		m_type = ElementType::type_select;

		pView->LogDebug(_T("object drawing finished ->") + pElement->ToString());
	}

	//
	// We are gonna end a slection rect...
	//

	if (m_bSelectionHasStarted == true)
	{
		CRect rect = pSelectionElement->m_rect;

		// remove the selection element from the objects list
		vector<std::shared_ptr<CElement>>::iterator pos;
		pos = find(m_objects.m_objects.begin(), m_objects.m_objects.end(), pSelectionElement);
		if (pos != m_objects.m_objects.end())
		{
			m_objects.m_objects.erase(pos);
		}

		// remove the selection element from the selection list
		vector<std::shared_ptr<CElement>>::iterator pos2;
		pos2 = find(m_selection.m_objects.begin(), m_selection.m_objects.end(), pSelectionElement);
		if (pos2 != m_selection.m_objects.end())
		{
			m_selection.m_objects.erase(pos2);
		}

		//ViewToManager(pView, rect);
		SelectNone();

		shared_ptr<CElement> pLastSelected = nullptr;
		vector<std::shared_ptr<CElement>> v = m_objects.ObjectsInRectEx(rect, m_selectType); // version Ex : do not select lines with full connector
		if (v.size() != 0)
		{
			for (std::shared_ptr<CElement> pElement : v)
			{
				if (IsSelected(pElement) == false)
				{
					if (pElement->m_bGrouping == false)
					{
						Select(pElement);
						pLastSelected = pElement;
					}
				}
			}
		}

		if (pLastSelected != nullptr)
		{
			// Update UI
			UpdateUI(pView, pLastSelected);
		}

		pSelectionElement = nullptr;
		m_bSelectionHasStarted = false;
	}

	m_bSizingALine = false;

	// Set selectType to default
	m_selectType = SelectType::intuitive;
	m_connectorInUse = ConnectorType::connector2;
	m_bDrawRectForConnectionPoint = false;

	pElement->m_bMoving = FALSE;
	// Update UI
	//UpdateUI(pView, pElement);
	// Redraw
	InvalObj(pView, pElement);	

	m_selectMode = SelectMode::none;
	pView->GetDocument()->SetModifiedFlag();
	pView->GetDocument()->UpdateAllViews(pView);
}

void CElementManager::InvalObj(CModeler1View * pView, std::shared_ptr<CElement> pElement)
{
	CRect rect = pElement->m_rect;
	ManagerToView(pView, rect, pElement.get());
	if( pView->m_bActive && IsSelected(pElement) )
	{
		rect.left -= 4;
		rect.top -= 5;
		rect.right += 5;
		rect.bottom += 4;
	}
	rect.InflateRect(1, 1); // handles CDrawOleObj objects

	pView->InvalidateRect(rect, FALSE);
	//FIXME:
	Invalidate(pView, pElement);
}

void CElementManager::Invalidate(CModeler1View * pView, BOOL erase)
{
	pView->Invalidate(erase);
}

void CElementManager::Invalidate(CModeler1View * pView, std::shared_ptr<CElement> pElement)
{
	pView->Invalidate(FALSE);
}

void CElementManager::Update(CModeler1View * pView, LPARAM lHint, CObject* pHint)
{
	switch (lHint)
	{
	case HINT_UPDATE_WINDOW:    // redraw entire window
		//pView->LogDebug(_T("CElementManager::Update HINT_UPDATE_WINDOW"));
		pView->Invalidate(FALSE);
		break;

	case HINT_UPDATE_DRAWOBJ:   // a single object has changed
		{
			//pView->LogDebug(_T("CElementManager::Update HINT_UPDATE_DRAWOBJ"));
			CElement * p = (CElement *)pHint;
			InvalObj(pView, std::shared_ptr<CElement>(p));
		}
		break;

	case HINT_UPDATE_SELECTION: // an entire selection has changed
		{
			//pView->LogDebug(_T("CElementManager::Update HINT_UPDATE_SELECTION"));
			pView->Invalidate(FALSE);
			//CElementContainer * pList = pHint != NULL ? (CElementContainer*)pHint : &m_selection;
			//POSITION pos = pList->m_objects.GetHeadPosition();
			//while (pos != NULL)
			//{
			//	CElement * pElement = pList->m_objects.GetNext(pos);
			//	InvalObj(pView, pElement);

			//	// Update UI
			//	UpdateUI(pView, pElement);
			//}			
		}
		break;

	case HINT_DELETE_SELECTION: // an entire selection has been removed
		/*
		if (pHint != &m_selection)
		{
			CDrawObjList* pList = (CDrawObjList*)pHint;
			POSITION pos = pList->GetHeadPosition();
			while (pos != NULL)
			{
				CDrawObj* pObj = pList->GetNext(pos);
				InvalObj(pObj);
				Remove(pObj);   // remove it from this view's selection
			}
		}
		*/
		break;

	case HINT_UPDATE_OLE_ITEMS:
		/*
		{
			CDrawDoc* pDoc = GetDocument();
			POSITION pos = pDoc->GetObjects()->GetHeadPosition();
			while (pos != NULL)
			{
				CDrawObj* pObj = pDoc->GetObjects()->GetNext(pos);
				if (pObj->IsKindOf(RUNTIME_CLASS(CDrawOleObj)))
					InvalObj(pObj);
			}
		}
		*/
		break;

	default:
		ASSERT(FALSE);
		break;
	}
}

void CElementManager::UpdateClassView()
{
	std::shared_ptr<CElement> pNullElement;
	UpdateClassView(pNullElement);
	for( vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin() ; i!=GetObjects().end() ; i++ )
	{
		std::shared_ptr<CElement> pElement = *i;
		UpdateClassView(pElement);
	}
}

void CElementManager::UpdateClassView(std::shared_ptr<CElement> pElement)
{
	CWnd * p = AfxGetMainWnd();
	CMainFrame * pmf = (CMainFrame *)p;
	if( pElement == NULL )
		pmf->InitClassView();
	else
		pmf->UpdateClassViewFromObject(pElement);
}

void CElementManager::UpdateFileView()
{
	std::shared_ptr<CElement> pNullElement;
	UpdateFileView(pNullElement);
	for( vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin() ; i!=GetObjects().end() ; i++ )
	{
		std::shared_ptr<CElement> pElement = *i;
		UpdateFileView(pElement);
	}
}

void CElementManager::UpdateFileView(std::shared_ptr<CElement> pElement)
{
	CWnd * p = AfxGetMainWnd();
	CMainFrame * pmf = (CMainFrame *)p;
	if( pElement == NULL )
		pmf->InitFileView();
	else
		pmf->UpdateFileViewFromObject(pElement);
}

void CElementManager::UpdateUI(CModeler1View * pView, std::shared_ptr<CElement> pElement)
{	
	// Update Property Grid
	UpdatePropertyGrid(pView, pElement);
	// Update Ribbon UI
	UpdateRibbonUI(pView, pElement);
	
	pView->GetDocument()->UpdateAllViews(pView);
}

void CElementManager::UpdatePropertyGrid(CModeler1View * pView, std::shared_ptr<CElement> pElement)
{
	CWnd * p = AfxGetMainWnd();
	CMainFrame * pmf = (CMainFrame *)p;
	pmf->UpdatePropertiesFromObject(pElement);
}

void CElementManager::UpdateRibbonUI(CModeler1View * pView, std::shared_ptr<CElement> pElement)
{	
	CWnd * p = AfxGetMainWnd();
	CMainFrame * pmf = (CMainFrame *)p;
	pmf->UpdateRibbonUI(pView);
}

void CElementManager::UpdateFromPropertyGrid(std::wstring objectId, std::wstring name, std::wstring value)
{
	bool bUpdateUI = false;
	std::shared_ptr<CElement> pElement = m_objects.FindElement(objectId);
	if( pElement == NULL )
	{
		return;
	}

	if( name == prop_Name )
	{
		pElement->m_name = value;
		bUpdateUI = true;
	}

	if( name == prop_Caption )
	{
		pElement->m_caption = value;
	}

	if( name == prop_Text )
	{
		pElement->m_text = value;
		if (pElement->IsText())
		{
			pElement->BuildVChar();
		}
	}

	if (name == prop_Comments)
	{
		pElement->m_code = value;
	}

	if (name == prop_Version)
	{
		pElement->m_version = value;
	}

	if (name == prop_Product)
	{
		pElement->m_product = value;
	}

	if (name == prop_Text_Align)
	{
		if (value == _T("None") || value == _T("Left") || value == _T("Center") || value == _T("Right"))
		{
			pElement->m_textAlign = value;
		}
	}

	if (name == prop_Connector1)
	{
		if (value == _T("") || value == _T(""))
		{
			pElement->m_pConnector->m_pElement1 = nullptr;
		}
		else
		{
			pElement->m_pConnector->m_pElement1 = m_objects.FindElementByName(value);
		}
	}

	if (name == prop_Connector2)
	{
		if (value == _T("") || value == _T(""))
		{
			pElement->m_pConnector->m_pElement2 = nullptr;
		}
		else
		{
			pElement->m_pConnector->m_pElement2 = m_objects.FindElementByName(value);
		}
	}

	if (name == prop_Team)
	{
		CElement::m_team = value;
	}
		
	if (name == prop_Authors)
	{
		CElement::m_authors = value;
	}

	if (name == prop_Connector1Handle)
	{
		if (value == _T("") || value == _T("TopLeft") || value == _T("Center") || value == _T("TopCenter")
			|| value == _T("BottomCenter") || value == _T("LeftCenter") || value == _T("RightCenter"))
		{
			pElement->m_connectorDragHandle1 = pElement->DragHandleFromString(value);
		}
	}

	if (name == prop_Connector2Handle)
	{
		if (value == _T("") || value == _T("TopLeft") || value == _T("Center") || value == _T("TopCenter")
			|| value == _T("BottomCenter") || value == _T("LeftCenter") || value == _T("RightCenter"))
		{
			pElement->m_connectorDragHandle2 = pElement->DragHandleFromString(value);
		}
	}

	if (name == prop_Connector1_Text)
	{
		pElement->m_textConnector1 = value;
	}

	if (name == prop_Connector2_Text)
	{
		pElement->m_textConnector2 = value;
	}

	if (name == prop_Image)
	{
		pElement->m_image = value;
	}

	if (name == prop_Font_Name)
	{
		pElement->m_fontName = value;
	}

	if (name == prop_Document)
	{
		pElement->m_document = value;
	}

	if (name == prop_Document_Type)
	{
		if (value == _T("None") || value == _T("File") || value == _T("Folder") || value == _T("Diagram"))
		{
			pElement->m_documentType = pElement->FromString(value);
		}
	}

	if (name == prop_DashLine_Type)
	{
		if (value == _T("Solid") || value == _T("Dash") || value == _T("Dot") || value == _T("DashDot") || value == _T("DashDotDot"))
		{
			pElement->m_dashLineType = pElement->FromStringEx(value);
		}
	}

	if (name == prop_Arrow_Type)
	{
		if (value == _T("None") || value == _T("Left") || value == _T("Right") || value == _T("Left Right"))
		{
			pElement->m_arrowType = pElement->FromStringEx2(value);
		}
	}

	// Some properties could change the UI in class view or file view
	if( bUpdateUI == true )
	{
		this->UpdateClassView();
		this->UpdateFileView();
	}
	InvalObj(pElement->GetView(), pElement);
}

void CElementManager::UpdateFromPropertyGrid(std::wstring objectId, std::wstring name, COLORREF color)
{
	std::shared_ptr<CElement> pElement = m_objects.FindElement(objectId);
	if( pElement == NULL )
	{
		return;
	}

	if (name == prop_Fill_Color)
	{
		pElement->m_colorFill = color;
	}
	
	if (name == prop_Line_Color)
	{
		pElement->m_colorLine = color;
	}

	if (name == prop_Standard_Shapes_Text_Color)
	{
		pElement->m_standardShapesTextColor = color;
	}
	
	if (name == prop_Connector_Shapes_Text_Color)
	{
		pElement->m_connectorShapesTextColor = color;
	}

	UpdateRibbonUI(pElement->GetView(), pElement);
	InvalObj(pElement->GetView(), pElement);
}

void CElementManager::UpdateFromPropertyGrid(std::wstring objectId, std::wstring name, long value)
{
	std::shared_ptr<CElement> pElement = m_objects.FindElement(objectId);
	if( pElement == NULL )
	{
		return;
	}

	if (name == prop_Left)
	{
		pElement->m_rect.left = value;
	}
	
	if (name == prop_Right)
	{
		pElement->m_rect.right = value;
	}
	
	if (name == prop_Top)
	{
		pElement->m_rect.top = value;
	}

	if (name == prop_Bottom)
	{
		pElement->m_rect.bottom = value;
	}

	if (name == prop_Has_Fill_Color)
	{
		pElement->m_bColorFill = value == 0 ? false: true;
	}

	if (name == prop_Is_Fill_Solid_Color)
	{
		pElement->m_bSolidColorFill = value == 0 ? false: true;
	}

	if (name == prop_Has_Line_Color)
	{
		pElement->m_bColorLine = value == 0 ? false: true;
	}

	if (name == prop_Font_Size)
	{
		pElement->m_fontSize = value;
	}

	if (name == prop_Fixed)
	{
		pElement->m_bFixed = value == 0 ? false : true;
	}

	if (name == prop_Left_Margin)
	{
		pElement->m_leftMargin = value;
	}

	if (name == prop_Top_Margin)
	{
		pElement->m_topMargin = value;
	}

	if (name == prop_Rotation_Angle)
	{
		pElement->m_rotateAngle = value;
	}

	if (name == prop_ViewElementName)
	{
		pElement->m_bShowElementName = value == 0 ? false : true;
	}

	if (name == prop_ShowConnectors)
	{
		pElement->m_bShowConnectors = value == 0 ? false : true;
	}

	InvalObj(pElement->GetView(), pElement);
}

void CElementManager::ActivateView(CModeler1View * pView, bool bActivate, CView* pActiveView, CView* pDeactiveView)
{
	// invalidate selections when active status changes
	if (pView->m_bActive != bActivate)
	{
		if( bActivate )  // if becoming active update as if active
			pView->m_bActive = bActivate;
		if( HasSelection() )
			Update(pView, HINT_UPDATE_SELECTION, NULL);
		pView->m_bActive = bActivate;

		// Initiate the connection with the Property Window
		ConnectToPropertyGrid();
		ConnectToMainFrame(pView);
		// Update ClassView & FileView
		UpdateClassView();
		UpdateFileView();
	}

	//((CMainFrame*)AfxGetMainWnd())->UpdateUI(this);
	//((CMainFrame*)AfxGetMainWnd())->UpdateContextTab(this);
}

void CElementManager::ConnectToMainFrame(CModeler1View * pView)
{
	CWnd * p = AfxGetMainWnd();
	CMainFrame * pmf = (CMainFrame *)p;
	pmf->SetView(pView);
}

void CElementManager::DebugDumpObjects(CModeler1View * pView)
{
	pView->LogDebug(_T("Dumping m_objects..."));
	m_objects.DebugDumpObjects(pView);
	pView->LogDebug(_T("Dumping m_selection..."));
	m_selection.DebugDumpObjects(pView);
}

void CElementManager::NoFillColor(CModeler1View* pView)
{
	//POSITION pos = m_selection.m_objects.GetHeadPosition();
	//while (pos != NULL)
	//{
	//	CElement * pElement = m_selection.m_objects.GetNext(pos);
	//	if (pElement->CanChangeFillColor())
	//	{
	//		pElement->m_bColorFill = FALSE;
	//	}
	//}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->CanChangeFillColor())
	{
		pElement->m_bColorFill = FALSE;
	}
	//Invalidate(pView);
	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->CanChangeFillColor())
		{
			pObj->m_bColorFill = FALSE;
			InvalObj(pView, pObj);
		}
	}
}

void CElementManager::FillColor(CModeler1View * pView)
{
	COLORREF color = ((CMainFrame*)AfxGetMainWnd())->GetColorFromColorButton(ID_FORMAT_FILLCOLOR);

	if (color == (COLORREF) -1)
	{
		return;
	}

	//POSITION pos = m_selection.m_objects.GetHeadPosition();
	//while (pos != NULL)
	//{
	//	CElement * pElement = m_selection.m_objects.GetNext(pos);
	//	if (pElement->CanChangeFillColor())
	//	{
	//		pElement->m_colorFill = color;
	//		pElement->m_bColorFill = true;
	//	}
	//}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->CanChangeFillColor())
	{
		pElement->m_colorFill = color;
		pElement->m_bColorFill = true;
	}
	//Invalidate(pView);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->CanChangeFillColor())
		{
			pObj->m_colorFill = color;
			pObj->m_bColorFill = true;
			InvalObj(pView, pObj);
		}
	}
}

void CElementManager::LineColor(CModeler1View * pView)
{
	COLORREF color = ((CMainFrame*)AfxGetMainWnd())->GetColorFromColorButton(ID_FORMAT_LINECOLOR);

	if (color == (COLORREF) -1)
	{
		return;
	}

	//POSITION pos = m_selection.m_objects.GetHeadPosition();
	//while (pos != NULL)
	//{
	//	CElement * pElement = m_selection.m_objects.GetNext(pos);
	//	if (pElement->CanChangeLineColor())
	//	{
	//		pElement->m_colorLine = color;
	//		pElement->m_bColorLine = true;
	//	}
	//}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->CanChangeLineColor())
	{
		pElement->m_colorLine = color;
		pElement->m_bColorLine = true;
	}
	//Invalidate(pView);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->CanChangeLineColor())
		{
			pObj->m_colorLine = color;
			pObj->m_bColorLine = true;
			InvalObj(pView, pObj);
		}
	}
}

void CElementManager::LineWidth(CModeler1View * pView, UINT nID)
{
	int weight = -1;
	if (nID == ID_FORMAT_LINEWIDTH)
	{
		weight = ((CMainFrame*)AfxGetMainWnd())->GetWidthFromLineWidth(ID_FORMAT_LINEWIDTH);
	}
	else if (nID == ID_FORMAT_LINEWIDTH_MORE)
	{
		/*
		CLineWeightDlg dlg(AfxGetMainWnd());

		if (m_selection.GetCount() == 1)
		{
			dlg.m_penSize = m_selection.GetHead()->IsEnableLine() ? m_selection.GetHead()->GetLineWeight() : 0;
		}

		if (dlg.DoModal() == IDOK)
		{
			weight = dlg.m_penSize;
		}
		*/
	}

	if (weight == -1)
	{
		return;
	}

	//POSITION pos = m_selection.m_objects.GetHeadPosition();
	//while (pos != NULL)
	//{
	//	CElement * pElement = m_selection.m_objects.GetNext(pos);
	//	if (pElement->CanChangeLineWidth())
	//	{
	//		pElement->m_bLineWidth = weight > 0;
	//		pElement->m_lineWidth = weight;
	//	}
	//}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->CanChangeLineWidth())
	{
		pElement->m_bLineWidth = weight > 0;
		pElement->m_lineWidth = weight;
	}
	//Invalidate(pView);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->CanChangeLineWidth())
		{
			pObj->m_bLineWidth = weight > 0;
			pObj->m_lineWidth = weight;
			InvalObj(pView, pObj);
		}
	}
}

void CElementManager::PageColor(CModeler1View * pView)
{
	COLORREF color = ((CMainFrame*)AfxGetMainWnd())->GetColorFromColorButton(ID_FORMAT_PAGECOLOR);

	if (color == (COLORREF) -1)
	{
		return;
	}

	m_paperColor = color;
	Invalidate(pView);
}

void CElementManager::Zoom(CModeler1View * pView)
{
	//m_fZoomFactor += 0.10f;
	//Invalidate(pView);

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	CMFCRibbonBar* pRibbon = pFrame->GetRibbonBar();
	CMFCRibbonComboBox* pFindCombobox = DYNAMIC_DOWNCAST(CMFCRibbonComboBox, pRibbon->FindByID(ID_FORMAT_ZOOM));
		// this returns the last value before the combo box edit field got the focus:
	CString value = pFindCombobox->GetEditText();

	//AfxMessageBox(value);

	if (value == _T("25%"))
	{
		m_fZoomFactor = 0.25f;
	}
	if (value == _T("50%"))
	{
		m_fZoomFactor = 0.50f;
	}
	if (value == _T("75%"))
	{
		m_fZoomFactor = 0.75f;
	}
	if (value == _T("100%"))
	{
		m_fZoomFactor = 1.0f;
	}
	if (value == _T("150%"))
	{
		m_fZoomFactor = 1.5f;
	}
	if (value == _T("200%"))
	{
		m_fZoomFactor = 2.0f;
	}
	if (value == _T("400%"))
	{
		m_fZoomFactor = 4.0f;
	}

	Invalidate(pView);
}

void CElementManager::ZoomIn(CModeler1View * pView)
{
	m_fZoomFactor += 0.10f;
	Invalidate(pView);
}

void CElementManager::ZoomOut(CModeler1View * pView)
{
	m_fZoomFactor -= 0.10f;
	Invalidate(pView);
}

void CalcAutoPointRect2(int count, std::shared_ptr<CElement> pNewElement)
{
	int x = count % 20;
	int y = count % 20;

	pNewElement->m_point.x = 50 * x;
	pNewElement->m_point.y = 50 * y;

	pNewElement->m_rect.top = pNewElement->m_point.y;
	pNewElement->m_rect.left = pNewElement->m_point.x;
	pNewElement->m_rect.bottom = pNewElement->m_point.y + 100;
	pNewElement->m_rect.right = pNewElement->m_point.x + 100;
}

void CalcAutoPointRect(int count, std::shared_ptr<CElement> pNewElement)
{
	int c = 0;
	for (int y = 0; y < 30; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			if (count%400 == c)
			{
				pNewElement->m_point.x = 175 * x;
				pNewElement->m_point.y = 50 * y;

				pNewElement->m_rect.left = pNewElement->m_point.x;
				pNewElement->m_rect.top = pNewElement->m_point.y;
				pNewElement->m_rect.right = pNewElement->m_point.x + 100 + 50;
				pNewElement->m_rect.bottom = pNewElement->m_point.y + 30;
				return;
			}

			c++;
		}
	}
}

CString CElementManager::SearchDrive(const CString& strFile, const CString& strFilePath, const bool& bRecursive, const bool& bStopWhenFound)
{
	USES_CONVERSION;

	CWnd* pWnd = AfxGetMainWnd();
	CMainFrame* pMainFrame = (CMainFrame*)pWnd;

	CString strFoundFilePath;
	WIN32_FIND_DATA file;

	CString strPathToSearch = strFilePath;
	strPathToSearch += _T("\\");

	HANDLE hFile = FindFirstFile((strPathToSearch + "*"), &file);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			CString strTheNameOfTheFile = file.cFileName;

			// It could be a directory we are looking at
			// if so look into that dir
			if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				//if ((strTheNameOfTheFile != ".") && (strTheNameOfTheFile != ".."))
				{
					// ADD TO COLLECTION TYPE
					std::shared_ptr<CCodeFile> cf = std::make_shared<CCodeFile>();
					cf->_type = FileType::folder;
					cf->_name = T2W((LPTSTR)(LPCTSTR)(strTheNameOfTheFile));
					cf->_path = T2W((LPTSTR)(LPCTSTR)(strPathToSearch + strTheNameOfTheFile));
					_files.push_back(cf);
				}

				if ((strTheNameOfTheFile != ".") && (strTheNameOfTheFile != "..") && (bRecursive))
				{
					strFoundFilePath = SearchDrive(strFile, strPathToSearch + strTheNameOfTheFile, bRecursive, bStopWhenFound);

					if (!strFoundFilePath.IsEmpty() && bStopWhenFound)
						break;
				}		
			}
			else
			{
				//if (strTheNameOfTheFile == strFile)
				{
					strFoundFilePath = strPathToSearch + strTheNameOfTheFile; //strFile;

					// ADD TO COLLECTION TYPE
					std::shared_ptr<CCodeFile> cf = std::make_shared<CCodeFile>();
					cf->_type = FileType::file;
					cf->_name = T2W((LPTSTR)(LPCTSTR)strTheNameOfTheFile); //strFile;
					cf->_path = T2W((LPTSTR)(LPCTSTR)strFoundFilePath);
					_files.push_back(cf);

					if (bStopWhenFound)
						break;
				}
			}
		} while (FindNextFile(hFile, &file));

		FindClose(hFile);
	}

	return strFoundFilePath;
}

wstring GetFileContent(wstring filename)
{
	ifstream file(filename);
	string str;
	string file_contents;
	while (std::getline(file, str))
	{
		file_contents += str + string("\r\n");
	}
	wstring ws(file_contents.begin(), file_contents.end());
	return ws;
}

wstring GetFileContent(shared_ptr<CCodeFile> codeFile)
{
	return GetFileContent(codeFile->_path);
}


void CElementManager::LoadModule(CModeler1View * pView)
{
	CFolderPickerDialog dlg;
	if (dlg.DoModal() == IDCANCEL)
		return;

	CString strPath = dlg.GetFolderPath();
	_files.clear();
	SearchDrive(_T("*.*"), strPath, false, false);

	int count = 0;
	for (shared_ptr<CCodeFile> file : _files)
	{
		std::shared_ptr<CElement> pNewElement = CFactory::CreateElementOfType(ElementType::type_shapes_development, ShapeType::development_class);
		CalcAutoPointRect(count, pNewElement);
		pNewElement->m_pManager = this;
		pNewElement->m_pView = pView;
		pNewElement->m_text = file->_name;
		// Read file content
		pNewElement->m_code = GetFileContent(file);
		pNewElement->m_documentType = DocumentType::document_file;
		pNewElement->m_documentTypeText = _T("File");

		// Add an object
		m_objects.AddTail(pNewElement);
		pView->LogDebug(_T("object created ->") + pNewElement->ToString());

		++count;
	}

	Invalidate(pView);
}

void CElementManager::LoadFolders(CModeler1View* pView)
{
	CFolderPickerDialog dlg;
	if (dlg.DoModal() == IDCANCEL)
		return;

	CString strPath = dlg.GetFolderPath();
	_files.clear();
	SearchDrive(_T("*.*"), strPath, false, false);

	int count = 0;
	for (shared_ptr<CCodeFile> file : _files)
	{
		std::shared_ptr<CElement> pNewElement = nullptr;
		if (file->_type == FileType::file)
		{
			pNewElement = CFactory::CreateElementOfType(ElementType::type_shapes_development, ShapeType::development_class);
			pNewElement->m_documentType = DocumentType::document_file;
			pNewElement->m_documentTypeText = _T("File");
		}
		else
		{
			pNewElement = CFactory::CreateElementOfType(ElementType::type_shapes_development, ShapeType::development_interface);
			pNewElement->m_documentType = DocumentType::document_folder;
			pNewElement->m_documentTypeText = _T("Folder");
		}
		
		CalcAutoPointRect(count, pNewElement);
		pNewElement->m_pManager = this;
		pNewElement->m_pView = pView;
		pNewElement->m_text = file->_name;
		// Read file content
		//pNewElement->m_code = GetFileContent(file);
		pNewElement->m_document = file->_path;

		// Add an object
		m_objects.AddTail(pNewElement);
		pView->LogDebug(_T("object created ->") + pNewElement->ToString());

		++count;
	}

	Invalidate(pView);
}

void CElementManager::OpenFolder(CModeler1View* pView)
{
	shared_ptr<CElement> pElement = m_selection.GetHead();

	CString strPath = pElement->m_document.c_str();;
	_files.clear();
	SearchDrive(_T("*.*"), strPath, false, false);

	m_objects.RemoveAll();
	Invalidate(pView);

	int count = 0;
	for (shared_ptr<CCodeFile> file : _files)
	{
		std::shared_ptr<CElement> pNewElement = nullptr;
		if (file->_type == FileType::file)
		{
			pNewElement = CFactory::CreateElementOfType(ElementType::type_shapes_development, ShapeType::development_class);
			pNewElement->m_documentType = DocumentType::document_file;
			pNewElement->m_documentTypeText = _T("File");
		}
		else
		{
			pNewElement = CFactory::CreateElementOfType(ElementType::type_shapes_development, ShapeType::development_interface);
			pNewElement->m_documentType = DocumentType::document_folder;
			pNewElement->m_documentTypeText = _T("Folder");
		}

		CalcAutoPointRect(count, pNewElement);
		pNewElement->m_pManager = this;
		pNewElement->m_pView = pView;
		pNewElement->m_text = file->_name;
		// Read file content
		//pNewElement->m_code = GetFileContent(file);
		pNewElement->m_document = file->_path;

		// Add an object
		m_objects.AddTail(pNewElement);
		pView->LogDebug(_T("object created ->") + pNewElement->ToString());

		++count;
	}

	Invalidate(pView);
}

void CElementManager::OpenFile(CModeler1View* pView)
{

	shared_ptr<CElement> pElement = m_selection.GetHead();
	::ShellExecuteW(NULL, NULL, pElement->m_document.c_str(), NULL/*lpszArgs*/, NULL, SW_SHOW);
}

void CElementManager::OpenFileContent(CModeler1View* pView)
{

	shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_code = GetFileContent(pElement->m_document);
	
	CRuntimeClass* prt = RUNTIME_CLASS(CTabbedView); // CModeler1SourceView);
	CView* pview = NULL;
	// Continue search in inactive View by T(o)m
	CModeler1Doc* pDoc = pView->GetDocument();
	POSITION pos = pDoc->GetFirstViewPosition();
	while (pos != NULL)
	{
		pview = pDoc->GetNextView(pos);
		CRuntimeClass* pRT = pview->GetRuntimeClass();
		
		if( prt = pRT)
		{
			CTabbedView* pTView = (CTabbedView*)pview;
			pTView->SetActiveView(1);
			break;
		}
		pView = NULL;       // not valid vie
	}
}

void CElementManager::SetConnector(std::shared_ptr<CElement> pLineElement, std::shared_ptr<CElement> pElementFound, ConnectorType connector)
{
	/*
	pLineElement->m_connectorDragHandle2 = 2;

	shared_ptr<CElement> pElement = nullptr;
	pElement = pElementFound;

	// Connect to the right connector
	CPoint pointLine1 = pLineElement->m_rect.TopLeft();
	CPoint pointLine2 = pLineElement->m_rect.BottomRight();

	CPoint pointElement1 = pElement->m_rect.TopLeft();
	CPoint pointElement2 = pElement->m_rect.CenterPoint();
	CPoint pointElementCenter = pElement->m_rect.TopLeft();
	CRect re = pElement->m_rect;
	int handle = 2;
	*/

	/*
	if (pointLine2.x < re.TopLeft().x && pointLine2.y < re.TopLeft().y)
	{
		// haut centre
		handle = 2;
	}
	else if (pointLine2.x < re.CenterPoint().x && pointLine2.y < re.TopLeft().y)
	{
		// milieu gauche
		handle = 8;
	}
	else if (pointLine2.x < re.CenterPoint().x && pointLine2.y < re.BottomRight().y)
	{
		// bas centre
		handle = 6;
	}

	if (connector == ConnectorType::connector1)
	{
		pLineElement->m_connectorDragHandle1 = handle;
	}
	else
	{
		pLineElement->m_connectorDragHandle2 = handle;
	}
	*/

	/*
	CPoint point1 = pLineElement->m_rect.TopLeft();
	CPoint point2;
	if (connector == ConnectorType::connector1)
	{
		point2 = pLineElement->m_pConnector->m_pElement1->GetHandle(handle);
	}
	else
	{
		point2 = pLineElement->m_pConnector->m_pElement2->GetHandle(handle);
	}
	CRect rect(point1, point2);
	rect.NormalizeRect();
	pLineElement->m_rect = rect;
	*/

	/*
	shared_ptr<CElement> pElement = pLineElement;
	shared_ptr<CElement> pElement1 = pLineElement->m_pConnector->m_pElement1;
	shared_ptr<CElement> pElement2 = pLineElement->m_pConnector->m_pElement2;
	CPoint point1;

	if (pElement1 == nullptr)
	{
		point1 = pElement->m_rect.TopLeft();
	}
	else
	{
		//point1 = pElement1->m_rect.CenterPoint();
		int handle = pElement->m_connectorDragHandle1;
		if (handle == 0)
		{
			point1 = pElement1->m_rect.TopLeft();
		}
		else
		{
			point1 = pElement1->GetHandle(handle);
		}
	}
	*/
}

void CElementManager::FindAConnectionFor(std::shared_ptr<CElement> pLineElement, CPoint point, CModeler1View* pView, ConnectorType connector)
{
	// Find a connection ?
	if (pLineElement->IsLine() == true)
	{
		m_bSizingALine = true;

		SelectNone();
		Select(pLineElement);
		std::shared_ptr<CElement> pElement = m_objects.ObjectExceptLinesAt(point, pLineElement);
		if (pElement != NULL)
		{
			CRect rect = pElement->m_rect;
			/*CClientDC dc(pView);
			Graphics graphics(dc.m_hDC);
			
			Matrix matrix;
			CPoint pt = pElement->m_rect.CenterPoint();
			PointF pointf;
			pointf.X = pt.x;
			pointf.Y = pt.y;
			matrix.RotateAt(pElement->m_rotateAngle, pointf);
			graphics.SetTransform(&matrix);
			graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);

			SolidBrush solidBrush(Color::Yellow);
			graphics.FillRectangle(&solidBrush, rect.left, rect.top, rect.Width(), rect.Height());
			*/

			pElement->DrawTracker(pView);
			
			m_bDrawRectForConnectionPoint = true;
			m_DrawRectForConnectionPoint = rect;

			int nHandle = pElement->HitTest(point, pView, true);
			if (nHandle != 0)
			{
				CRect rectTracker = pElement->GetHandleRect(nHandle, pView);
				rectTracker.left -= 3;
				rectTracker.top -= 3;
				rectTracker.right += 3;
				rectTracker.bottom += 3;

				m_DractRectHandleTrackerForConnectionPoint = rectTracker;
				/*
				ViewToManager(pView, rectTracker);
				SolidBrush solidBrushTracker(Color::Green);
				graphics.FillRectangle(&solidBrushTracker, rectTracker.left, rectTracker.top, rectTracker.Width(), rectTracker.Height());
				Pen pen(Color::Violet);
				graphics.DrawEllipse(&pen, rectTracker.left, rectTracker.top, rectTracker.Width(), rectTracker.Height());
				*/
			}

			// Register the connector
			// if start, we take only the first connector in handle
			if (connector == ConnectorType::connector1) 
			{
				//pView->LogDebug(_T("FindAConnectionFor:: if (connector == ConnectorType::connector1)"));
				pLineElement->m_pConnector->m_pElement1 = pElement;
				//pLineElement->m_connectorDragHandle1 = 2;
				pLineElement->m_connectorDragHandle1 = nHandle;

				// Connect to the right connector
				SetConnector(pLineElement, pElement, ConnectorType::connector1);
			}
			else if (connector == ConnectorType::connector2)
			{
				//pView->LogDebug(_T("FindAConnectionFor:: if (connector == ConnectorType::connector2)"));
				pLineElement->m_pConnector->m_pElement2 = pElement;
				//pLineElement->m_connectorDragHandle2 = 2;
				pLineElement->m_connectorDragHandle2 = nHandle;

				// Connect to the right connector
				SetConnector(pLineElement, pElement, ConnectorType::connector2);
			}
		}
		else
		{
			// Register no connector
			if (connector == ConnectorType::connector1)
			{
				//pView->LogDebug(_T("FindAConnectionFor:: pLineElement->m_pConnector->m_pElement1 = nullptr;"));
				pLineElement->m_pConnector->m_pElement1 = nullptr;
			}
			else if (connector == ConnectorType::connector2)
			{
				//pView->LogDebug(_T("FindAConnectionFor:: pLineElement->m_pConnector->m_pElement2 = nullptr;"));
				pLineElement->m_pConnector->m_pElement2 = nullptr;
			}
		}

		//UpdatePropertyGrid(pView, pLineElement);
	}
}

void CElementManager::OnFileOpenGabarit(CModeler1View* pView)
{
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() != IDOK)
		return;

	CStringW fileName = dlg.GetPathName();

	CDocument* pDocument = ((CModeler1App*)AfxGetApp())->m_pDocTemplate->CreateNewDocument();
	pDocument->m_bAutoDelete = FALSE;   // don't destroy if something goes wrong
	pDocument->OnNewDocument();
	// open an existing document
	CWaitCursor wait;
	pDocument->OnOpenDocument((LPTSTR)(LPCTSTR)fileName);
	CModeler1Doc* pDoc = (CModeler1Doc*)pDocument;

	vector<shared_ptr<CElement>> newVector;
	for (shared_ptr<CElement> pElement : pDoc->GetManager()->GetObjects())
	{
		std::shared_ptr<CElement> pNewElement = CFactory::CreateElementOfType(pElement->m_type, pElement->m_shapeType);
		pNewElement->m_name = pElement->m_name;
		pNewElement->m_text = pElement->m_text;
		pNewElement->BuildVChar();
		pNewElement->m_code = pElement->m_code;
		pNewElement->m_image = pElement->m_image;
		pNewElement->m_objectId = pElement->m_objectId;
		pNewElement->m_rect = pElement->m_rect;
		pNewElement->m_bColorFill = pElement->m_bColorFill;
		pNewElement->m_bColorLine = pElement->m_bColorLine;
		pNewElement->m_bColorFill = pElement->m_bColorFill;
		pNewElement->m_bLineWidth = pElement->m_bLineWidth;
		pNewElement->m_bSolidColorFill = pElement->m_bSolidColorFill;
		//pNewElement->m_caption = pElement->m_caption;
		pNewElement->m_colorFill = pElement->m_colorFill;
		pNewElement->m_colorLine = pElement->m_colorLine;
		pNewElement->m_image = pElement->m_image;
		pNewElement->m_last = pElement->m_last;
		pNewElement->m_lineWidth = pElement->m_lineWidth;
		pNewElement->m_textAlign = pElement->m_textAlign;
		pNewElement->m_fontName = pElement->m_fontName;
		pNewElement->m_pManager = this;
		pNewElement->m_point = pElement->m_point;
		pNewElement->m_pView = pView;

		newVector.push_back(pNewElement);
	}

	//pView->GetDocument()->GetManager()->m_objects.m_objects.insert(pView->GetDocument()->GetManager()->m_objects.m_objects.end(), newVector.begin(), newVector.end());
	// eq. MoveBack
	for (shared_ptr<CElement> pElement : newVector)
	{
		pView->GetDocument()->GetManager()->m_objects.m_objects.insert(pView->GetDocument()->GetManager()->m_objects.m_objects.begin(), pElement);
	}

	Invalidate(pView);
}

void CElementManager::AlignLeft(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;
	
			int width = pObj->m_rect.Width();
			pObj->m_rect.left = pElementBase->m_rect.left;
			pObj->m_rect.right = pObj->m_rect.left + width;
			pObj->m_point = pObj->m_rect.TopLeft();
			InvalObj(pView, pObj);
		}

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::AlignRight(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			int width = pObj->m_rect.Width();
			pObj->m_rect.right = pElementBase->m_rect.right;
			pObj->m_rect.left = pObj->m_rect.right - width;
			pObj->m_point = pObj->m_rect.TopLeft();
			InvalObj(pView, pObj);
		}

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::AlignTop(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			int height = pObj->m_rect.Height();
			pObj->m_rect.top = pElementBase->m_rect.top;
			pObj->m_rect.bottom = pObj->m_rect.top + height;
			pObj->m_point = pObj->m_rect.TopLeft();
			InvalObj(pView, pObj);
		}

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::AlignBottom(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			int height = pObj->m_rect.Height();
			pObj->m_rect.bottom = pElementBase->m_rect.bottom;
			pObj->m_rect.top = pObj->m_rect.bottom - height;
			pObj->m_point = pObj->m_rect.TopLeft();
			InvalObj(pView, pObj);
		}

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::AlignTextLeft(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			pObj->m_textAlign = _T("Left");
			InvalObj(pView, pObj);
		}

		UpdatePropertyGrid(pView, pElementBase);

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::AlignTextCenter(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			pObj->m_textAlign = _T("Center");
			InvalObj(pView, pObj);
		}

		UpdatePropertyGrid(pView, pElementBase);

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::AlignTextRight(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			pObj->m_textAlign = _T("Right");
			InvalObj(pView, pObj);
		}

		UpdatePropertyGrid(pView, pElementBase);

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::OnEditGroup(CModeler1View* pView)
{
	static int count = 0;
	//AfxMessageBox(L"Grouping");

	++count;

	wstringstream ss;
	ss << _T("Group_") << count;

	shared_ptr<CElementGroup> speg = make_shared<CElementGroup>();
	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pElement = *itSel;
		pElement->m_pElementGroup = speg;
		pElement->m_bGrouping = true;
		speg->m_name = ss.str();
		speg->m_Groups.push_back(pElement);
	}
	this->m_groups.push_back(speg);
}

void CElementManager::OnEditUngroup(CModeler1View* pView)
{
	//AfxMessageBox(L"Ungrouping");
	shared_ptr<CElementGroup> speg = nullptr;

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pElement = *itSel;
		speg = pElement->m_pElementGroup;
		break;
	}

	for (vector<std::shared_ptr<CElementGroup>>::iterator it = m_groups.begin(); it != m_groups.end(); it++)
	{
		shared_ptr<CElementGroup> pElementGroup = *it;

		if (pElementGroup == speg)
		{
			for (vector<std::shared_ptr<CElement>>::const_iterator itSel = pElementGroup->m_Groups.begin(); itSel != pElementGroup->m_Groups.end(); itSel++)
			{
				std::shared_ptr<CElement> pObj = *itSel;
				pObj->m_pElementGroup = nullptr;
				pObj->m_bGrouping = false;

			}
			m_groups.erase(it);
			break;
		}
	}
}

std::vector<std::wstring> CElementManager::Split(const std::wstring& s, wchar_t delim)
{
	std::wstringstream ss(s);
	std::wstring item;
	std::vector<std::wstring> elems;
	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
		// elems.push_back(std::move(item)); // if C++11 (based on comment from @mchiasson)
	}
	return elems;
}

void CElementManager::BuildGroups()
{
	wstring n = CElement::m_elementGroupNames;
	wstring elts = CElement::m_elementGroupElements;
	vector<wstring> vnames = CElementManager::Split(n, _T('|'));
	vector<wstring> vlistes = CElementManager::Split(elts, _T('|'));

	//for (int i = 0; i < vnames.size(); ++i)
	//{
	//	wstring aName = vnames[i];
	//	if (aName.size() == 0)
	//	{
	//		continue;
	//	}
	//}

	this->m_groups.clear();
		
	for (int j = 0; j < vlistes.size(); ++j)
	{

		wstring aName = vnames[j];
		wstring aList = vlistes[j];
		if (aList.size() <= 2)
		{
			continue;
		}

		shared_ptr<CElementGroup> speg = make_shared<CElementGroup>();

		vector<wstring> velements = CElementManager::Split(aList, _T(';'));
		for (int x = 0; x < velements.size(); ++x)
		{
			wstring element = velements[x];
			if (element.size() <= 2)
			{
				continue;
			}

			std::shared_ptr<CElement> pElement = this->m_objects.FindElementByName(element);
			if (pElement != nullptr)
			{
				pElement->m_pElementGroup = speg;
				pElement->m_bGrouping = true;
				speg->m_Groups.push_back(pElement);
			}
		}

		this->m_groups.push_back(speg);
	}

}

void CElementManager::ExpandGroupAttributes()
{
	CString n;
	CString elts;
	for (shared_ptr<CElementGroup> pElementGroup : m_groups)
	{
		//elts += CString(_T("|"));
		n += /*CString(_T("|")) +*/ CString(pElementGroup->m_name.c_str()) + CString(_T("|"));
		for (shared_ptr<CElement> pElement : pElementGroup->m_Groups)
		{
			elts += CString(pElement->m_name.c_str()) + CString(_T(";"));
		}
		elts += CString(_T("|"));
	}
	CElement::m_elementGroupNames = n;
	CElement::m_elementGroupElements = elts;
}

int CElementManager::GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		//AfxMessageBox(pImageCodecInfo[j].MimeType);
		//continue;

		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

void CElementManager::OnFileExportPNG(CModeler1View* pView)
{
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() != IDOK)
		return;
	CStringW fileName = dlg.GetPathName();
	wstring doc = (LPTSTR)(LPCTSTR)fileName;

	Bitmap myBitmap(m_size.cx, m_size.cy, PixelFormat32bppARGB);
	Graphics graphics(&myBitmap);

	// just like that
	//graphics.ScaleTransform(0.75f, 0.75f);
	//graphics.RotateTransform(10.0f, MatrixOrder::MatrixOrderAppend);
	graphics.ScaleTransform(m_fZoomFactor, m_fZoomFactor);

	// TODO: add draw code for native data here
	for (vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin(); i != GetObjects().end(); i++)
	{
		std::shared_ptr<CElement> pElement = *i;
		// FIXME: Update the view for Property Window
		pElement->m_pView = pView;

		// Construct the graphic context for each element
		CDrawingContext ctxt(pElement.get());
		ctxt.m_pGraphics = &graphics;

		graphics.ResetTransform();
		Matrix matrix;
		CPoint pt = pElement->m_rect.CenterPoint();
		PointF point;
		point.X = pt.x;
		point.Y = pt.y;
		matrix.RotateAt(pElement->m_rotateAngle, point);
		graphics.SetTransform(&matrix);

		//pElement->Draw(pView, pDC);
		pElement->Draw(ctxt);

		// HACK 14072012 : no more SimpleTextElement / Just other text elements except for caption old element
		// caption property is deprecated. be carefull. DO NOT USE m_caption any more !!!
		// informations: 
		//    for m_type that are simple shapes  ; it means it is not about text shapes,
		//    the m_text property can be optional and if it exists,
		//    it should be appended to the rendering area by creating a dedicated object. 
		//    We call it CSimpleTextElement.
		if (pElement->m_text.empty() == false &&
			(pElement->m_type != ElementType::type_text)
			)
		{
			//std::shared_ptr<CElement> pTextElement = make_shared<CSimpleTextElement>();
			std::shared_ptr<CElement> pTextElement(new CSimpleTextElement());
			pTextElement->m_rect = pElement->m_rect;
			pTextElement->m_text = pElement->m_text;
			pTextElement->m_textAlign = pElement->m_textAlign;
			pTextElement->m_fontName = pElement->m_fontName;
			pTextElement->m_fontSize = pElement->m_fontSize;
			pTextElement->m_leftMargin = pElement->m_leftMargin;
			pTextElement->m_topMargin = pElement->m_topMargin;
			pTextElement->m_rotateAngle = pElement->m_rotateAngle;
			pTextElement->Draw(ctxt);
		}

	}

	// Save bitmap (as a png)
	CLSID pngClsid;
	int result = GetEncoderClsid(L"image/png", &pngClsid);
	myBitmap.Save(doc.c_str(), &pngClsid, NULL);
}

void CElementManager::OnSelectAll(CModeler1View* pView)
{
	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_objects.m_objects.begin(); itSel != m_objects.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pElement = *itSel;
		Select(pElement);
	}

	Invalidate(pView);
}

void CElementManager::OnFontBold(CModeler1View* pView)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->m_bBold == true)
	{
		pElement->m_bBold = false;
	}
	else
	{
		pElement->m_bBold = true;
	}
	//InvalObj(pView, pElement);
	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->m_bBold == true)
		{
			pObj->m_bBold = false;
		}
		else
		{
			pObj->m_bBold = true;
		}
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontItalic(CModeler1View* pView)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->m_bItalic == true)
	{
		pElement->m_bItalic = false;
	}
	else
	{
		pElement->m_bItalic = true;
	}
	//InvalObj(pView, pElement);
	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->m_bItalic == true)
		{
			pObj->m_bItalic = false;
		}
		else
		{
			pObj->m_bItalic = true;
		}
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontUnderline(CModeler1View* pView)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->m_bUnderline == true)
	{
		pElement->m_bUnderline = false;
	}
	else
	{
		pElement->m_bUnderline = true;
	}
	// Redraw the element
	//InvalObj(pView, pElement);
	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->m_bUnderline == true)
		{
			pObj->m_bUnderline = false;
		}
		else
		{
			pObj->m_bUnderline = true;
		}
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontStrikeThrough(CModeler1View* pView)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	if (pElement->m_bStrikeThrough == true)
	{
		pElement->m_bStrikeThrough = false;
	}
	else
	{
		pElement->m_bStrikeThrough = true;
	}
	// Redraw the element
	//InvalObj(pView, pElement);
	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		if (pObj->m_bStrikeThrough == true)
		{
			pObj->m_bStrikeThrough = false;
		}
		else
		{
			pObj->m_bStrikeThrough = true;
		}
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontGrowFont(CModeler1View* pView)
{
	CMFCRibbonBar* pRibbon = ((CMainFrame*)pView->GetTopLevelFrame())->GetRibbonBar();

	CMFCRibbonComboBox* pFontCombo = DYNAMIC_DOWNCAST(CMFCRibbonComboBox, pRibbon->FindByID(ID_FONT_FONTSIZE));
	if (pFontCombo == NULL)
	{
		return;
	}

	CString fontSize = pFontCombo->GetEditText();
	int iFontSize = _ttoi(fontSize);

	iFontSize += 2;

	if (iFontSize > 60)
	{
		return;
	}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_fontSize = iFontSize;

	TCHAR sz[255];
	_stprintf_s(sz, _T("%d"), pElement->m_fontSize);
	pFontCombo->SelectItem(sz);

	// Redraw the element
	//InvalObj(pView, pElement);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		pObj->m_fontSize = iFontSize;
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontShrink(CModeler1View* pView)
{
	CMFCRibbonBar* pRibbon = ((CMainFrame*)pView->GetTopLevelFrame())->GetRibbonBar();

	CMFCRibbonComboBox* pFontCombo = DYNAMIC_DOWNCAST(CMFCRibbonComboBox, pRibbon->FindByID(ID_FONT_FONTSIZE));
	if (pFontCombo == NULL)
	{
		return;
	}

	CString fontSize = pFontCombo->GetEditText();
	int iFontSize = _ttoi(fontSize);

	iFontSize -= 2;

	if (iFontSize < 8)
	{
		return;
	}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_fontSize = iFontSize;

	TCHAR sz[255];
	_stprintf_s(sz, _T("%d"), pElement->m_fontSize);
	pFontCombo->SelectItem(sz);

	// Redraw the element
	//InvalObj(pView, pElement);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		pObj->m_fontSize = iFontSize;
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontClearFormat(CModeler1View* pView)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_fontName = _T("Calibri");
	pElement->m_fontSize = 12;
	pElement->m_bBold = false;
	pElement->m_bItalic = false;
	pElement->m_bUnderline = false;
	pElement->m_bStrikeThrough = false;
	pElement->m_colorText = RGB(0, 0, 0);
	pElement->m_colorFill = RGB(255, 255, 255);
	pElement->m_bColorFill = true;
	pElement->m_bColorLine = false;
	pElement->m_bSolidColorFill = true;

	UpdatePropertyGrid(pView, pElement);
	UpdateRibbonUI(pView, pElement);

	// Redraw the element
	//InvalObj(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		pObj->m_fontName = _T("Calibri");
		pObj->m_fontSize = 12;
		pObj->m_bBold = false;
		pObj->m_bItalic = false;
		pObj->m_bUnderline = false;
		pObj->m_bStrikeThrough = false;
		pObj->m_colorText = RGB(0, 0, 0);
		pObj->m_colorFill = RGB(255, 255, 255);
		pObj->m_bColorFill = true;
		pObj->m_bColorLine = false;
		pObj->m_bSolidColorFill = true;
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontColor(CModeler1View* pView)
{
	CMFCRibbonBar* pRibbon = ((CMainFrame*)pView->GetTopLevelFrame())->GetRibbonBar();

	COLORREF color = ((CMainFrame*)AfxGetMainWnd())->GetColorFromColorButton(ID_FONT_COLOR);

	if (color == (COLORREF)-1)
	{
		return;
	}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_colorText = color;

	// Redraw the element
	//InvalObj(pView, pElement);
	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		pObj->m_colorText = color;
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontTextHighlight(CModeler1View* pView)
{
	CMFCRibbonBar* pRibbon = ((CMainFrame*)pView->GetTopLevelFrame())->GetRibbonBar();

	COLORREF color = ((CMainFrame*)AfxGetMainWnd())->GetColorFromColorButton(ID_FONT_TEXTHIGHLIGHT);

	if (color == (COLORREF)-1)
	{
		return;
	}

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_colorFill = color;
	pElement->m_bColorFill = true;
	pElement->m_bColorLine = false;
	pElement->m_bSolidColorFill = true;

	// Redraw the element
	//InvalObj(pView, pElement);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		pObj->m_colorFill = color;
		pObj->m_bColorFill = true;
		pObj->m_bColorLine = false;
		pObj->m_bSolidColorFill = true;
		InvalObj(pView, pObj);
	}
}

void CElementManager::OnFontChangeCase(CModeler1View* pView)
{
	static int count = 0;

	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	wstring text = pElement->m_text;

	++count;

	if (count % 2 == 0)
	{
		transform(text.begin(), text.end(), text.begin(), tolower);
	}
	else
	{
		transform(text.begin(), text.end(), text.begin(), toupper);
	}

	pElement->m_text = text;
	pElement->BuildVChar();

	// Redraw the element
	//InvalObj(pView, pElement);

	UpdatePropertyGrid(pView, pElement);

	for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
	{
		std::shared_ptr<CElement> pObj = *itSel;

		pObj->m_text = text;
		pObj->BuildVChar();
	}
}

void CElementManager::OnActionElements(CModeler1View* pView)
{
	CWnd* p = AfxGetMainWnd();
	CMainFrame* pmf = (CMainFrame*)p;
	pmf->OnActionElements(pView);
}

void CElementManager::BuildElementsCombo(CModeler1View* pView)
{
	CWnd* p = AfxGetMainWnd();
	CMainFrame* pmf = (CMainFrame*)p;
	pmf->BuildElementsCombo(pView);
}

void CElementManager::OnDesignDeconnect(CModeler1View* pView)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();
	pElement->m_pConnector->m_pElement1 = nullptr;
	pElement->m_pConnector->m_pElement2 = nullptr;
}

void CElementManager::OnFileImportJSON(CModeler1View* pView)
{
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() != IDOK)
		return;
	CStringW fileName = dlg.GetPathName();
	wstring doc = (LPTSTR)(LPCTSTR)fileName;

	wstring json = GetFileContent(doc);

	m_objects.RemoveAll();
	Invalidate(pView);

	try
	{
		web::json::value jdata = web::json::value::parse(json);
		FromJSON(jdata.as_object());
	}
	catch (web::json::json_exception ex)
	{
		string str = ex.what();
		wstring w(str.begin(), str.end());
		AfxMessageBox(w.c_str());
		return;
	}

	for (shared_ptr<CElement> pElement : GetObjects())
	{
		if (pElement->IsLine())
		{
			pElement->m_pConnector->m_pElement1 = m_objects.FindElementByName(pElement->m_connectorName1);
			pElement->m_pConnector->m_pElement2 = m_objects.FindElementByName(pElement->m_connectorName2);
		}

		pElement->m_pManager = this; // TODO

		POSITION pos = /*pNewElement->m_pView =*/ pView->GetDocument()->GetFirstViewPosition(); //nullptr; // TODO
		pElement->m_pView = (CModeler1View*)(pView->GetDocument()->GetNextView(pos)); //()->GetRoutingView();
	}

	// Build groups
	BuildGroups();

	BuildElementsCombo(pView);
}

void CElementManager::OnFileExportJSON(CModeler1View* pView)
{
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() != IDOK)
		return;
	CStringW fileName = dlg.GetPathName();
	wstring doc = (LPTSTR)(LPCTSTR)fileName;

	web::json::value jdata = AsJSON();
	wstring json = jdata.serialize();

	string str(json.begin(), json.end());

	ofstream file(doc);
	file << str;
	file.close();

	//AfxMessageBox(json.c_str());
}

void CElementManager::Serialize_SaveAsXML(CModeler1View* pView)
{
	USES_CONVERSION;

	CFileDialog dlg(FALSE);
	if (dlg.DoModal() == IDCANCEL)
		return;

	CString strFileName = dlg.GetFileName();
	CString strPath = dlg.GetFolderPath();
	CString strFile;
	strFile.Format(_T("%s\\%s"), strPath, strFileName);
	std::wstring filename = T2W((LPTSTR)(LPCTSTR)strFile);

	boost::shared_ptr<CShapeCollection> data(new CShapeCollection());

	for (vector<std::shared_ptr<CElement>>::iterator i = m_objects.m_objects.begin(); i != m_objects.m_objects.end(); i++)
	{
		std::shared_ptr<CElement> pElement = *i;
		boost::shared_ptr<CSimpleShape> pNewElement(new CSimpleShape());
		pNewElement->m_name = pElement->m_name;
		pNewElement->m_id = pElement->m_objectId;
		pNewElement->m_type = pElement->m_type;
		pNewElement->m_shapeType = pElement->m_shapeType;
		pNewElement->m_caption = pElement->m_caption;
		pNewElement->m_text = pElement->m_text;

		CPoint p1 = pElement->m_rect.TopLeft();
		CPoint p2 = pElement->m_rect.BottomRight();
		pNewElement->m_x1 = p1.x;
		pNewElement->m_y1 = p1.y;
		pNewElement->m_x2 = p2.x;
		pNewElement->m_y2 = p2.y;

		pNewElement->m_colorFillR = GetRValue(pElement->m_colorFill);
		pNewElement->m_colorFillG = GetGValue(pElement->m_colorFill);
		pNewElement->m_colorFillB = GetBValue(pElement->m_colorFill);
		pNewElement->m_colorLineR = GetRValue(pElement->m_colorLine);
		pNewElement->m_colorLineG = GetGValue(pElement->m_colorLine);
		pNewElement->m_colorLineB = GetBValue(pElement->m_colorLine);

		pNewElement->m_bSolidColorFill = pElement->m_bSolidColorFill;
		pNewElement->m_bColorLine = pElement->m_bColorLine;
		pNewElement->m_bColorFill = pElement->m_bColorFill;
		pNewElement->m_lineWidth = pElement->m_lineWidth;

		pNewElement->m_image = pElement->m_image;
		pNewElement->m_textAlign = pElement->m_textAlign;

		pNewElement->m_bFixed = pElement->m_bFixed;
		pNewElement->m_fontSize = pElement->m_fontSize;
		pNewElement->m_fontName = pElement->m_fontName;
		pNewElement->m_code = pElement->m_code;
		pNewElement->m_bBold = pElement->m_bBold;
		pNewElement->m_bItalic = pElement->m_bItalic;
		pNewElement->m_bUnderline = pElement->m_bUnderline;
		pNewElement->m_bStrikeThrough = pElement->m_bStrikeThrough;
		pNewElement->m_colorTextR = GetRValue(pElement->m_colorText);
		pNewElement->m_colorTextG = GetGValue(pElement->m_colorText);
		pNewElement->m_colorTextB = GetBValue(pElement->m_colorText);
		pNewElement->m_connectorName1 = pElement->m_connectorName1;
		pNewElement->m_connectorName2 = pElement->m_connectorName2;
		pNewElement->m_connectorDragHandle1 = pElement->m_connectorDragHandle1;
		pNewElement->m_connectorDragHandle2 = pElement->m_connectorDragHandle2;
		pNewElement->m_document = pElement->m_document;
		pNewElement->m_elementGroupNames = pElement->m_elementGroupNames;
		pNewElement->m_elementGroupElements = pElement->m_elementGroupElements;
		pNewElement->m_documentType = pElement->m_documentType;
		pNewElement->m_documentTypeText = pElement->m_documentTypeText;
		pNewElement->m_version = pElement->m_version;
		pNewElement->m_product = pElement->m_product;
		pNewElement->m_leftMargin = pElement->m_leftMargin;
		pNewElement->m_topMargin = pElement->m_topMargin;
		pNewElement->m_rotateAngle = pElement->m_rotateAngle;
		pNewElement->m_team = pElement->m_team;
		pNewElement->m_authors = pElement->m_authors;
		pNewElement->m_bShowElementName = pElement->m_bShowElementName;
		pNewElement->m_standardShapesTextColorR = GetRValue(pElement->m_standardShapesTextColor);
		pNewElement->m_standardShapesTextColorG = GetGValue(pElement->m_standardShapesTextColor);
		pNewElement->m_standardShapesTextColorB = GetBValue(pElement->m_standardShapesTextColor);
		pNewElement->m_connectorShapesTextColorR = GetRValue(pElement->m_connectorShapesTextColor);
		pNewElement->m_connectorShapesTextColorG = GetGValue(pElement->m_connectorShapesTextColor);
		pNewElement->m_connectorShapesTextColorB = GetBValue(pElement->m_connectorShapesTextColor);;
		pNewElement->m_bShowConnectors = pElement->m_bShowConnectors;
		pNewElement->m_textConnector1 = pElement->m_textConnector1;
		pNewElement->m_textConnector2 = pElement->m_textConnector2;
		pNewElement->m_dashLineType = pElement->m_dashLineType;
		pNewElement->m_arrowType = pElement->m_arrowType;


		data->m_shapes.push_back(pNewElement);
	}

	/*
	for( vector<std::shared_ptr<CElement>>::const_iterator i = GetObjects().begin() ; GetObjects.end() ; i++ )
	{
		const std::shared_ptr<CElement> pElement = *i;
		std::shared_ptr<CElement> pElement
	}
	*/

	//filename = _T("C:\\christophep\\temp\\mymodeler1.xml");
	// load an archive
	std::ofstream xofs(filename.c_str());
	boost::archive::xml_oarchive xoa(xofs);
	xoa << BOOST_SERIALIZATION_NVP(data);

}

void CElementManager::Serialize_LoadAsXML(CModeler1View* pView)
{
	USES_CONVERSION;

	boost::shared_ptr<CShapeCollection> data(new CShapeCollection());

	CFileDialog dlg(TRUE);
	if (dlg.DoModal() == IDCANCEL)
		return;

	CString strFileName = dlg.GetFileName();
	CString strPath = dlg.GetFolderPath();
	CString strFile;
	strFile.Format(_T("%s\\%s"), strPath, strFileName);
	std::wstring filename = T2W((LPTSTR)(LPCTSTR)strFile);

	// load an archive
	std::ifstream xifs(filename.c_str());
	assert(xifs.good());
	boost::archive::xml_iarchive xia(xifs);
	//AfxMessageBox("xia >> BOOST_SERIALIZATION_NVP(data);...");
	xia >> BOOST_SERIALIZATION_NVP(data);

	int count = data->m_shapes.size();
	CString str;
	str.Format(_T("data read size=%d"), count);
	//AfxMessageBox(str);

	// Clear existing shapes
	m_objects.RemoveAll();

	for (vector<boost::shared_ptr<CSimpleShape> >::iterator i = data->m_shapes.begin(); i != data->m_shapes.end(); i++)
	{
		boost::shared_ptr<CSimpleShape> pElement = *i;
		//AfxMessageBox(pElement->m_name + " " + pElement->m_id);

		std::shared_ptr<CElement> pNewElement = CFactory::CreateElementOfType((ElementType)pElement->m_type,
			(ShapeType)pElement->m_shapeType);
		pNewElement->m_name = pElement->m_name;
		pNewElement->m_objectId = pElement->m_id;
		pNewElement->m_caption = pElement->m_caption;
		pNewElement->m_text = pElement->m_text;
		pNewElement->m_pManager = this;
		pNewElement->m_pView = pView;

		CPoint p1;
		CPoint p2;
		p1.x = pElement->m_x1;
		p1.y = pElement->m_y1;
		p2.x = pElement->m_x2;
		p2.y = pElement->m_y2;
		pNewElement->m_rect = CRect(p1, p2);

		int colorFillR = pElement->m_colorFillR;
		int colorFillG = pElement->m_colorFillG;
		int colorFillB = pElement->m_colorFillB;
		pNewElement->m_colorFill = RGB(colorFillR, colorFillG, colorFillB);
		int colorLineR = pElement->m_colorLineR;
		int colorLineG = pElement->m_colorLineG;
		int colorLineB = pElement->m_colorLineB;
		pNewElement->m_colorLine = RGB(colorLineR, colorLineG, colorLineB);

		pNewElement->m_bSolidColorFill = pElement->m_bSolidColorFill;
		pNewElement->m_bColorLine = pElement->m_bColorLine;
		pNewElement->m_bColorFill = pElement->m_bColorFill;

		pNewElement->m_image = pElement->m_image;
		pNewElement->m_textAlign = pElement->m_textAlign;

		pNewElement->m_bFixed = pElement->m_bFixed;
		pNewElement->m_fontSize = pElement->m_fontSize;
		pNewElement->m_fontName = pElement->m_fontName;
		pNewElement->m_code = pElement->m_code;
		pNewElement->m_bBold = pElement->m_bBold;
		pNewElement->m_bItalic = pElement->m_bItalic;
		pNewElement->m_bUnderline = pElement->m_bUnderline;
		pNewElement->m_bStrikeThrough = pElement->m_bStrikeThrough;
		int colorTextR = GetRValue(pElement->m_colorTextR);
		int colorTextG = GetGValue(pElement->m_colorTextG);
		int colorTextB = GetBValue(pElement->m_colorTextB);
		pNewElement->m_colorText = RGB(colorTextR, colorTextG, colorTextB);
		pNewElement->m_connectorName1 = pElement->m_connectorName1;
		pNewElement->m_connectorName2 = pElement->m_connectorName2;
		pNewElement->m_connectorDragHandle1 = pElement->m_connectorDragHandle1;
		pNewElement->m_connectorDragHandle2 = pElement->m_connectorDragHandle2;
		pNewElement->m_document = pElement->m_document;
		pNewElement->m_elementGroupNames = pElement->m_elementGroupNames;
		pNewElement->m_elementGroupElements = pElement->m_elementGroupElements;
		pNewElement->m_documentType = (DocumentType)pElement->m_documentType;
		pNewElement->m_documentTypeText = pElement->m_documentTypeText;
		pNewElement->m_version = pElement->m_version;
		pNewElement->m_product = pElement->m_product;
		pNewElement->m_leftMargin = pElement->m_leftMargin;
		pNewElement->m_topMargin = pElement->m_topMargin;
		pNewElement->m_rotateAngle = pElement->m_rotateAngle;
		pNewElement->m_team = pElement->m_team;
		pNewElement->m_authors = pElement->m_authors;
		pNewElement->m_bShowElementName = pElement->m_bShowElementName;
		int standardShapesTextColorR = GetRValue(pElement->m_standardShapesTextColorR);
		int standardShapesTextColorG = GetGValue(pElement->m_standardShapesTextColorG);
		int standardShapesTextColorB = GetBValue(pElement->m_standardShapesTextColorB);
		pNewElement->m_standardShapesTextColor = RGB(standardShapesTextColorR, standardShapesTextColorG, standardShapesTextColorB);
		int connectorShapesTextColorR = GetRValue(pElement->m_connectorShapesTextColorR);
		int connectorShapesTextColorG = GetGValue(pElement->m_connectorShapesTextColorG);
		int connectorShapesTextColorB = GetBValue(pElement->m_connectorShapesTextColorB);
		pNewElement->m_connectorShapesTextColor = RGB(connectorShapesTextColorR, connectorShapesTextColorG, connectorShapesTextColorB);
		pNewElement->m_bShowConnectors = pElement->m_bShowConnectors;
		pNewElement->m_textConnector1 = pElement->m_textConnector1;
		pNewElement->m_textConnector2 = pElement->m_textConnector2;
		pNewElement->m_dashLineType = (DashLineType)pElement->m_dashLineType;
		pNewElement->m_arrowType = (ArrowType)pElement->m_arrowType;

		m_objects.AddTail(pNewElement);
		pView->LogDebug(_T("object created ->") + pNewElement->ToString());
	}

	for (shared_ptr<CElement> pElement : GetObjects())
	{
		if (pElement->IsLine())
		{
			pElement->m_pConnector->m_pElement1 = m_objects.FindElementByName(pElement->m_connectorName1);
			pElement->m_pConnector->m_pElement2 = m_objects.FindElementByName(pElement->m_connectorName2);
		}

		pElement->m_pManager = this; // TODO

		POSITION pos = /*pNewElement->m_pView =*/ pView->GetDocument()->GetFirstViewPosition(); //nullptr; // TODO
		pElement->m_pView = (CModeler1View*)(pView->GetDocument()->GetNextView(pos)); //()->GetRoutingView();
	}

	// Build groups
	BuildGroups();

	BuildElementsCombo(pView);

	// Redraw the view
	Invalidate(pView);

}

void CElementManager::OnElementsScalePlus(CModeler1View* pView)
{
	// For each elements, x2 the font size
	for (shared_ptr<CElement> pElement : m_selection.m_objects)
	{
		// m_ font size
		int fontSize = pElement->m_fontSize;
		fontSize = fontSize + 4;
		if (fontSize > 20 && (fontSize % 2 != 0))
		{
			fontSize++;
		}
		pElement->m_fontSize = fontSize;

		// m_rect
		int x1 = pElement->m_rect.left;
		int y1 = pElement->m_rect.top;
		int x2 = pElement->m_rect.right;
		int y2 = pElement->m_rect.bottom;
		x2 = x1 + (pElement->m_rect.Width() * 1.25);
		y2 = y1 + (pElement->m_rect.Height() * 1.25);
		CRect rect(CPoint(x1, y1), CPoint(x2, y2));
		pElement->m_rect = rect;
	}

	shared_ptr<CElement> pElement = m_selection.GetHead();
	UpdateUI(pView, pElement);
	pView->Invalidate();
}

void CElementManager::OnElementsScaleMoins(CModeler1View* pView)
{
	// For each elements, x2 the font size
	for (shared_ptr<CElement> pElement : m_selection.m_objects)
	{
		// m_ font size
		int fontSize = pElement->m_fontSize;
		fontSize = fontSize - 4;
		if (fontSize > 20 && (fontSize % 2 != 0))
		{
			fontSize++;
		}
		pElement->m_fontSize = fontSize;

		// m_rect
		int x1 = pElement->m_rect.left;
		int y1 = pElement->m_rect.top;
		int x2 = pElement->m_rect.right;
		int y2 = pElement->m_rect.bottom;
		x2 = x1 + (pElement->m_rect.Width() / 1.25);
		y2 = y1 + (pElement->m_rect.Height() / 1.25);
		CRect rect(CPoint(x1, y1), CPoint(x2, y2));
		pElement->m_rect = rect;
	}

	shared_ptr<CElement> pElement = m_selection.GetHead();
	UpdateUI(pView, pElement);
	pView->Invalidate();
}

bool CElementManager::IsMyLocalDev()
{
	TCHAR szName[255];
	DWORD dwSize = sizeof(szName);
	if (::GetComputerName(szName, &dwSize) == 0)
	{
		return false;
	}

	//AfxMessageBox(szName);
	wstring computerName = szName;
	if (computerName == _T("DESKTOP-7VJOH39"))
	{
		return true;
	}

	return false;
}

void CElementManager::OnFomatRotateRight90(CModeler1View* pView)
{
	// For each elements, +=90 to the rotateAngle
	for (shared_ptr<CElement> pElement : m_selection.m_objects)
	{
		// rotateAngle
		int angle = pElement->m_rotateAngle;
		angle += 90;
		pElement->m_rotateAngle = angle;
	}

	shared_ptr<CElement> pElement = m_selection.GetHead();
	UpdateUI(pView, pElement);
	pView->Invalidate();
}

void CElementManager::OnFomatRotateLeft90(CModeler1View* pView)
{
	// For each elements, -=90 to the rotateAngle
	for (shared_ptr<CElement> pElement : m_selection.m_objects)
	{
		// rotateAngle
		int angle = pElement->m_rotateAngle;
		angle -= 90;
		pElement->m_rotateAngle = angle;
	}

	shared_ptr<CElement> pElement = m_selection.GetHead();
	UpdateUI(pView, pElement);
	pView->Invalidate();
}

void CElementManager::ExpandHigh(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			int height = pElementBase->m_rect.Height();
			pObj->m_rect.top = pObj->m_rect.top;
			pObj->m_rect.bottom = pObj->m_rect.top + height;
			pObj->m_point = pObj->m_rect.TopLeft();
			InvalObj(pView, pObj);
		}

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::ExpandLarge(CModeler1View* pView)
{
	if (HasSelection())
	{
		shared_ptr<CElement> pElementBase = m_selection.m_objects[0];

		for (vector<std::shared_ptr<CElement>>::const_iterator itSel = m_selection.m_objects.begin(); itSel != m_selection.m_objects.end(); itSel++)
		{
			std::shared_ptr<CElement> pObj = *itSel;

			int width = pElementBase->m_rect.Width();
			pObj->m_rect.left = pObj->m_rect.left;
			pObj->m_rect.right = pObj->m_rect.left + width;
			pObj->m_point = pObj->m_rect.TopLeft();
			InvalObj(pView, pObj);
		}

		pView->GetDocument()->SetModifiedFlag();
	}
}

void CElementManager::SetActiveView(CModeler1View* pView, View view)
{
	CRuntimeClass* prt = RUNTIME_CLASS(CTabbedView);
	CView* pview = NULL;
	// Continue search in inactive View by T(o)m
	CModeler1Doc* pDoc = pView->GetDocument();
	POSITION pos = pDoc->GetFirstViewPosition();
	while (pos != NULL)
	{
		pview = pDoc->GetNextView(pos);
		CRuntimeClass* pRT = pview->GetRuntimeClass();

		if (prt = pRT)
		{
			CTabbedView* pTView = (CTabbedView*)pview;

			if (view == View::Source)
			{
				pTView->SetActiveView(1);
			}
			else
			{
				pTView->SetActiveView(0);
			}

			break;
		}
		pView = NULL;       // not valid vie
	}
}

void CElementManager::OnShapesLeftTop(CModeler1View* pView, ShapeType shapeType)
{
	//AfxMessageBox(_T("CElementManager::OnShapesLeftTop"));

	ElementType type = CElement::From(shapeType);
	shared_ptr<CElement> pNewElement = CFactory::CreateElementOfType(type, shapeType);
	pNewElement->m_point = CPoint(10, 10);
	pNewElement->m_rect = CRect(10, 10, 100, 100);
	pNewElement->m_pManager = this;
	pNewElement->m_pView = pView;
	pNewElement->m_text = _T("");

	m_objects.AddTail(pNewElement);

	SelectNone();
	Select(pNewElement);

	Invalidate(pView);
	UpdateUI(pView, pNewElement);

	pView->GetDocument()->UpdateAllViews(pView);
	//SetActiveView(pView, View::Source);
	//SetActiveView(pView, View::Modeling);

	pView->GetDocument()->SetModifiedFlag();

	//pView->UpdateWindow();
	/*
	RECT rect;
	rect.left = 0;
	rect.right = m_size.cx;
	rect.top = 0;
	rect.bottom = m_size.cy;
	::InvalidateRect(pView->m_hWnd, &rect, TRUE);
	*/
	//SendMessage(pView->m_hWnd, WM_PAINT, 0, 0);
	//SetActiveView(pView, View::Source);
	//SetActiveView(pView, View::Modeling);
	//SendMessage(pView->m_hWnd, WM_LBUTTONDOWN, 0, 0);
	//pView->SetFocus();
}

void CElementManager::OnShapesCenter(CModeler1View* pView, ShapeType shapeType)
{
	//AfxMessageBox(_T("CElementManager::OnShapesCenter"));

	ElementType type = CElement::From(shapeType);
	shared_ptr<CElement> pNewElement = CFactory::CreateElementOfType(type, shapeType);
	pNewElement->m_point = CPoint(500, 500);
	pNewElement->m_rect = CRect(500, 500, 600, 600);
	pNewElement->m_pManager = this;
	pNewElement->m_pView = pView;
	pNewElement->m_text = _T("");

	m_objects.AddTail(pNewElement);

	SelectNone();
	Select(pNewElement);

	Invalidate(pView);
	UpdateUI(pView, pNewElement);

	pView->GetDocument()->UpdateAllViews(pView);
	//SetActiveView(pView, View::Source);
	//SetActiveView(pView, View::Modeling);

	pView->GetDocument()->SetModifiedFlag();

}

void CElementManager::HideAllEditControls()
{
}

void CElementManager::OnFileSaveDatabase(CModeler1View* pView)
{
	CDialogSaveDatabase dlg;
	dlg.m_strDiagramName = pView->GetDocument()->GetTitle();

	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}

	// get data from dialog
	std::wstring wdiagramName = (LPTSTR)(LPCTSTR)dlg.m_strDiagramName;
	std::string diagramName(wdiagramName.begin(), wdiagramName.end());
	m_diagramName = wdiagramName;

	// serialize as json
	web::json::value jdata = AsJSON();
	wstring json = jdata.serialize();
	string strJson(json.begin(), json.end());

	// open database
	SQLite::Database db;
	string dbName = UFM_SQLITE_DATABASE;
	db.SetDatabaseName(dbName);
	if (!db.OpenEx(UFM_SQLITE_USER, UFM_SQLITE_PASSWORD))
		return;

	db.SetBusyTimeout(100000);

	// store to db
	SQLiteDiagramEntity diagramEntity(&db);
	diagramEntity.FileName = diagramName;
	diagramEntity.Json = strJson;
	diagramEntity.InsertOrUpdate(m_diagramId);
	db.Close();

	CString str;
	str.Format(_T("Save To Database... Id=%d"), m_diagramId);
	//AfxMessageBox(str); // _T("Save To Database..."));
}

void CElementManager::OnFileLoadDatabase(CModeler1View* pView)
{
	CDialogLoadDatabase dlg;
	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}

	wstring diagramName = dlg.m_diagramName;
	pView->GetDocument()->SetTitle(diagramName.c_str());

	auto it = find_if(dlg.m_vDiagrams.begin(), dlg.m_vDiagrams.end(), [diagramName](shared_ptr<SQLiteDiagramEntity> sde) {
		wstring fn(sde->FileName.begin(), sde->FileName.end());
		if (fn == diagramName)
		{
			return true;
		}
		return false;
	}
	);

	if (it == dlg.m_vDiagrams.end())
		return;

	shared_ptr<SQLiteDiagramEntity> sde = *it;
	string json = sde->Json;
	wstring wjson(json.begin(), json.end());
	m_diagramId = sde->DiagramPK;

	m_objects.RemoveAll();
	Invalidate(pView);

	try
	{
		web::json::value jdata = web::json::value::parse(wjson);
		FromJSON(jdata.as_object());
	}
	catch (web::json::json_exception ex)
	{
		string str = ex.what();
		wstring w(str.begin(), str.end());
		AfxMessageBox(w.c_str());
		return;
	}

	for (shared_ptr<CElement> pElement : GetObjects())
	{
		if (pElement->IsLine())
		{
			pElement->m_pConnector->m_pElement1 = m_objects.FindElementByName(pElement->m_connectorName1);
			pElement->m_pConnector->m_pElement2 = m_objects.FindElementByName(pElement->m_connectorName2);
		}

		pElement->m_pManager = this; // TODO

		POSITION pos = /*pNewElement->m_pView =*/ pView->GetDocument()->GetFirstViewPosition(); //nullptr; // TODO
		pElement->m_pView = (CModeler1View*)(pView->GetDocument()->GetNextView(pos)); //()->GetRoutingView();
	}

	// Build groups
	BuildGroups();

	BuildElementsCombo(pView);
}

void CElementManager::OnOperationDelete(CModeler1View* pView)
{
	// the clipboard is cleared
	//m_clipboard.RemoveAll();
	// the current selection is cleared
	RemoveSelectedObjects(pView);
}

void CElementManager::OnChar(CModeler1View* pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	std::shared_ptr<CElement> pElement = m_selection.GetHead();

	/*
	if (nChar == VK_RETURN)
	{
		pElement->m_vChar.push_back('\n');
	}
	else if (nChar == VK_BACK)
	{
		if (pElement->m_vChar.size() != 0)
		{
			//vector<char> newv;
			//copy(pElement->m_vChar.begin(), pElement->m_vChar.end() - 1, newv.begin());
			//pElement->m_vChar = newv;

			pElement->m_vChar.erase(pElement->m_vChar.end() - 1);
		}
	}
	else
	{
		pElement->m_vChar.push_back(nChar);
	}
	*/

	shared_ptr<CCharElement> pCharElement = make_shared<CCharElement>();

	if (nChar == VK_RETURN)
	{
		pCharElement->m_char = '\n';
		pElement->m_vCharElement.push_back(pCharElement);
		pElement->m_text.push_back('\n');
	}
	else if (nChar == VK_BACK)
	{
		if (pElement->m_vCharElement.size() != 0)
		{
			pElement->m_vCharElement.erase(pElement->m_vCharElement.end() - 1);
			pElement->m_text.erase(pElement->m_text.end() - 1);
		}
	}
	else
	{
		pCharElement->m_char = nChar;
		pElement->m_vCharElement.push_back(pCharElement);
		pElement->m_text.push_back(nChar);
	}


	//pElement->InvalidateObj();
	Invalidate(pView);
	//pView->GetDocument()->UpdateAllViews(pView);
}


void CElementManager::OnCharSpecial(CModeler1View* pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_LEFT)
	{
		for (std::shared_ptr<CElement> pElement : m_selection.m_objects)
		{
			pElement->m_rect.left -= 1;
			pElement->m_rect.right -= 1;
		}
		Invalidate(pView);
	}
	else if (nChar == VK_RIGHT)
	{
		for (std::shared_ptr<CElement> pElement : m_selection.m_objects)
		{
			pElement->m_rect.left += 1;
			pElement->m_rect.right += 1;
		}
		Invalidate(pView);
	}
	else if (nChar == VK_UP)
	{
		for (std::shared_ptr<CElement> pElement : m_selection.m_objects)
		{
			pElement->m_rect.top -= 1;
			pElement->m_rect.bottom -= 1;
		}
		Invalidate(pView);
	}
	else if (nChar == VK_DOWN)
	{
		for (std::shared_ptr<CElement> pElement : m_selection.m_objects)
		{
			pElement->m_rect.top += 1;
			pElement->m_rect.bottom += 1;
		}
		Invalidate(pView);
	}
	else if (nChar == VK_DELETE)
	{
		RemoveSelectedObjects(pView);
		Invalidate(pView);
	}
}

void CElementManager::CreateCaret(CModeler1View* pView)
{
	static bool done = false;

	if (done == false)
	{
		m_bmpCaret.LoadBitmap(IDB_CARET);
		::CreateCaret(pView->m_hWnd, (HBITMAP)m_bmpCaret.m_hObject, 2, 10); // pElement->m_fontSize); // 10);
	}

	done = true;
}

