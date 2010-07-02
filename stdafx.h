/* Эта часть не должна изменяться! */

#include <boost/config/warning_disable.hpp> /* против unsafe в wxWidgets */

#undef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp> /* Сокеты, таймеры, асинхронные операции.
                             Обязательно до включения windows.h! */

#include <wx/msw/setup.h> /* Обязательно самым первым среди wxWidgets! */
#include <wx/msgdlg.h>    /* А это вторым! */


/* А вот дальше - пожалуйста */
