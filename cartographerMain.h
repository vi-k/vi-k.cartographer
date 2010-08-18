/***************************************************************
 * Name:      cartographerMain.h
 * Purpose:   Defines Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-07-16
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#ifndef CARTOGRAPHERMAIN_H
#define CARTOGRAPHERMAIN_H

#include "cartographer/Painter.h"

//(*Headers(cartographerFrame)
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/toolbar.h>
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/combobox.h>
#include <wx/statusbr.h>
//*)

#include <wx/tglbtn.h>

class cartographerFrame: public wxFrame
{
	public:

		cartographerFrame(wxWindow* parent,wxWindowID id = -1);
		virtual ~cartographerFrame();

	private:
		cartographer::Painter *Cartographer;

		/* Шрифты */
		int big_font_;
		int small_font_;

		/* Изображения */
		int images_[11];
		int green_mark16_id_;
		int red_mark16_id_;
		int yellow_mark16_id_;

		/* "Быстрые" точки */
		static const int count_ = 9;
		wxString names_[count_];
		int z_[count_];
		cartographer::coord coords_[count_];

		void Test(); /* Тестирование функций Картографера */

		void OnMapPaint(double z, const cartographer::size &screen_size);

		void DrawImage(int id, const cartographer::coord &pt, double alpha = 1.0);

		void DrawCircle(const cartographer::coord &pt,
			double r, double line_width, const cartographer::color &line_color,
			const cartographer::color &fill_color);

		double DrawPath(const cartographer::coord &pt1,
			const cartographer::coord &pt2,
			double line_width, const cartographer::color &line_color,
			double *p_azimuth = NULL, double *p_rev_azimuth = NULL);

		cartographer::coord DrawPath(const cartographer::coord &pt,
			double azimuth, double distance,
			double line_width, const cartographer::color &line_color,
			double *p_rev_azimuth = NULL);

		//(*Handlers(cartographerFrame)
		void OnQuit(wxCommandEvent& event);
		void OnAbout(wxCommandEvent& event);
		void OnComboBox1Select(wxCommandEvent& event);
		void OnChoice1Select(wxCommandEvent& event);
		void OnZoomInButtonClick(wxCommandEvent& event);
		void OnZoomOutButtonClick(wxCommandEvent& event);
		void OnAnchorButtonClick(wxCommandEvent& event);
		//*)

		//(*Identifiers(cartographerFrame)
		static const long ID_COMBOBOX1;
		static const long ID_CHOICE1;
		static const long ID_PANEL2;
		static const long ID_PANEL1;
		static const long ID_MENU_QUIT;
		static const long ID_MENU_ABOUT;
		static const long ID_STATUSBAR1;
		static const long ID_ZOOMIN;
		static const long ID_ZOOMOUT;
		static const long ID_ANCHOR;
		static const long ID_TOOLBAR1;
		//*)

		//(*Declarations(cartographerFrame)
		wxToolBar* ToolBar1;
		wxToolBarToolBase* ToolBarItem3;
		wxPanel* Panel1;
		wxToolBarToolBase* ToolBarItem1;
		wxStatusBar* StatusBar1;
		wxComboBox* ComboBox1;
		wxPanel* Panel2;
		wxFlexGridSizer* FlexGridSizer1;
		wxChoice* Choice1;
		wxToolBarToolBase* ToolBarItem2;
		//*)

		DECLARE_EVENT_TABLE()
};

#endif // CARTOGRAPHERMAIN_H
