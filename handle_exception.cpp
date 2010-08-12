#include "stdafx.h"

#include "handle_exception.h"

#include <mylib.h>

#include <string>
#include <sstream>

#include <wx/msgdlg.h>


extern my::log main_log;

void handle_exception(
	std::exception *e,
	const std::wstring &add_to_log,
	const std::wstring &window_title)
{
	std::wstring log_title;
    std::wstring error;
	std::wstringstream log;

    if (!e)
    {
    	log_title = L"unknown exception";
    	error = L"Неизвестное исключение";
    }
    else
    {
	    my::exception *my_e_ptr = dynamic_cast<my::exception*>(e);

	    if (my_e_ptr)
    	{
    		log_title = L"my::exception";
	    	error = my_e_ptr->message();
    	}
		else
		{
    		log_title = L"std::exception";
			my::exception my_e(*e);
			error = my_e.message();
		}
	}

	log << L"-- " << log_title;

	if (!add_to_log.empty())
		log << ' ' << add_to_log;

	log << L" --\n" << error;

	/* Запись ошибки в лог */
	main_log << log.str() << main_log;

	if (!window_title.empty())
		wxMessageBox(log.str(), window_title, wxOK | wxICON_ERROR);
}
