﻿/***************************************************************
 * Name:      cartographerMain.cpp
 * Purpose:   Code for Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-07-16
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include "cartographerMain.h"

#include <string>

//(*InternalHeaders(cartographerFrame)
#include <wx/bitmap.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/image.h>
#include <wx/string.h>
//*)

#include "images/green_mark16.c"
#include "images/red_mark16.c"
#include "images/yellow_mark16.c"

//#include <wx/filename.h>

//(*IdInit(cartographerFrame)
const long cartographerFrame::ID_COMBOBOX1 = wxNewId();
const long cartographerFrame::ID_CHOICE1 = wxNewId();
const long cartographerFrame::ID_PANEL2 = wxNewId();
const long cartographerFrame::ID_PANEL1 = wxNewId();
const long cartographerFrame::ID_MENU_QUIT = wxNewId();
const long cartographerFrame::ID_MENU_ABOUT = wxNewId();
const long cartographerFrame::ID_STATUSBAR1 = wxNewId();
const long cartographerFrame::ID_ZOOMIN = wxNewId();
const long cartographerFrame::ID_ZOOMOUT = wxNewId();
const long cartographerFrame::ID_ANCHOR = wxNewId();
const long cartographerFrame::ID_TOOLBAR1 = wxNewId();
//*)

/*-
wxImage convert_to_image()
{
	wxImage img(width_, height_, true);
	if
}
-*/

BEGIN_EVENT_TABLE(cartographerFrame,wxFrame)
	//(*EventTable(cartographerFrame)
	//*)
END_EVENT_TABLE()

