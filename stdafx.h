/* Эта часть не должна изменяться! */

#include <boost/config/warning_disable.hpp> /* против unsafe в wxWidgets */
#include <boost/config.hpp>

#undef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp> /* Сокеты, таймеры, асинхронные операции.
                             Обязательно до включения windows.h! */

#if defined(BOOST_WINDOWS)
#include <wx/msw/winundef.h>
#endif

#include <wx/setup.h> /* Обязательно самым первым среди wxWidgets! */
#include <wx/msgdlg.h>    /* А это вторым! */


/* А вот дальше - пожалуйста */
