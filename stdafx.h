/* ��� ����� �� ������ ����������! */

#include <boost/config/warning_disable.hpp> /* ������ unsafe � wxWidgets */

#undef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp> /* ������, �������, ����������� ��������.
                             ����������� �� ��������� windows.h! */

#include <wx/msw/winundef.h>
#include <wx/platform.h> /* ����������� ����� ������ ����� wxWidgets! */
#include <wx/msgdlg.h>    /* � ��� ������! */


/* � ��� ������ - ���������� */
