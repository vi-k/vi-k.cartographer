/***************************************************************
 * Name:      cartographerApp.h
 * Purpose:   Defines Application Class
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-07-16
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#ifndef CARTOGRAPHERAPP_H
#define CARTOGRAPHERAPP_H

#include <wx/app.h>

class cartographerApp : public wxApp
{
    public:
        virtual bool OnInit();
        virtual int OnExit();
		virtual bool OnExceptionInMainLoop();
	    virtual void OnUnhandledException();
	    virtual void OnFatalException();
};

#endif // CARTOGRAPHERAPP_H
