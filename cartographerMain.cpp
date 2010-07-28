/***************************************************************
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
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/icon.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/image.h>
#include <wx/string.h>
//*)

//#include <wx/filename.h>

//(*IdInit(cartographerFrame)
const long cartographerFrame::ID_COMBOBOX1 = wxNewId();
const long cartographerFrame::ID_CHOICE1 = wxNewId();
const long cartographerFrame::ID_PANEL2 = wxNewId();
const long cartographerFrame::ID_MENU_QUIT = wxNewId();
const long cartographerFrame::ID_MENU_ABOUT = wxNewId();
const long cartographerFrame::ID_STATUSBAR1 = wxNewId();
//*)

BEGIN_EVENT_TABLE(cartographerFrame,wxFrame)
	//(*EventTable(cartographerFrame)
	//*)
END_EVENT_TABLE()

cartographerFrame::cartographerFrame(wxWindow* parent,wxWindowID id)
	: cartographer_(0)
{
	//(*Initialize(cartographerFrame)
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
	Panel2 = new wxPanel(this, ID_PANEL2, wxDefaultPosition, wxSize(616,61), wxTAB_TRAVERSAL, _T("ID_PANEL2"));
	ComboBox1 = new wxComboBox(Panel2, ID_COMBOBOX1, wxEmptyString, wxPoint(8,8), wxSize(208,24), 0, 0, wxCB_READONLY|wxCB_DROPDOWN, wxDefaultValidator, _T("ID_COMBOBOX1"));
	Choice1 = new wxChoice(Panel2, ID_CHOICE1, wxPoint(232,8), wxSize(192,24), 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
	FlexGridSizer1->Add(Panel2, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

	//Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxSize(616,331), wxTAB_TRAVERSAL, _T("ID_PANEL1"));
	//FlexGridSizer1->Add(Panel1, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

	MenuBar1 = new wxMenuBar();
	Menu1 = new wxMenu();
	MenuItem1 = new wxMenuItem(Menu1, ID_MENU_QUIT, _(L"Выход\tAlt-F4"), wxEmptyString, wxITEM_NORMAL);
	Menu1->Append(MenuItem1);
	MenuBar1->Append(Menu1, _(L"Файл"));
	Menu2 = new wxMenu();
	MenuItem2 = new wxMenuItem(Menu2, ID_MENU_ABOUT, _(L"О программе...\tF1"), wxEmptyString, wxITEM_NORMAL);
	Menu2->Append(MenuItem2);
	MenuBar1->Append(Menu2, _(L"Помощь"));
	SetMenuBar(MenuBar1);
	StatusBar1 = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));
	int __wxStatusBarWidths_1[1] = { -1 };
	int __wxStatusBarStyles_1[1] = { wxSB_NORMAL };
	StatusBar1->SetFieldsCount(1,__wxStatusBarWidths_1);
	StatusBar1->SetStatusStyles(1,__wxStatusBarStyles_1);
	SetStatusBar(StatusBar1);
	FlexGridSizer1->SetSizeHints(this);

	Connect(ID_COMBOBOX1,wxEVT_COMMAND_COMBOBOX_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnComboBox1Select);
	Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnChoice1Select);
	Connect(ID_MENU_QUIT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnQuit);
	Connect(ID_MENU_ABOUT,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&cartographerFrame::OnAbout);
	//*)

	SetClientSize(400, 400);
	Show(true);

	cartographer_ = new cgr::Cartographer(
		this
		, L"127.0.0.1" /* ServerAddr - адрес сервера */
		, L"27543" /* ServerPort - порт сервера */
		, 500 /* CacheSize - размер кэша (в тайлах) */
		, L"cache" /* CachePath - путь к кэшу на диске */
		, false /* OnlyCache - работать только с кэшем */
		, L"Google.Спутник" /* InitMap - исходная карта (Яндекс.Карта, Яндекс.Спутник, Google.Спутник) */
		//, L"Яндекс.Карта" /* InitMap - исходная карта (Яндекс.Карта, Яндекс.Спутник, Google.Спутник) */
		, 2 /* InitZ - исходный масштаб (>1) */
		, 48.48021475 /* InitLat - широта исходной точки */
		, 135.0719556 /* InitLon - долгота исходной точки */
		, boost::bind(&cartographerFrame::OnMapPaint, this, _1, _2, _3) /* OnPaintProc - функция рисования */
		, 50, 5 /* 0 - нет анимации */
  	);
	FlexGridSizer1->Add(cartographer_, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

	SetSizer(FlexGridSizer1);

	/* Очень обязательная вещь! */
	//setlocale(LC_ALL, "");

/*-
	test_image_id_ = cartographer_->LoadImageFromFile(L"test.png");
	cartographer_->SetImageCenter(test_image_id_, 0.5, 1.0);

	int maps_count = cartographer_->GetMapsCount();
	cgr::map_info map;

	for( int i = 0; i < maps_count; ++i)
	{
		map = cartographer_->GetMapInfo(i);
		ComboBox1->Append(map.name);
	}

	map = cartographer_->GetActiveMapInfo();
	ComboBox1->SetValue(map.name);
-*/
	Choice1->Append(L"Хабаровск");
	Choice1->Append(L"Владивосток");
	Choice1->Append(L"Магадан");
	Choice1->Append(L"Якутск");
	Choice1->Append(L"Южно-Сахалинск");
	Choice1->Append(L"Петропавлоск-Камчатский");
}

