﻿/***************************************************************
 * Name:      cartographerMain.h
 * Purpose:   Defines Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-07-16
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#ifndef CARTOGRAPHERMAIN_H
#define CARTOGRAPHERMAIN_H

#include "wxCartographer.h"

//(*Headers(cartographerFrame)
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/combobox.h>
#include <wx/statusbr.h>
//*)

class cartographerFrame: public wxFrame
{
	public:

		cartographerFrame(wxWindow* parent,wxWindowID id = -1);
		virtual ~cartographerFrame();

	private:
		cgr::Cartographer *cartographer_;
		static const int count_ = 8; //9;
		int images_[count_];
		wxString names_[count_];
		int z_[count_];
		cgr::coord coords_[count_];

		void OnMapPaint(wxGCDC &gc, int width, int height);

		void DrawImage(int id, const cgr::coord &pt);

		static wxCoord DrawTextInBox( wxGCDC &gc,
			const wxString &str, wxCoord x, wxCoord y,
			const wxFont &font, const wxColour &color,
			const wxPen &back_pen, const wxBrush &back_brush );

		//(*Handlers(cartographerFrame)
		void OnQuit(wxCommandEvent& event);
		void OnAbout(wxCommandEvent& event);
		void OnComboBox1Select(wxCommandEvent& event);
		void OnChoice1Select(wxCommandEvent& event);
		//*)

		//(*Identifiers(cartographerFrame)
		static const long ID_COMBOBOX1;
		static const long ID_CHOICE1;
		static const long ID_PANEL2;
		static const long ID_PANEL1;
		static const long ID_MENU_QUIT;
		static const long ID_MENU_ABOUT;
		static const long ID_STATUSBAR1;
		//*)

		//(*Declarations(wx_MainFrame)
		wxStatusBar* StatusBar1;
		wxComboBox* ComboBox1;
		wxPanel* Panel2;
		wxPanel* Panel1;
		wxFlexGridSizer* FlexGridSizer1;
		wxChoice* Choice1;
		//*)

		DECLARE_EVENT_TABLE()
};

#endif // CARTOGRAPHERMAIN_H
