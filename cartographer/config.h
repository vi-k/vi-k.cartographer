#ifndef CART_CONFIG_H
#define CART_CONFIG_H

/* Из-за нюансов работы под Windows этот файл обязательно должен
	быть подключен самым первым (особенно это касается работы
	с Precompiled Headers):
	1) Boost.Asio не откомпилируется, если windows.h будет подключен
	до boost/asio.hpp;
	2) WinAPI после boost/asio.hpp будет урезанным, если не установить
	константу BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN;
	3) wxWidgets подключает windows.h, соответственно он не должен
	подключаться ранее Boost.Asio */


/* Разные системы/компиляторы используют разные константы
	препроцессора для включения Unicode - синхронизируем их.
	Ради этих строчек имеет смысл подключать config.h и в других модулях,
	либо в Precompiled Headers */
#if defined(_UNICODE) || defined(UNICODE) || defined(wxUSE_UNICODE)
	#ifndef _UNICODE
		#define _UNICODE
	#endif
	#ifndef UNICODE
		#define UNICODE
	#endif
	#ifndef wxUSE_UNICODE
		#define wxUSE_UNICODE
	#endif
#endif


/* Msvc выдаёт много warning'ов на unsafe-Функции в wxWidgets. Убираем */
#include <boost/config/warning_disable.hpp>

/* Настройки под платформу (например, BOOST_WINDOWS) */
#include <boost/config.hpp>

/* Дополнительные манипуляции для Boost.Asio под Windows */
#ifdef BOOST_WINDOWS
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#endif

#include <boost/asio.hpp> /* Обязательно до включения windows.h */
#include <wx/wxprec.h> /* Обязательно после boost/asio.h */

#endif /* CART_CONFIG_H */