cartographerFrame::~cartographerFrame()
{
	/* Остановка картографера обязательно должна быть выполнена
		до удаления всех объектов, использующихся в обработчике OnMapPaint */
	cartographer_->Stop();

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
	cartographer_->SetActiveMapByName(str);
}

wxCoord cartographerFrame::DrawTextInBox(wxGCDC &gc,
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

void cartographerFrame::OnMapPaint(wxGCDC &gc, wxCoord width, wxCoord height)
{
/*-
	cgr::point pt = cartographer_->ll_to_xy(48.47259794, 135.04954039);

	double z = cartographer_->GetActiveZ();

	pt = cartographer_->ll_to_xy(48.48021475, 135.0719556);

	cgr::size img_sz = cartographer_->GetImageSize(test_image_id_);

	if (z < 6.0)
	{
		img_sz.width *= z / 6.0;
		img_sz.height *= z / 6.0;
	}

	glColor4d(1.0, 1.0, 1.0, 1.0);
	cartographer_->DrawImage(test_image_id_, pt.x, pt.y, img_sz.width, img_sz.height);
-*/
	/*-
	wxString str;
	str = z > 12 ? L"Хабаровский утёс" : L"Хабаровск";

	int font_sz = (z == 1 ? 5 : z <=3 ? 6 : z <= 5 ? 7 : 8);

	DrawTextInBox(gc, str, x, y,
		wxFont(font_sz, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL),
		wxColour(255, 255, 255),
		wxPen( wxColor(0, 0, 0), 1 ),
		wxBrush( wxColor(255, 255, 0, 192) ));
	-*/

	/*
	glBegin(GL_QUADS);
		glColor4d(1.0, 0.0, 0.0, 0.5);
		glVertex3i(100, 100, 0);
		glVertex3i(200, 100, 0);
		glColor4d(0.0, 1.0, 0.0, 0.5);
		glVertex3i(200, 200, 0);
		glVertex3i(100, 200, 0);
	glEnd();

	glBegin(GL_QUADS);
		glColor4d(0.0, 1.0, 0.0, 0.5);
		glVertex3i(200, 200, 0);
		glVertex3i(300, 200, 0);
		glColor4d(0.0, 0.0, 1.0, 0.5);
		glVertex3i(300, 300, 0);
		glVertex3i(200, 300, 0);
	glEnd();

	glBegin(GL_QUADS);
		glColor4d(0.0, 0.0, 1.0, 0.5);
		glVertex3i(300, 300, 0);
		glVertex3i(400, 300, 0);
		glColor4d(1.0, 0.0, 0.0, 0.5);
		glVertex3i(400, 400, 0);
		glVertex3i(300, 400, 0);
	glEnd();
	-*/
}

void cartographerFrame::OnChoice1Select(wxCommandEvent& event)
{
	switch (Choice1->GetCurrentSelection())
	{
		case 0: /* Хабаровск */
			cartographer_->MoveTo(12, 48.48021475, 135.0719556);
			break;

		case 1: /* Владивосток */
			cartographer_->MoveTo(13,
				cgr::DegreesToGeo(43,7,17.95), cgr::DegreesToGeo(131,55,34.4));
			break;

		case 2: /* Магадан */
			cartographer_->MoveTo(12,
				cgr::DegreesToGeo(59,33,41.79), cgr::DegreesToGeo(150,50,19.87));
			break;

		case 3: /* Якутск */
			cartographer_->MoveTo(10,
				cgr::DegreesToGeo(62,4,30.33), cgr::DegreesToGeo(129,45,24.39));
			break;

		case 4: /* Южно-Сахалинск */
			cartographer_->MoveTo(12,
				cgr::DegreesToGeo(46,57,34.28), cgr::DegreesToGeo(142,44,18.58));
			break;

		case 5: /* Петропавлоск-Камчатский */
			cartographer_->MoveTo(13,
				cgr::DegreesToGeo(53,4,11.14), cgr::DegreesToGeo(158,37,9.24));
			break;
	}
}
