/***************************************************************
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
#include <fstream>
#include <exception>

std::wofstream main_log_stream;
void on_main_log(const std::wstring &text)
{
	main_log_stream << my::time::to_wstring(
		my::time::utc_now(), L"[%Y-%m-%d %H:%M:%S]\n")
		<< text << L"\n\n";
	main_log_stream.flush();
}
my::log main_log(on_main_log);

IMPLEMENT_APP(cartographerApp);

bool cartographerApp::OnInit()
{
	#if wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
	#endif

	/* Открываем лог */
	bool log_exists = fs::exists("main.log");

	main_log_stream.open("main.log", std::ios::app);

	if (log_exists)
        main_log_stream << std::endl;
    else
	{
	    std::ofstream fs("main.log");
	    fs << "\xEF\xBB\xBF";
	    fs.close();
	    main_log_stream.open("main.log", std::ios::app);
	}

	main_log_stream.imbue( std::locale( main_log_stream.getloc(),
		new boost::archive::detail::utf8_codecvt_facet) );

	main_log << L"Start" << main_log;


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
	main_log << L"Finish" << main_log;
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
