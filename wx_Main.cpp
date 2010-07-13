﻿/***************************************************************
 * Name:	  wx_Main.cpp
 * Purpose:   Code for Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include <boost/config/warning_disable.hpp> /* против unsafe в wxWidgets */

#include <wx/msw/setup.h> /* Обязательно самым первым среди wxWidgets! */
#include <wx/msgdlg.h>
#include "wx_Main.h"

#include <string>
using namespace std;

//(*InternalHeaders(wx_MainFrame)
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/image.h>
#include <wx/string.h>
//*)

#include <wx/filename.h>

//(*IdInit(wx_MainFrame)
const long wx_MainFrame::ID_COMBOBOX1 = wxNewId();
const long wx_MainFrame::ID_CHOICE1 = wxNewId();
const long wx_MainFrame::ID_PANEL1 = wxNewId();
const long wx_MainFrame::ID_PANEL2 = wxNewId();
const long wx_MainFrame::ID_MENU_QUIT = wxNewId();
const long wx_MainFrame::ID_MENU_ABOUT = wxNewId();
const long wx_MainFrame::ID_STATUSBAR1 = wxNewId();
//*)

BEGIN_EVENT_TABLE(wx_MainFrame,wxFrame)
	//(*EventTable(wx_MainFrame)
	//*)
END_EVENT_TABLE()

wx_MainFrame::wx_MainFrame(wxWindow* parent, wxWindowID id)
	: cartographer_(0)
{
	//(*Initialize(wx_MainFrame)
	wxMenuItem* MenuItem2;
	wxMenuItem* MenuItem1;
	wxMenu* Menu1;
	wxMenuBar* MenuBar1;
	wxMenu* Menu2;

	Create(parent, wxID_ANY, _("MainFrame"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("wxID_ANY"));
	SetClientSize(wxSize(626,293));
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	{
	wxIcon FrameIcon;
	FrameIcon.CopyFromBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_TIP")),wxART_FRAME_ICON));
	SetIcon(FrameIcon);
	}
	FlexGridSizer1 = new wxFlexGridSizer(2, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	FlexGridSizer1->AddGrowableRow(1);
	Panel2 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxSize(616,61), wxTAB_TRAVERSAL, _T("ID_PANEL1"));
	ComboBox1 = new wxComboBox(Panel2, ID_COMBOBOX1, wxEmptyString, wxPoint(8,8), wxSize(208,24), 0, 0, wxCB_READONLY|wxCB_DROPDOWN, wxDefaultValidator, _T("ID_COMBOBOX1"));
	Choice1 = new wxChoice(Panel2, ID_CHOICE1, wxPoint(232,8), wxSize(192,24), 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
	FlexGridSizer1->Add(Panel2, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	Panel1 = new wxPanel(this, ID_PANEL2, wxDefaultPosition, wxSize(616,331), wxTAB_TRAVERSAL, _T("ID_PANEL2"));
	FlexGridSizer1->Add(Panel1, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);
	MenuBar1 = new wxMenuBar();
	Menu1 = new wxMenu();
	MenuItem1 = new wxMenuItem(Menu1, ID_MENU_QUIT, _("Выход\tAlt-F4"), wxEmptyString, wxITEM_NORMAL);
	Menu1->Append(MenuItem1);
	MenuBar1->Append(Menu1, _("Файл"));
	Menu2 = new wxMenu();
	MenuItem2 = new wxMenuItem(Menu2, ID_MENU_ABOUT, _("О программе...\tF1"), wxEmptyString, wxITEM_NORMAL);
	Menu2->Append(MenuItem2);
	MenuBar1->Append(Menu2, _("Помощь"));
	SetMenuBar(MenuBar1);
	StatusBar1 = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));
	int __wxStatusBarWidths_1[1] = { -1 };
	int __wxStatusBarStyles_1[1] = { wxSB_NORMAL };
	StatusBar1->SetFieldsCount(1,__wxStatusBarWidths_1);
	StatusBar1->SetStatusStyles(1,__wxStatusBarStyles_1);
	SetStatusBar(StatusBar1);
	FlexGridSizer1->SetSizeHints(this);

	Connect(ID_COMBOBOX1,wxEVT_COMMAND_COMBOBOX_SELECTED,(wxObjectEventFunction)&wx_MainFrame::OnComboBox1Select);
	Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&wx_MainFrame::OnChoice1Select);
	Connect(ID_MENU_QUIT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_MainFrame::OnQuit);
	Connect(ID_MENU_ABOUT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_MainFrame::OnAbout);
	//*)

	/* Очень обязательная вещь! */
	//setlocale(LC_ALL, "");


	bitmap_.LoadFile(L"test.png", wxBITMAP_TYPE_ANY);

	cartographer_ = new wxCartographer(
		Panel1 /* Window - на чём будем рисовать */
		, L"172.16.19.1" // L"127.0.0.1" /* ServerAddr - адрес сервера */
		, L"27543" /* ServerPort - порт сервера */
		, 1000 /* CacheSize - размер кэша (в тайлах) */
		, L"cache" /* CachePath - путь к кэшу на диске */
		, false /* OnlyCache - работать только с кэшем */
		, L"Google.Спутник" /* InitMap - исходная карта (Яндекс.Карта, Яндекс.Спутник, Google.Спутник) */
		, 12 /* InitZ - исходный масштаб (>1) */
		, 48.48021475 /* InitLat - широта исходной точки */
		, 135.0719556 /* InitLon - долгота исходной точки */
		, boost::bind(&wx_MainFrame::OnMapPaint, this, _1, _2, _3) /* OnPaintProc - функция рисования */
		);

	cartographer_->GetMaps(maps_);

	for( maps_list::iterator iter = maps_.begin();
		iter != maps_.end(); ++iter )
	{
		ComboBox1->Append(iter->name);
	}

	ComboBox1->SetValue( cartographer_->GetActiveMap().name );

	Choice1->Append(L"Хабаровск");
	Choice1->Append(L"Владивосток");
	Choice1->Append(L"Магадан");
	Choice1->Append(L"Якутск");
	Choice1->Append(L"Южно-Сахалинск");
	Choice1->Append(L"Петропавлоск-Камчатский");
}

