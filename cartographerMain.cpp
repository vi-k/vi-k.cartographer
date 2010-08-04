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
#include <wx/bitmap.h>
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
const long cartographerFrame::ID_PANEL1 = wxNewId();
const long cartographerFrame::ID_MENU_QUIT = wxNewId();
const long cartographerFrame::ID_MENU_ABOUT = wxNewId();
const long cartographerFrame::ID_STATUSBAR1 = wxNewId();
const long cartographerFrame::ID_ZOOMIN = wxNewId();
const long cartographerFrame::ID_ZOOMOUT = wxNewId();
const long cartographerFrame::ID_ANCHOR = wxNewId();
const long cartographerFrame::ID_TOOLBAR1 = wxNewId();
//*)

BEGIN_EVENT_TABLE(cartographerFrame,wxFrame)
	//(*EventTable(cartographerFrame)
	//*)
END_EVENT_TABLE()

const double c_PI = M_PI;
const double c_A = 6378137;
const double c_a = 1/298.257223563;
const double c_e2 = 2*c_a - c_a*c_a;

struct point_t {
	// Longitude and latitude, in degrees.
	float x, y;

	point_t () {}
	point_t (float _x, float _y) : x (_x), y (_y) {}

	bool operator == (const point_t & _other) const {return x == _other.x && y == _other.y;}
};

