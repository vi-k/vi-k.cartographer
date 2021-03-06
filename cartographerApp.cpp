﻿/***************************************************************
 * Name:      cartographerApp.cpp
 * Purpose:   Code for Application Class
 * Author:    vi.k (vi.k@mail.ru)
 * Created:   2010-07-16
 * Copyright: vi.k ()
 * License:
 **************************************************************/

#include "stdafx.h"

#include "cartographerApp.h"
#include "handle_exception.h"

//(*AppHeaders
#include "cartographerMain.h"
#include <wx/image.h>
//*)

#include <mylib.h>

#include <string>
#include <exception>

my::log main_log(L"main.log", my::log::clean);

IMPLEMENT_APP(cartographerApp);

bool cartographerApp::OnInit()
{
	#if wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
	#endif

	main_log << L"OnInit()" << main_log;

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

int cartographerApp::OnExit()
{
	main_log << L"OnExit()" << main_log;
	return 0;
}

bool cartographerApp::OnExceptionInMainLoop()
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

void cartographerApp::OnUnhandledException()
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

void cartographerApp::OnFatalException()
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
