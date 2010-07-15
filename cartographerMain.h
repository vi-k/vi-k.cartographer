﻿/***************************************************************
 * Name:      wx_Main.h
 * Purpose:   Defines Application Frame
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#ifndef WX_MAIN_H
#define WX_MAIN_H

#include "wxCartographer.h"
#include <wx/dcgraph.h> /* wxGCDC */

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
		cartographerFrame(wxWindow* parent, wxWindowID id = -1);
		virtual ~cartographerFrame();

	private:
		typedef std::vector<wxCartographer::map> maps_list;

		wxCartographer *cartographer_;
		maps_list maps_;
		wxBitmap bitmap_;

		void OnMapPaint(wxGCDC &gc, wxCoord width, wxCoord height);

		static wxCoord DrawTextInBox( wxGCDC &gc,
			const wxString &str, wxCoord x, wxCoord y,
			const wxFont &font, const wxColour &color,
			const wxPen &back_pen, const wxBrush &back_brush );

		//(*Handlers(wx_MainFrame)
		void OnQuit(wxCommandEvent& event);
		void OnAbout(wxCommandEvent& event);
		void OnComboBox1Select(wxCommandEvent& event);
		void OnChoice1Select(wxCommandEvent& event);
		//*)

		//(*Identifiers(wx_MainFrame)
		static const long ID_COMBOBOX1;
		static const long ID_CHOICE1;
		static const long ID_PANEL1;
		static const long ID_PANEL2;
		static const long ID_MENU_QUIT;
		static const long ID_MENU_ABOUT;
		static const long ID_STATUSBAR1;
		//*)

		//(*Declarations(wx_MainFrame)
		wxPanel* Panel1;
		wxStatusBar* StatusBar1;
		wxComboBox* ComboBox1;
		wxPanel* Panel2;
		wxFlexGridSizer* FlexGridSizer1;
		wxChoice* Choice1;
		//*)

		DECLARE_EVENT_TABLE()
};

#endif // CARTOGRAPHERMAIN_H