cartographerFrame::cartographerFrame(wxWindow* parent,wxWindowID id)
	: Cartographer(0)
	, big_font_(0)
	, small_font_(0)
{
	#undef _
	#define _(s) (L##s)

	//(*Initialize(cartographerFrame)
	wxMenuItem* MenuItem2;
	wxMenuItem* MenuItem1;
	wxMenu* Menu1;
	wxMenuBar* MenuBar1;
	wxMenu* Menu2;

	Create(parent, wxID_ANY, _("MainFrame"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("wxID_ANY"));
	SetClientSize(wxSize(626,293));
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	FlexGridSizer1 = new wxFlexGridSizer(2, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	FlexGridSizer1->AddGrowableRow(1);
	Panel2 = new wxPanel(this, ID_PANEL2, wxDefaultPosition, wxSize(616,61), wxTAB_TRAVERSAL, _T("ID_PANEL2"));
	ComboBox1 = new wxComboBox(Panel2, ID_COMBOBOX1, wxEmptyString, wxPoint(8,8), wxSize(208,24), 0, 0, wxCB_READONLY|wxCB_DROPDOWN, wxDefaultValidator, _T("ID_COMBOBOX1"));
	Choice1 = new wxChoice(Panel2, ID_CHOICE1, wxPoint(232,8), wxSize(192,24), 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
	FlexGridSizer1->Add(Panel2, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxSize(616,331), wxTAB_TRAVERSAL, _T("ID_PANEL1"));
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
	ToolBar1 = new wxToolBar(this, ID_TOOLBAR1, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL|wxNO_BORDER, _T("ID_TOOLBAR1"));
	ToolBarItem1 = ToolBar1->AddTool(ID_ZOOMIN, _("ZoomIn"), wxBitmap(wxImage(_T("images/zoom-in-32.png"))), wxNullBitmap, wxITEM_NORMAL, _("Увеличить"), wxEmptyString);
	ToolBarItem2 = ToolBar1->AddTool(ID_ZOOMOUT, _("ZoomOut"), wxBitmap(wxImage(_T("images/zoom-out-32.png"))), wxNullBitmap, wxITEM_NORMAL, _("Уменьшить"), wxEmptyString);
	ToolBarItem3 = ToolBar1->AddTool(ID_ANCHOR, _("Anchor"), wxBitmap(wxImage(_T("images/binoculars-32.png"))), wxNullBitmap, wxITEM_CHECK, _("Следить"), wxEmptyString);
	ToolBar1->Realize();
	SetToolBar(ToolBar1);
	FlexGridSizer1->SetSizeHints(this);

	Connect(ID_COMBOBOX1,wxEVT_COMMAND_COMBOBOX_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnComboBox1Select);
	Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnChoice1Select);
	Connect(ID_MENU_QUIT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnQuit);
	Connect(ID_MENU_ABOUT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnAbout);
	Connect(ID_ZOOMIN,wxEVT_COMMAND_TOOL_CLICKED,(wxObjectEventFunction)&cartographerFrame::OnZoomInButtonClick);
	Connect(ID_ZOOMOUT,wxEVT_COMMAND_TOOL_CLICKED,(wxObjectEventFunction)&cartographerFrame::OnZoomOutButtonClick);
	Connect(ID_ANCHOR,wxEVT_COMMAND_TOOL_CLICKED,(wxObjectEventFunction)&cartographerFrame::OnAnchorButtonClick);
	//*)

	setlocale(LC_ALL, "Russian_Russia.1251");	setlocale(LC_NUMERIC, "C");
	{
		wxIcon FrameIcon;
		FrameIcon.CopyFromBitmap(wxBitmap(wxImage(_T("images/cartographer.png"))));
		SetIcon(FrameIcon);
	}

	SetClientSize(400, 400);
	Maximize(true);
	Show(true);

	/* Обязательно обнуляем, а то получим ошибку, т.к. картинки начнут
		использоваться ещё до того, как мы успеем их загрузить */
	for (int i = 0; i < count_; ++i)
		images_[i]= 0;

	Cartographer = new cartographer::Painter(this, L"172.16.19.1");
	//Cartographer = new cartographer::Painter(this, L"127.0.0.1");

	delete Panel1;
	FlexGridSizer1->Add(Cartographer, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);

	big_font_ = Cartographer->CreateFont(
		wxFont(12, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD) );
	small_font_ = Cartographer->CreateFont(
		wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD) );

	/* Список карт */
	int maps_count = Cartographer->GetMapsCount();
	for( int i = 0; i < maps_count; ++i)
	{
		cartographer::map_info map = Cartographer->GetMapInfo(i);
		ComboBox1->Append(map.name);
	}

	/* Текущая карта */
	cartographer::map_info map = Cartographer->GetActiveMapInfo();
	ComboBox1->SetValue(map.name);

	/* Изображения */
	green_mark16_id_ = Cartographer->LoadImageFromC(green_mark16);
	red_mark16_id_ = Cartographer->LoadImageFromC(red_mark16);
	yellow_mark16_id_ = Cartographer->LoadImageFromC(yellow_mark16);


	/* Метки - не забыть изменить размер массива (!),
		когда надо будет добавить ещё */
	images_[0] = Cartographer->LoadImageFromFile(L"images/blu-blank.png");
	Cartographer->SetImageCentralPoint(images_[0], 31.5, 64.0);

	images_[1] = Cartographer->LoadImageFromFile(L"images/back.png");
	Cartographer->SetImageCentralPoint(images_[1], -1.0, 15.5);

	images_[2] = Cartographer->LoadImageFromFile(L"images/forward.png");
	Cartographer->SetImageCentralPoint(images_[2], 32.0, 15.5);

	images_[3] = Cartographer->LoadImageFromFile(L"images/up.png");
	Cartographer->SetImageCentralPoint(images_[3], 15.5, -1.0);

	images_[4] = Cartographer->LoadImageFromFile(L"images/down.png");
	Cartographer->SetImageCentralPoint(images_[4], 15.5, 31.0);

	images_[5] = Cartographer->LoadImageFromFile(L"images/flag.png");
	Cartographer->SetImageCentralPoint(images_[5], 3.5, 31.0);

	images_[6] = Cartographer->LoadImageFromFile(L"images/write.png");
	Cartographer->SetImageCentralPoint(images_[6], 1.0, 31.0);

	images_[7] = Cartographer->LoadImageFromFile(L"images/ylw-pushpin.png");
	Cartographer->SetImageCentralPoint(images_[7], 18.0, 63.0);
	Cartographer->SetImageScale(images_[7], 0.5, 0.5);

	images_[8] = Cartographer->LoadImageFromFile(L"images/wifi.png");
	Cartographer->SetImageCentralPoint(images_[8], 15.5, 35.0);

	images_[9] = Cartographer->LoadImageFromFile(L"images/blue_star.png");
	Cartographer->SetImageCentralPoint(images_[9], 7.0, 8.0);

	images_[10] = Cartographer->LoadImageFromFile(L"images/green_star.png");
	Cartographer->SetImageCentralPoint(images_[10], 7.0, 8.0);

	/* Места для быстрого перехода */
	names_[0] = L"Хабаровск";
	z_[0] = 12;
	coords_[0] = cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 );

	names_[1] = L"Владивосток";
	z_[1] = 13;
	coords_[1] = cartographer::DMSToDD( 43,7,17.95, 131,55,34.4 );

	names_[2] = L"Магадан";
	z_[2] = 12;
	coords_[2] = cartographer::DMSToDD( 59,33,41.79, 150,50,19.87 );

	names_[3] = L"Якутск";
	z_[3] = 10;
	coords_[3] = cartographer::DMSToDD( 62,4,30.33, 129,45,24.39 );

	names_[4] = L"Южно-Сахалинск";
	z_[4] = 12;
	coords_[4] = cartographer::DMSToDD( 46,57,34.28, 142,44,18.58 );

	names_[5] = L"Петропавловск-Камчатский";
	z_[5] = 13;
	coords_[5] = cartographer::DMSToDD( 53,4,11.14, 158,37,9.24 );

	names_[6] = L"Бикин";
	z_[6] = 11;
	coords_[6] = cartographer::DMSToDD( 46,48,47.59, 134,14,55.71 );

	names_[7] = L"Благовещенск";
	z_[7] = 14;
	coords_[7] = cartographer::DMSToDD( 50,16,55.96, 127,31,46.09 );

	names_[8] = L"Биробиджан";
	z_[8] = 14;
	coords_[8] = cartographer::DMSToDD( 48,47,52.55, 132,55,5.13 );

	for (int i = 0; i < count_; ++i)
		Choice1->Append(names_[i]);

	Cartographer->SetPainter(
		boost::bind(&cartographerFrame::OnMapPaint, this, _1, _2));

	Cartographer->MoveTo(z_[0], coords_[0]);

	Test();
}

