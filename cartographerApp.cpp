/***************************************************************
 * Name:      cartographerApp.cpp
 * Purpose:   Code for Application Class
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-07-13
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include "stdafx.h"

#include "cartographerApp.h"

//(*AppHeaders
#include "cartographerMain.h"
#include <wx/image.h>
//*)

IMPLEMENT_APP(cartographerApp);

bool cartographerApp::OnInit()
{
    //(*AppInitialize
    bool wxsOK = true;
    wxInitAllImageHandlers();
    if ( wxsOK )
    {
    cartographerFrame* Frame = new cartographerFrame(0);
    Frame->Show();
    SetTopWindow(Frame);
    }
    //*)
    return wxsOK;

}
