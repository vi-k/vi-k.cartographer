/***************************************************************
 * Name:	  wx_App.cpp
 * Purpose:   Code for Application Class
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-03-30
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include "stdafx.h"
#include "wx_App.h"
#include "handle_exception.h"

//(*AppHeaders
#include "wx_Main.h"
#include <wx/image.h>
//*)

#include <exception>

IMPLEMENT_APP(wx_App);

bool wx_App::OnInit()
{
	wxHandleFatalExceptions(true);

	//(*AppInitialize
	bool wxsOK = true;
	wxInitAllImageHandlers();
	if ( wxsOK )
	{
	wx_MainFrame* Frame = new wx_MainFrame(0);
	Frame->Show();
	SetTopWindow(Frame);
	}
	//*)
	return wxsOK;

}

bool wx_App::OnExceptionInMainLoop()
{
	try
	{
		throw;
	}
	catch (std::exception &e)
	{
		handle_exception(&e, L"in App::OnExceptionInMainLoop", L"Ошибка");
	}
	catch (...)
	{
		handle_exception(0, L"in App::OnExceptionInMainLoop", L"Ошибка");
	}

	return true;
}

void wx_App::OnUnhandledException()
{
	try
	{
		throw;
	}
	catch (std::exception &e)
	{
		handle_exception(&e, L"in App::OnUnhandledException", L"Ошибка");
	}
	catch (...)
	{
		handle_exception(0, L"in App::OnUnhandledException", L"Ошибка");
	}
}

void wx_App::OnFatalException()
{
	try
	{
		throw;
	}
	catch (std::exception &e)
	{
		handle_exception(&e, L"in App::OnFatalException", L"Критическая ошибка");
	}
	catch (...)
	{
		handle_exception(0, L"in App::OnFatalException", L"Критическая ошибка");
	}
}