void cartographerFrame::Test()
{
	using cartographer::coord;

	double d, d2;
	double a1, a2;
	double eps_in_m = 0.01;

	/* Точки-антиподы */
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, -180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(10.0, 0.0), coord(-10.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(20.0, 0.0), coord(-20.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(30.0, 0.0), coord(-30.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(40.0, 0.0), coord(-40.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(50.0, 0.0), coord(-50.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(60.0, 0.0), coord(-60.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(70.0, 0.0), coord(-70.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(80.0, 0.0), coord(-80.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(90.0, 0.0), coord(-90.0, 180.0), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 170.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 177.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 178.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 179.3964), coord(0.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 179.3965), coord(0.0, 180.0), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.3964), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.3965), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.3966), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.397), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.40), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.41), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.42), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.43), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.44), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.45), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.46), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.47), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.48), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.49), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.50), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.55), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.60), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.65), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.70), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.75), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.80), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.85), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.90), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.95), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.96), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.97), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.98), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 180.0), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.30), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.31), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.32), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.33), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.34), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.341), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.342), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.343), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.344), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.345), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.346), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.347), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.348), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.349), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.35), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.36), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.37), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.38), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.39), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.40), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.41), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.42), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.43), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.44), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.45), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.46), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.47), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.48), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.50), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.60), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.70), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.80), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 179.90), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.1, 180.00), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.10, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.20, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.30, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.40, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.50, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.60, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.70, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.80, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.90, 179.99), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(1.00, 179.99), &a1, &a2, eps_in_m );

	/* Близкие точки */
	{
		double eps_in_m = 0.0000000001;
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,29,41.65, 135,6,4.73 ),
			cartographer::DMSToDD( 48,29,41.43, 135,6,4.29 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,29,41.65, 135,6,4.73 ),
			cartographer::DMSToDD( 48,29,41.43, 135,6,4.29 ));
	}

	coord pt = cartographer::Direct(
		cartographer::DMSToDD( 48,29,41.65, 135,6,4.73 ),
		233.04672768896376, 11.303987628995905);

	/* Точки по стране */
	{
		double eps_in_m = 0.0000001;
		/* Хабаровск - Москва */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 55,45,15.01, 37,37,12.14 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 55,45,15.01, 37,37,12.14 ));

		coord pt = cartographer::Direct(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			317.193349, 6158610.810);

		/* Хабаровск - Магадан */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 59,33,41.79, 150,50,19.87 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 59,33,41.79, 150,50,19.87 ));

		/* Хабаровск - Якутск */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 62,4,30.33,  129,45,24.39 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 62,4,30.33,  129,45,24.39 ));

		/* Магадан - Якутск */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 59,33,41.79, 150,50,19.87 ),
			cartographer::DMSToDD( 62,4,30.33,  129,45,24.39 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 59,33,41.79, 150,50,19.87 ),
			cartographer::DMSToDD( 62,4,30.33,  129,45,24.39 ));

		/* Хабаровск - Бикин */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 46,48,47.59, 134,14,55.71 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 46,48,47.59, 134,14,55.71 ));

		/* Хабаровск - Биробиджан */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 48,47,52.55, 132,55,5.13 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 48,47,52.55, 132,55,5.13 ));

		/* Хабаровск - Владивосток */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 43,7,17.95,  131,55,34.4 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
			cartographer::DMSToDD( 43,7,17.95,  131,55,34.4 ));

		/* Владивосток - Хабаровск */
		d = cartographer::Inverse(
			cartographer::DMSToDD( 43,7,17.95,  131,55,34.4 ),
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ), &a1, &a2, eps_in_m );
		d2 = cartographer::FastDistance(
			cartographer::DMSToDD( 43,7,17.95,  131,55,34.4 ),
			cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ));
	}

	/*-
	//double dB = cartographer::Inverse( coord(-90.0, 0.0), coord(90.0, 0.0), &a1, &a2, eps_in_m );
	//double dC = cartographer::Inverse( coord(45.0, 135.0), coord(-45.0, -45.0), &a1, &a2, eps_in_m );

	double d1 = cartographer::Inverse( coords_[2], coords_[0], &a1, &a2, eps_in_m );
	double d2 = cartographer::Inverse( coord(0.0, 0.0), coord(90.0, 0.0), &a1, &a2, eps_in_m );
	-*/

	/*-
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 90.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 170.0), coord(0.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 179.0), coord(0.0, 180.0), &a1, &a2, eps_in_m );

	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 170.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 177.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 178.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 179.3964), coord(0.0, 180.0), &a1, &a2, eps_in_m );
	d = cartographer::Inverse( coord(0.0, 179.3965), coord(0.0, 180.0), &a1, &a2, eps_in_m );

	double d3 = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.3964), &a1, &a2, eps_in_m );
	double d3a= cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 179.3965), &a1, &a2, eps_in_m );
	-*/

	/*-
	double d4 = cartographer::Inverse( coord(0.0, 0.0), coord(0.0, 90.0), &a1, &a2, eps_in_m );
	double d5 = cartographer::Inverse( coord(0.0, 0.0), coord(45.0, 90.0), &a1, &a2, eps_in_m );
	double d6 = cartographer::Inverse( coord(0.0, 0.0), coord(45.0, -90.0), &a1, &a2, eps_in_m );
	double d7 = cartographer::Inverse( coord(0.0, 0.0), coord(-45.0, 90.0), &a1, &a2, eps_in_m );
	double d8 = cartographer::Inverse( coord(0.0, 0.0), coord(-45.0, -90.0), &a1, &a2, eps_in_m );
	-*/

	return;
}

