/* ��� ����� �� ������ ����������! */

#include <boost/config/warning_disable.hpp> /* ������ unsafe � wxWidgets */
#include <boost/config.hpp>

#undef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp> /* ������, �������, ����������� ��������.
                             ����������� �� ��������� windows.h! */

#if defined(BOOST_WINDOWS)
#include <wx/msw/winundef.h>
#endif

#include <wx/setup.h> /* ����������� ����� ������ ����� wxWidgets! */
#include <wx/msgdlg.h>    /* � ��� ������! */


/* � ��� ������ - ���������� */
