/***************************************************************
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
	ComboBox1 = new wxComboBox(Panel2, ID_COMBOBOX1, wxEmptyString, wxPoint(8,8), wxSize(208,24), 0, 0, wxCB_DROPDOWN, wxDefaultValidator, _T("ID_COMBOBOX1"));
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
	Connect(ID_MENU_QUIT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_MainFrame::OnQuit);
	Connect(ID_MENU_ABOUT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&wx_MainFrame::OnAbout);
	//*)

	/* Очень обязательная вещь! */
	//setlocale(LC_ALL, "");

	cartographer_.reset( new wxCartographer(
		Panel1 /* window - на чём будем рисовать */
		, L"127.0.0.1" //L"172.16.19.1" /* server - адрес сервера */
		, L"27543" /* port - порт сервера */
		, 1000 /* cache_size - размер кэша (в тайлах) */
		, L"cache" /* cache_path - путь к кэшу на диске */
		, false /* only_cache - работать только с кэшем */
		, L"Google.Спутник" /* init_map - исходная карта (Яндекс.Карта, Яндекс.Спутник, Google.Спутник) */
		, 1 /* init_z - исходный масштаб (>1) */
		, 48.48021475 /* init_lat - широта исходной точки */
		, 135.0719556 /* init_lon - долгота исходной точки */
		) );

	cartographer_->GetMaps(maps_);

	for( maps_list::iterator iter = maps_.begin();
		iter != maps_.end(); ++iter )
	{
		ComboBox1->Append(iter->name);
	}
}

wx_MainFrame::~wx_MainFrame()
{
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