cartographerFrame::~cartographerFrame()
{
	/* Остановка картографера обязательно должна быть выполнена
		до удаления всех объектов, использующихся в обработчике OnMapPaint */
	Cartographer->Stop();

	//(*Destroy(cartographerFrame)
	//*)
}

void cartographerFrame::OnQuit(wxCommandEvent& event)
{
	Close();
}

void cartographerFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox( L"About...");
}

void cartographerFrame::OnComboBox1Select(wxCommandEvent& event)
{
	std::wstring str = (const wchar_t *)ComboBox1->GetValue().c_str();
	Cartographer->SetActiveMapByName(str);
}

void cartographerFrame::OnMapPaint(double z, const cartographer::size &screen_size)
{
	static double angle = 0.0;

	angle += 5.0;
	if (angle > 360.0)
		angle -= 360.0;

	for (int i = 0; i < count_; ++i)
		Cartographer->DrawImage(images_[i], coords_[i], 1.0, 1.0, angle);

	/* Путь от Хабаровска до Москвы */
	{
		cartographer::coord pt1 = cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 );
		cartographer::coord pt2 = cartographer::DMSToDD( 55,45,15.01, 37,37,12.14 );

		Cartographer->DrawPath(pt1, pt2, 4.0, cartographer::color(1.0, 0.5, 0.0));

		/* Круг, примерно в центре пути */
		cartographer::coord pt = cartographer::DMSToDD( 62,57,0.84, 91,18,8.73 );

		/* ... с учётом картографических искажений */
		Cartographer->DrawCircle(pt, 1000000.0, 4.0,
			cartographer::color(1.0, 0.0, 0.0), cartographer::color(1.0, 0.0, 0.0, 0.3));

		/* ... и для сравнения - без учёта картографических искажений */
		Cartographer->DrawSimpleCircle(pt, 1000000.0, 4.0,
			cartographer::color(0.0, 0.0, 1.0, 1.0), cartographer::color(0.0, 0.0, 0.0, 0.5));
		Cartographer->DrawImage(images_[9], pt);
	}

	/* Круг на Сахалине радиусом 100 км */
	{
		cartographer::coord pt = coords_[4];

		/* ... с учётом картографических искажений */
		Cartographer->DrawCircle(pt, 100000.0, 4.0,
			cartographer::color(1.0, 0.0, 0.0), cartographer::color(1.0, 0.0, 0.0, 0.3));

		/* ... и для сравнения - без учёта картографических искажений */
		Cartographer->DrawSimpleCircle(pt, 100000.0, 4.0,
			cartographer::color(0.0, 0.0, 1.0, 1.0), cartographer::color(0.0, 0.0, 0.0, 0.5));

		Cartographer->DrawImage(images_[9], pt);
	}

	Cartographer->DrawPath( cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
		-30.0, 40000000.0, 4.0, cartographer::color(0.0, 1.0, 0.0));

	Cartographer->DrawPath( cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 ),
		90.0, 40000000.0, 4.0, cartographer::color(0.0, 0.5, 1.0));

	Cartographer->DrawImage(images_[9], cartographer::DMSToDD( -54,39,51.45, -65,13,31.03 ));

	Cartographer->DrawImage(images_[9], cartographer::DMSToDD( 48,28,43.0, 135,4,5.0 ));

	/* Простые круги с радиусом в метрах */
	Cartographer->DrawSimpleCircle( cartographer::DMSToDD( 48,28,36.0, 135,3,53.0 ),
		10.0, 2.0, cartographer::color(1.0, 0.0, 0.0),
		cartographer::color(1.0, 0.0, 0.0, 0.5));
	Cartographer->DrawSimpleCircle( cartographer::DMSToDD( 48,28,38.0, 135,3,58.0 ),
		13.3, 2.0, cartographer::color(1.0, 0.67, 0.0),
		cartographer::color(1.0, 0.67, 0.0, 0.5));
	Cartographer->DrawSimpleCircle( cartographer::DMSToDD( 48,28,40.0, 135,4,2.0 ),
		16.7, 2.0, cartographer::color(0.67, 1.0, 0.0),
		cartographer::color(0.67, 1.0, 0.0, 0.5));
	Cartographer->DrawSimpleCircle( cartographer::DMSToDD( 48,28,41.5, 135,4,5.5 ),
		20.0, 2.0, cartographer::color(0.0, 1.0, 0.0),
		cartographer::color(0.0, 1.0, 0.0, 0.5));
	Cartographer->DrawSimpleCircle( cartographer::DMSToDD( 48,28,43.5, 135,4,9.0 ),
		16.7, 2.0, cartographer::color(0.67, 1.0, 0.0),
		cartographer::color(0.67, 1.0, 0.0, 0.5));

	/* Простые круги с радиусом в пикселях */
	Cartographer->DrawSimpleCircle( cartographer::point(25, 25),
		23, 3.0, cartographer::color(0.0, 0.5, 1.0),
		cartographer::color(0.0, 0.0, 0.0, 0.0));
	Cartographer->DrawSimpleCircle( cartographer::point(50, 50),
		23, 3.0, cartographer::color(1.0, 0.5, 0.0),
		cartographer::color(0.0, 0.0, 0.0, 0.0));
	Cartographer->DrawSimpleCircle( cartographer::point(75, 25),
		23, 3.0, cartographer::color(0.0, 0.0, 0.0),
		cartographer::color(0.0, 0.0, 0.0, 0.0));
	Cartographer->DrawSimpleCircle( cartographer::point(100, 50),
		23, 3.0, cartographer::color(0.0, 1.0, 0.0),
		cartographer::color(0.0, 0.0, 0.0, 0.0));
	Cartographer->DrawSimpleCircle( cartographer::point(125, 25),
		23, 3.0, cartographer::color(1.0, 0.0, 0.0),
		cartographer::color(0.0, 0.0, 0.0, 0.0));

	{
		cartographer::coord pt = cartographer::DMSToDD( 48,28,48.77, 135,4,19.04 );
		cartographer::point pt_pos = Cartographer->CoordToScreen(pt);

		double a = Cartographer->FlashAlpha();

		std::string cstr = "Хабаровск";
		std::wstring wstr = my::str::to_wstring(cstr);		//std::wstring wstr = my::utf8::decode(cstr);

		cartographer::size sz
			= Cartographer->DrawText(big_font_,
				wstr, //L"Хабаровск",
				pt_pos, cartographer::color(1.0, 1.0, 1.0, a),
				cartographer::ratio(0.5, 0.0));

		pt_pos.y += sz.height;
		Cartographer->DrawText(small_font_,
			L"(столица Дальнего Востока)",
			pt_pos, cartographer::color(1.0, 1.0, 1.0, a),
			cartographer::ratio(0.5, 0.0));

		Cartographer->DrawImage(green_mark16_id_, pt);

		pt.lon += 0.01;
		Cartographer->DrawImage(green_mark16_id_, pt);
		Cartographer->DrawImage(yellow_mark16_id_, pt, 1.0, 0.33);

		pt.lon += 0.01;
		Cartographer->DrawImage(green_mark16_id_, pt);
		Cartographer->DrawImage(yellow_mark16_id_, pt, 1.0, 0.67);

		pt.lon += 0.01;
		Cartographer->DrawImage(yellow_mark16_id_, pt);

		pt.lon += 0.01;
		Cartographer->DrawImage(yellow_mark16_id_, pt);
		Cartographer->DrawImage(red_mark16_id_, pt, 1.0, 0.33);

		pt.lon += 0.01;
		Cartographer->DrawImage(yellow_mark16_id_, pt);
		Cartographer->DrawImage(red_mark16_id_, pt, 1.0, 0.67);

		pt.lon += 0.01;
		Cartographer->DrawImage(red_mark16_id_, pt);
	}

	{
		cartographer::coord pt = cartographer::DMSToDD( 55,45,15.01, 37,37,12.14 );
		cartographer::point pt_pos = Cartographer->CoordToScreen(pt);

		cartographer::size sz
			= Cartographer->DrawText(big_font_, L"Москва",
				pt_pos, cartographer::color(1.0, 1.0, 0.0),
				cartographer::ratio(0.5, 0.0), cartographer::ratio(0.8, 0.8));

		pt_pos.y += sz.height;
		Cartographer->DrawText(small_font_, L"(столица России)",
			pt_pos, cartographer::color(1.0, 1.0, 0.0),
			cartographer::ratio(0.5, 0.0), cartographer::ratio(0.8, 0.8));

		Cartographer->DrawImage(red_mark16_id_, pt);
	}
}

void cartographerFrame::OnChoice1Select(wxCommandEvent& event)
{
	int i = Choice1->GetCurrentSelection();
	Cartographer->MoveTo(z_[i], coords_[i]);
}

void cartographerFrame::OnZoomInButtonClick(wxCommandEvent& event)
{
	Cartographer->ZoomIn();
}

void cartographerFrame::OnZoomOutButtonClick(wxCommandEvent& event)
{
	Cartographer->ZoomOut();
}

void cartographerFrame::OnAnchorButtonClick(wxCommandEvent& event)
{
	//AnchorButton->SetValue( !AnchorButton->GetValue() );
}
