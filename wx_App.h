/***************************************************************
 * Name:      wx_App.h
 * Purpose:   Defines Application Class
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#ifndef WX_APP_H
#define WX_APP_H

#include <wx/msgdlg.h>
#include <wx/app.h>

class wx_App : public wxApp
{
	public:
		virtual bool OnInit();
	    
		virtual bool OnExceptionInMainLoop();
	    virtual void OnUnhandledException();
	    virtual void OnFatalException();
};

#endif // WX_APP_H