float Distance (const point_t & _1, const point_t & _2) {
	const double fSinB1 = ::sin (_1.y*c_PI/180);
	const double fCosB1 = ::cos (_1.y*c_PI/180);
	const double fSinL1 = ::sin (_1.x*c_PI/180);
	const double fCosL1 = ::cos (_1.x*c_PI/180);

	const double N1 = c_A/::sqrt (1 - c_e2*fSinB1*fSinB1);

	const double X1 = N1*fCosB1*fCosL1;
	const double Y1 = N1*fCosB1*fSinL1;
	const double Z1 = (1 - c_e2)*N1*fSinB1;

	const double fSinB2 = ::sin (_2.y*c_PI/180);
	const double fCosB2 = ::cos (_2.y*c_PI/180);
	const double fSinL2 = ::sin (_2.x*c_PI/180);
	const double fCosL2 = ::cos (_2.x*c_PI/180);

	const double N2 = c_A/::sqrt (1 - c_e2*fSinB2*fSinB2);

	const double X2 = N2*fCosB2*fCosL2;
	const double Y2 = N2*fCosB2*fSinL2;
	const double Z2 = (1 - c_e2)*N2*fSinB2;

	const double D = ::sqrt ((X1 - X2)*(X1 - X2) + (Y1 - Y2)*(Y1 - Y2) + (Z1 - Z2)*(Z1 - Z2));

	const double R = N1;
	const double D2 = 2*R*::asin (.5f*D/R);

	return D2;
}

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
	ToolBarItem3 = ToolBar1->AddTool(ID_ANCHOR, _("Anchor"), wxBitmap(wxImage(_T("images\\binoculars-32.png"))), wxNullBitmap, wxITEM_CHECK, _("Следить"), wxEmptyString);
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

	/*-
	{
	wxIcon FrameIcon;
	FrameIcon.CopyFromBitmap(wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_TIP")),wxART_FRAME_ICON));
	SetIcon(FrameIcon);
	}
	-*/

	SetClientSize(400, 400);
	Show(true);

	/* Обязательно обнуляем, а то получим ошибку, т.к. картинки начнут
		использоваться ещё до того, как мы успеем их загрузить */
	for (int i = 0; i < count_; ++i)
		images_[i]= 0;

	cartographer_ = new cart::Cartographer(
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
	delete Panel1;
	FlexGridSizer1->Add(cartographer_, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);

	/* Список карт */
	int maps_count = cartographer_->GetMapsCount();
	cart::map_info map;

	for( int i = 0; i < maps_count; ++i)
	{
		map = cartographer_->GetMapInfo(i);
		ComboBox1->Append(map.name);
	}

	map = cartographer_->GetActiveMapInfo();
	ComboBox1->SetValue(map.name);


	/* Метки - не забыть изменить размер массива (!),
		когда надо будет добавить ещё */
	images_[0] = cartographer_->LoadImageFromFile(L"images/blu-blank.png");
	cartographer_->SetImageCentralPoint(images_[0], 31.5, 64.0);

	images_[1] = cartographer_->LoadImageFromFile(L"images/Back.png");
	cartographer_->SetImageCentralPoint(images_[1], -1.0, 15.5);

	images_[2] = cartographer_->LoadImageFromFile(L"images/Forward.png");
	cartographer_->SetImageCentralPoint(images_[2], 32.0, 15.5);

	images_[3] = cartographer_->LoadImageFromFile(L"images/Up.png");
	cartographer_->SetImageCentralPoint(images_[3], 15.5, -1.0);

	images_[4] = cartographer_->LoadImageFromFile(L"images/Down.png");
	cartographer_->SetImageCentralPoint(images_[4], 15.5, 31.0);

	images_[5] = cartographer_->LoadImageFromFile(L"images/Flag.png");
	cartographer_->SetImageCentralPoint(images_[5], 3.5, 31.0);

	images_[6] = cartographer_->LoadImageFromFile(L"images/write.png");
	cartographer_->SetImageCentralPoint(images_[6], 1.0, 31.0);

	images_[7] = cartographer_->LoadImageFromFile(L"images/ylw-pushpin.png");
	cartographer_->SetImageCentralPoint(images_[7], 18.0, 63.0);
	cartographer_->SetImageScale(images_[7], 0.5, 0.5);

	images_[8] = cartographer_->LoadImageFromFile(L"images/wifi.png");
	cartographer_->SetImageCentralPoint(images_[8], 15.5, 35.0);


	/* Места для быстрого перехода */
	names_[0] = L"Хабаровск";
	z_[0] = 12;
	coords_[0] = cart::DegreesToGeo( 48,28,48.77, 135,4,19.04 );
	//coords_[0] = cart::DegreesToGeo( 55,45,15.0, 37,37,23.0 );

	names_[1] = L"Владивосток";
	z_[1] = 13;
	coords_[1] = cart::DegreesToGeo( 43,7,17.95, 131,55,34.4 );

	names_[2] = L"Магадан";
	z_[2] = 12;
	//coords_[2] = cart::DegreesToGeo( 59,33,41.79, 150,50,19.87 );
	coords_[2] = cart::DegreesToGeo( -54,-39,-28.39, -65,-7,-50.48 );

	names_[3] = L"Якутск";
	z_[3] = 10;
	coords_[3] = cart::DegreesToGeo( 62,4,30.33, 129,45,24.39 );

	names_[4] = L"Южно-Сахалинск";
	z_[4] = 12;
	coords_[4] = cart::DegreesToGeo( 46,57,34.28, 142,44,18.58 );

	names_[5] = L"Петропавловск-Камчатский";
	z_[5] = 13;
	coords_[5] = cart::DegreesToGeo( 53,4,11.14, 158,37,9.24 );

	names_[6] = L"Бикин";
	z_[6] = 11;
	coords_[6] = cart::DegreesToGeo( 46,48,47.59, 134,14,55.71 );

	names_[7] = L"Благовещенск";
	z_[7] = 14;
	coords_[7] = cart::DegreesToGeo( 50,16,55.96, 127,31,46.09 );

	names_[8] = L"Биробиджан";
	z_[8] = 14;
	coords_[8] = cart::DegreesToGeo( 48,47,52.55, 132,55,5.13 );

	for (int i = 0; i < count_; ++i)
		Choice1->Append(names_[i]);

	double kd = M_PI / 180.0;
	double B1 = coords_[0].lat * kd;
	double L1 = coords_[0].lon * kd;
	double B2 = coords_[2].lat * kd;
	double L2 = coords_[2].lon * kd;

	double s;

	{
		// 1.
		const double e2 = 0.0066934216;

		const double sin_B1 = sin(B1);
		const double cos_B1 = cos(B1);
		const double sin_B2 = sin(B2);
		const double cos_B2 = cos(B2);

		const double W1 = sqrt(1 - e2 * sin_B1 * sin_B1);
		const double W2 = sqrt(1 - e2 * sin_B2 * sin_B2);
		const double sin_mu1 = sin_B1 * sqrt(1 - e2) / W1;
		const double sin_mu2 = sin_B2 * sqrt(1 - e2) / W2;
		const double cos_mu1 = cos_B1 / W1;
		const double cos_mu2 = cos_B2 / W2;
		const double l = L2 - L1;
		const double a1 = sin_mu1 * sin_mu2;
		const double a2 = cos_mu1 * cos_mu2;
		const double b1 = cos_mu1 * sin_mu2;
		const double b2 = sin_mu1 * cos_mu2;

		// 2.
		double delta = 0.0;

		while (1) {
			double lambda = l + delta;
			double p = cos_mu2 * sin(lambda);
			double q = b1 - b2 * cos(lambda);
			double A1 = abs( atan(p / q) );
			//double A1 = atan(p / q);

			// 2.А
			if (p < 0.0)
			{
				if (q < 0.0)
					A1 = M_PI + A1;
				else
					A1 = 2 * M_PI - A1;
			}
			else if (q < 0.0)
			{
				A1 = M_PI - A1;
			}

			// 2.Б
			double sin_sigma = p * sin(A1) + q * cos(A1);
			double cos_sigma = a1 + a2 * cos(lambda);
			double sigma = abs( atan( sin_sigma / cos_sigma ) );
			//double sigma = atan( sin_sigma / cos_sigma );

			// 2.В
			if (cos_sigma < 0.0)
				sigma = M_PI - sigma;

			double sin_A0 = cos_mu1 * sin(A1);
			double cos_A0 = cos( asin(sin_A0) ); /*!*/
			double x = 2 * a1 - cos_A0 * cos_A0 * cos_sigma;
		
			double alpha = (33523299 - (28189 - 70 * cos_A0 * cos_A0) * cos_A0 * cos_A0) * 0.0000000001;
			double beta = (28189 - 94 * cos_A0 * cos_A0) * 0.0000000001;
		
			double old_delta = delta;
			delta = (alpha * sigma - beta * x * sin_sigma) * sin_A0;
			
			if (abs(old_delta - delta) < 0.0001)
			{
				// 3.
				double A = 6356863.020 + (10708.949 - 13.474 * cos_A0 * cos_A0) * cos_A0 * cos_A0;
				double B_ = 10708.938 - 17.956 * cos_A0 * cos_A0;
				double C_ = 4.487;

				double y = (cos_A0 * cos_A0 * cos_A0 * cos_A0 - 2 * x * x) * cos_sigma;
				s = A * sigma + (B_ * x + C_ * y) * sin_sigma;
				//A2 = 
				break;
			}
		}
	}

	point_t pt1( coords_[2].lon, coords_[2].lat );
	point_t pt2( coords_[0].lon, coords_[0].lat );
	float f = Distance(pt1, pt2);

	double a1, a2;
	const double accuracy_in_mm = 0.1;

	double d1 = cartographer_->Distance( coords_[2], coords_[0], &a1, &a2, accuracy_in_mm );
	double d2 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(90.0, 0.0), &a1, &a2, accuracy_in_mm );
	double d4 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(0.0, 179.0), &a1, &a2, accuracy_in_mm );
	//double d3 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(0.0, 180.0), &a1, &a2, accuracy_in_mm );
	double d5 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(0.0, 90.0), &a1, &a2, accuracy_in_mm );
	double d6 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(45.0, 90.0), &a1, &a2, accuracy_in_mm );
	double d7 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(45.0, -90.0), &a1, &a2, accuracy_in_mm );
	double d8 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(-45.0, 90.0), &a1, &a2, accuracy_in_mm );
	double d9 = cartographer_->Distance( cart::coord(0.0, 0.0), cart::coord(-45.0, -90.0), &a1, &a2, accuracy_in_mm );
	
	return;
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

void cartographerFrame::DrawImage(int id, const cart::coord &pt)
{
	if (id == 0)
		return;

	cart::point pos = cartographer_->GeoToScr(pt);
	double z = cartographer_->GetActiveZ();

	glColor4d(1.0, 1.0, 1.0, 1.0);
	cartographer_->DrawImage(id, pos, z < 6.0 ? z / 6.0 : 1.0);
}

void cartographerFrame::OnMapPaint(wxGCDC &gc, int width, int height)
{
	for (int i = 0; i < count_; ++i)
		DrawImage(images_[i], coords_[i]);

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
	int i = Choice1->GetCurrentSelection();
	cartographer_->MoveTo(z_[i], coords_[i].lat, coords_[i].lon);
}

void cartographerFrame::OnZoomInButtonClick(wxCommandEvent& event)
{
	cartographer_->ZoomIn();
}

void cartographerFrame::OnZoomOutButtonClick(wxCommandEvent& event)
{
	cartographer_->ZoomOut();
}

void cartographerFrame::OnAnchorButtonClick(wxCommandEvent& event)
{
	//AnchorButton->SetValue( !AnchorButton->GetValue() );
}