wx_MainFrame::~wx_MainFrame()
{
	/* Удаление/остановка картографера обязательно должно быть выполнено
		до удаления всех объектов, использующихся в обработчике OnMapPaint */
	delete cartographer_;

	//(*Destroy(wx_MainFrame)
	//*)
}

void wx_MainFrame::OnQuit(wxCommandEvent& event)
{
	Close();
}

void wx_MainFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox( L"About...");
}


void wx_MainFrame::OnComboBox1Select(wxCommandEvent& event)
{
	std::wstring str = ComboBox1->GetValue();
	cartographer_->SetActiveMap(str);
}

wxCoord wx_MainFrame::DrawTextInBox(wxGCDC &gc,
	const wxString &str, wxCoord x, wxCoord y,
	const wxFont &font, const wxColour &color,
	const wxPen &pen, const wxBrush &brush)
{
	gc.SetFont(font);
	gc.SetTextForeground(color);
	gc.SetPen(pen);
	gc.SetBrush(brush);

	wxCoord w, h;
	gc.GetTextExtent(str, &w, &h);

	/* Центрируем */
	x -= w / 2.0;
	++y;

	gc.DrawRoundedRectangle(x - 4, y - 1, w + 8, h + 2, -0.2);
	gc.DrawText(str, x, y);

	return h;
}

void wx_MainFrame::OnMapPaint(wxGCDC &gc, wxCoord width, wxCoord height)
{
	wxCoord x = cartographer_->LonToX(135.04954039);
	wxCoord y = cartographer_->LatToY(48.47259794);

	int z = cartographer_->GetZ();

	wxDouble bmp_w = bitmap_.GetWidth();
	wxDouble bmp_h = bitmap_.GetHeight();

	if (z < 6)
	{
		bmp_w = bmp_w / 6 * z;
		bmp_h = bmp_h / 6 * z;
	}
	
	wxDouble bmp_x = x - bmp_w / 2;
	wxDouble bmp_y = y - bmp_h;

	//gc.DrawBitmap(bitmap_, bmp_x, bmp_y);

	gc.GetGraphicsContext()->DrawBitmap(bitmap_,
		bmp_x, bmp_y, bmp_w, bmp_h);
	
	wxString str;
	str = z > 12 ? L"Хабаровский утёс" : L"Хабаровск";

	int font_sz = (z == 1 ? 5 : z <=3 ? 6 : z <= 5 ? 7 : 8);

	DrawTextInBox(gc, str, x, y,
		wxFont(font_sz, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL),
		wxColour(0, 0, 0),
		wxPen( wxColor(0, 0, 0), 1 ),
		wxBrush( wxColor(255, 255, 0, 192) ));
}

void wx_MainFrame::OnChoice1Select(wxCommandEvent& event)
{
	switch (Choice1->GetCurrentSelection())
	{
		case 0: /* Хабаровск */
			cartographer_->MoveTo(12, 48.48021475, 135.0719556);
			break;

		case 1: /* Владивосток */
			cartographer_->MoveTo(13, FROM_DEG(43,7,17.95), FROM_DEG(131,55,34.4));
			break;

		case 2: /* Магадан */
			cartographer_->MoveTo(12, FROM_DEG(59,33,41.79), FROM_DEG(150,50,19.87));
			break;

		case 3: /* Якутск */
			cartographer_->MoveTo(10, FROM_DEG(62,4,30.33), FROM_DEG(129,45,24.39));
			break;

		case 4: /* Южно-Сахалинск */
			cartographer_->MoveTo(12, FROM_DEG(46,57,34.28), FROM_DEG(142,44,18.58));
			break;

		case 5: /* Петропавлоск-Камчатский */
			cartographer_->MoveTo(13, FROM_DEG(53,4,11.14), FROM_DEG(158,37,9.24));
			break;
	}
}
