#include "wxCartographer.h"

/* windows */
#ifdef _WINDOWS
#include <minmax.h> /* Нужен для gdiplus.h */
#include <windows.h>
#undef GDIPVER
#define GDIPVER 0x0110
#include <gdiplus.h>
#include <windowsx.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#endif /* _WINDOWS */

#include <my_exception.h>
#include <my_ptr.h>
#include <my_utf8.h>
#include <my_time.h>
#include <my_xml.h>
#include <my_fs.h> /* boost::filesystem */

#include <math.h> /* sin, sqrt */
#include <wchar.h> /* swprintf_s */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

#include <wx/dcclient.h> /* wxPaintDC */
#include <wx/dcmemory.h> /* wxMemoryDC */
#include <wx/rawbmp.h> /* wxNativePixelData */

#ifndef NDEBUG
#include <my_log.h>
mutex lock_mutex;
std::wofstream lock_log_stream;
void on_lock_log(const std::wstring &text)
{
	lock_log_stream << boost::this_thread::get_id()
		<< L": " << text << std::endl << std::flush;
}
my::log lock_log(on_lock_log);
#define LOCK_LOG(text) {\
	mutex::scoped_lock l(lock_mutex);\
	lock_log << text << lock_log; }
#else
#define LOCK_LOG(text)
#endif

#define EXCT 0.081819790992 /* эксцентриситет эллипса */

template<typename Real>
Real atanh(Real x)
{
	return 0.5 * log( (1.0 + x) / (1.0 - x) );
}

BEGIN_EVENT_TABLE(wxCartographer, wxPanel)
    EVT_PAINT(wxCartographer::on_paint)
    EVT_ERASE_BACKGROUND(wxCartographer::on_erase_background)
    EVT_SIZE( wxCartographer::on_size)
    //EVT_SCROLL( wxCartographer::OnScroll)
    EVT_LEFT_DOWN( wxCartographer::on_left_down)
    EVT_LEFT_UP( wxCartographer::on_left_up)
    EVT_MOUSE_CAPTURE_LOST(wxCartographer::on_capture_lost)
    EVT_MOTION( wxCartographer::on_mouse_move)
    EVT_MOUSEWHEEL( wxCartographer::on_mouse_wheel)
    //EVT_KEY_DOWN( wxCartographer::OnKeyDown)
END_EVENT_TABLE()

wxCartographer::wxCartographer( const std::wstring &serverAddr,
	const std::wstring &serverPort, std::size_t cacheSize,
	std::wstring cachePath, bool onlyCache,
	const std::wstring &initMap, int initZ, wxDouble initLat, wxDouble initLon,
	OnPaintProc_t onPaintProc,
	wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size,
	int animPeriod, int defAnimSteps )
	: wxPanel(parent, id, pos, size, 0)
	, cache_(cacheSize)
	, cache_path_( fs::system_complete(cachePath).string() )
	, only_cache_(onlyCache)
	, builder_debug_counter_(0)
	, painter_debug_counter_(0)
	, animator_debug_counter_(0)
	, draw_tile_debug_dounter_(0)
	, file_loader_debug_counter_(0)
	, server_loader_debug_counter_(0)
	, file_queue_(100)
	, server_queue_(100)
	, buffer_(100,100)
	, z_(initZ)
	, fix_kx_(0.5)
	, fix_ky_(0.5)
	, fix_lat_(initLat)
	, fix_lon_(initLon)
	, active_map_id_(0)
	, on_paint_(onPaintProc)
	, anim_period_( posix_time::milliseconds(animPeriod) )
	, def_anim_steps_(defAnimSteps)
	, anim_speed_(0)
	, anim_freq_(0)
{
	#ifndef NDEBUG
	bool log_exists = fs::exists(L"lock.log");
	lock_log_stream.open("lock.log", std::ios::app);
	if (!log_exists)
		lock_log_stream << L"\xEF\xBB\xBF";
	else
		lock_log_stream << std::endl;

	lock_log_stream.imbue( std::locale( lock_log_stream.getloc(),
		new boost::archive::detail::utf8_codecvt_facet) );
	#endif

	try
	{
		std::wstring request;
		std::wstring file;

		/* Загружаем с сервера список доступных карт */
		try
		{
			request = L"/maps/maps.xml";
			file = cache_path_ + L"/maps.xml";

			/* Загружаем с сервера на диск (кэшируем) */
			if (!only_cache_)
			{
				/* Резолвим сервер */
				asio::ip::tcp::resolver resolver(io_service_);
				asio::ip::tcp::resolver::query query(
					my::ip::punycode_encode(serverAddr),
					my::ip::punycode_encode(serverPort));
				server_endpoint_ = *resolver.resolve(query);

				load_and_save_xml(request, file);
			}

			/* Загружаем с диска */
			xml::wptree config;
			my::xml::load(file, config);

			/* В 'p' - список всех значений maps\map */
			std::pair<xml::wptree::assoc_iterator, xml::wptree::assoc_iterator>
				p = config.get_child(L"maps").equal_range(L"map");

			while (p.first != p.second)
			{
				wxCartographer::map map;
				map.sid = p.first->second.get<std::wstring>(L"id");
				map.name = p.first->second.get<std::wstring>(L"name", L"");
				map.is_layer = p.first->second.get<bool>(L"layer", 0);

				map.tile_type = p.first->second.get<std::wstring>(L"tile-type");
				if (map.tile_type == L"image/jpeg")
					map.ext = L"jpg";
				else if (map.tile_type == L"image/png")
					map.ext = L"png";
				else
					throw my::exception(L"Неизвестный тип тайла")
						<< my::param(L"map", map.sid)
						<< my::param(L"tile-type", map.tile_type);

				std::wstring projection
					= p.first->second.get<std::wstring>(L"projection");

				if (projection == L"spheroid") /* Google */
					map.projection = wxCartographer::map::spheroid;
				else if (projection == L"ellipsoid") /* Yandex */
					map.projection = wxCartographer::map::ellipsoid;
				else
					throw my::exception(L"Неизвестный тип проекции")
						<< my::param(L"map", map.sid)
						<< my::param(L"projection", projection);

				/* Сохраняем описание карты в списке */
				int id = get_new_map_id(); /* новый идентификатор */
				maps_[id] = map;

				if (active_map_id_ == 0 || map.name == initMap)
					active_map_id_ = id;

				/* Сохраняем соответствие названия
					карты числовому идентификатору */
				maps_name_to_id_[map.name] = id;

				p.first++;
			}
		}
		catch(std::exception &e)
		{
			throw my::exception(L"Ошибка загрузки списка карт для Картографа")
				<< my::param(L"request", request)
				<< my::param(L"file", file)
				<< my::exception(e);
		} /* Загружаем с сервера список доступных карт */

		/* Запускаем файловый загрузчик тайлов */
		file_loader_ = new_worker("file_loader"); /* Название - только для отладки */
		boost::thread( boost::bind(
			&wxCartographer::file_loader_proc, this, file_loader_) );

		/* Запускаем серверный загрузчик тайлов */
		if (!only_cache_)
		{
			server_loader_ = new_worker("server_loader"); /* Название - только для отладки */
			boost::thread( boost::bind(
				&wxCartographer::server_loader_proc, this, server_loader_) );
		}

		/* Запускаем анимацию */
		if (animPeriod)
		{
			/* Чтобы при расчёте средних скорости и частоты анимации данные
				не скакали слишком быстро, будем хранить 10 последних расчётов
				и вычислять общее среднее */
			for (int i = 0; i < 10; i++)
			{
				anim_speed_sw_.push();
				anim_freq_sw_.push();
			}
			animator_ = new_worker("animator");
			boost::thread( boost::bind(
				&wxCartographer::anim_thread_proc, this, animator_) );
		}
	}
	catch(std::exception &e)
	{
		throw my::exception(L"Ошибка создания Картографа")
			<< my::param(L"serverAddr", serverAddr)
			<< my::param(L"serverPort", serverPort)
			<< my::exception(e);
	}

	Refresh(false);
}

wxCartographer::~wxCartographer()
{
	if (!finish())
		Stop();
}

void wxCartographer::Stop()
{
	/* Как обычно, самое весёлое занятие - это
		умудриться остановить всю эту махину */

	/* Оповещаем о завершении работы */
	lets_finish();

	/* Освобождаем ("увольняем") всех "работников" */
	dismiss(file_loader_);
	dismiss(server_loader_);
	dismiss(animator_);

    /* Ждём завершения */


	#ifndef NDEBUG
	/* Отладка - поиск зависших */
	{
		posix_time::ptime start_finish = my::time::utc_now();
		while (!check_for_finish())
		{
			std::vector<std::string> v;
			workers_state(v);
			assert( my::time::utc_now() - start_finish < posix_time::seconds(2) );
		}
	}
    #endif

	wait_for_finish();
}

void wxCartographer::add_to_cache(const tile::id &tile_id, tile::ptr ptr)
{
	{
		LOCK_LOG(L"unique cache_mutex - add_to_cache");
		unique_lock<shared_mutex> l(cache_mutex_);

		/* Ищем тайл в кэше */
		tile::ptr old = cache_[tile_id];

		bool need_for_load = ptr->need_for_load();
		bool need_for_build = ptr->need_for_build();

		if (old)
		{
			need_for_load &= old->need_for_load();
			need_for_build &= old->need_for_build();

			/* Если новый файл "лучше" старого - меняем содержимое */
			if (ptr->level() > old->level())
				ptr = old;

			/* Объединяем флаги */
			if (!need_for_load)
				ptr->reset_need_for_load();

			if (!need_for_build)
				ptr->reset_need_for_build();
		}

		cache_[tile_id] = ptr;
	}

	Refresh(false);
}

bool wxCartographer::need_for_load(tile::ptr ptr)
{
	return !ptr || ptr->need_for_load();
}

bool wxCartographer::need_for_build(tile::ptr ptr)
{
	return !ptr || ptr->need_for_build();
}

bool wxCartographer::check_tile_id(const tile::id &tile_id)
{
	int sz = size_for_int_z(tile_id.z);

	return tile_id.z >= 1
		&& tile_id.x >= 0 && tile_id.x < sz
		&& tile_id.y >= 0 && tile_id.y < sz;
}

wxCartographer::tile::ptr wxCartographer::find_tile(const tile::id &tile_id)
{
	/* Блокируем кэш для чтения */
	LOCK_LOG(L"shared cache_mutex - find_tile()");
	shared_lock<shared_mutex> l(cache_mutex_);

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

bool wxCartographer::tile_in_queue(const tiles_queue &queue,
	my::worker::ptr worker, const tile::id &tile_id)
{
	if (worker)
	{
		LOCK_LOG(L"unique worker->mutex - tile_in_queue()");
		unique_lock<mutex> lock = worker->create_lock();
		tiles_queue::const_iterator iter = queue.find(tile_id);
		return iter != queue.end();
	}

	return false;
}

wxCartographer::tile::ptr wxCartographer::get_tile(const tile::id &tile_id)
{
	/*
		Строим тайл на основании его "предков" (тайлов меньшего масштаба)

		Причина:
			При отсутствии тайла необходимо отобразить увеличенную часть
			его "предка". Это логично! Но операция StretchBlit дорогая,
			поэтому принято решение не рисовать каждый раз напрямую, а создавать
			с помощью неё новый тайл и сохранять его в кэше. Но, при этом,
			не препятствовать загрузчикам загрузить настоящий тайл
	*/

	if ( !check_tile_id(tile_id))
		return tile::ptr();

	/* Ищем, что уже имеем в кэше */
	tile::ptr tile_ptr = find_tile(tile_id);

	/* Добавляем в очередь на загрузку, если это необходимо */
	if ( need_for_load(tile_ptr) )
		add_to_file_queue(tile_id);

	/* Если тайл не нуждается в построении, пропускаем */
	if ( !need_for_build(tile_ptr) )
		return tile_ptr;

	++builder_debug_counter_;

	/* Строим предка */
	tile::ptr parent_ptr = get_tile( tile::id(
		tile_id.map_id, tile_id.z - 1,
		tile_id.x >> 1, tile_id.y >> 1) );

	/* Если не удалось построить предка, возвращаемся с тем, что есть */
	if (!parent_ptr)
		return tile_ptr;

	/* Если наш тайл уже построен от предка */
	if (tile_ptr && tile_ptr->level() - 1 == parent_ptr->level())
	{
		/* Если предок уже "готов", то и наш тайл
			уже перестраиваться не будет, но может быть загружен */
		if (parent_ptr->ready())
			tile_ptr->reset_need_for_build();

		return tile_ptr;
	}

	/* Строим тайл */
	tile_ptr.reset( new tile(parent_ptr->level() + 1) );

	/* Если предок уже "готов", то и наш тайл
		уже перестраиваться не будет, но может быть загружен */
	if (parent_ptr->ready())
		tile_ptr->reset_need_for_build();

	wxMemoryDC tile_dc( tile_ptr->bitmap() );
	wxMemoryDC parent_dc( parent_ptr->bitmap() );

	tile_dc.StretchBlit( 0, 0, 256, 256, &parent_dc,
		(tile_id.x & 1) * 128, (tile_id.y & 1) * 128, 128, 128 );

	/* Рамка вокруг построенного тайла */
	tile_dc.SetPen(*wxWHITE_PEN);
	tile_dc.SetBrush(*wxTRANSPARENT_BRUSH);
	tile_dc.DrawRectangle(0, 0, 256, 256);

	add_to_cache(tile_id, tile_ptr);

	return tile_ptr;
}

void wxCartographer::add_to_file_queue(const tile::id &tile_id)
{
	/* Не добавляем тайл в файловую очередь,
		если он уже стоит в серверной очереди */
	if ( !tile_in_queue(server_queue_, server_loader_, tile_id))
	{
		/* Копированием указателя на "работника" гарантируем,
			что он не будет удалён, пока выполняется функция */
		my::worker::ptr worker = file_loader_;
		if (worker)
		{
			LOCK_LOG(L"unique worker->mutex - add_to_file_queue()");
			unique_lock<mutex> lock = worker->create_lock();
			file_queue_[tile_id] = 0; /* Не важно значение, важно само присутствие */
			wake_up(worker, lock); /* Будим работника, если он спит */
		}
	}
}

void wxCartographer::add_to_server_queue(const tile::id &tile_id)
{
	if (!only_cache_)
	{
		/* Копированием указателя на "работника" гарантируем,
			что он не будет удалён, пока выполняется функция */
		my::worker::ptr worker = server_loader_;
		if (worker)
		{
			LOCK_LOG(L"unique worker->mutex - add_to_server_queue()");
			unique_lock<mutex> lock = worker->create_lock();
			server_queue_[tile_id] = 0; /* Не важно значение, важно само присутствие */
			wake_up(worker, lock); /* Будим работника, если он спит */
		}
	}
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void wxCartographer::file_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile::id tile_id;

		++file_loader_debug_counter_;

		/* Берём идентификатор первого тайла из очереди */
		{
			/* Блокировкой гарантируем, что очередь не изменится */
			LOCK_LOG(L"unique worker->mutex - file_loader_proc()");
			unique_lock<mutex> lock = this_worker->create_lock();

			tiles_queue::iterator iter = file_queue_.begin();

			/* Если очередь пуста - засыпаем */
			if (iter == file_queue_.end())
			{
				sleep(this_worker, lock);
				continue;
			}

			tile_id = iter->key();

			/* Для дальнейших действий блокировка нам не нужна */
		}

		/* Если тайл не нуждается  в загрузке, пропускаем */
		//if ( !need_for_load( find_tile(tile_id) ) )
		//	continue;

		/* Загружаем тайл с диска */
		std::wstringstream tile_path;

		wxCartographer::map &map = maps_[tile_id.map_id];

		tile_path << cache_path_
			<< L"/" << map.sid
			<< L"/z" << tile_id.z
			<< L'/' << (tile_id.x >> 10)
			<< L"/x" << tile_id.x
			<< L'/' << (tile_id.y >> 10)
			<< L"/y" << tile_id.y << L'.' << map.ext;

		tile::ptr ptr( new tile(tile_path.str()) );

		if (ptr->ok())
		    /* При успехе операции - сохраняем тайл в кэше */
			add_to_cache(tile_id, ptr);
		else
			/* Иначе - загружаем с сервера */
			add_to_server_queue(tile_id);

		/* Удаляем тайл из очереди */
		{
			LOCK_LOG(L"unique worker->mutex - file_loader_proc()");
			unique_lock<mutex> lock = this_worker->create_lock();
			file_queue_.remove(tile_id);
		}

	} /* while (!finish()) */
}

/* Загрузчик тайлов с сервера. При пустой очереди - засыпает */
void wxCartographer::server_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile::id tile_id;

		++server_loader_debug_counter_;

		/* Берём идентификатор первого тайла из очереди */
		{
			/* Блокировкой гарантируем, что очередь не изменится */
			LOCK_LOG(L"unique worker->mutex - server_loader_proc()");
			unique_lock<mutex> lock = this_worker->create_lock();

			tiles_queue::iterator iter = server_queue_.begin();

			/* Если очередь пуста - засыпаем */
			if (iter == server_queue_.end())
			{
				sleep(this_worker, lock);
				continue;
			}

			tile_id = iter->key();

			/* Для дальнейших действий блокировка нам не нужна */
		}

		/* Если тайл уже загружен, пропускаем */
		//if ( !need_for_load( find_tile(tile_id) ) )
		//	continue;

		/* Загружаем тайл с сервера */
		std::wstringstream tile_path; /* Путь к локальному файлу  */

		wxCartographer::map &map = maps_[tile_id.map_id];

		tile_path << cache_path_
			<< L"/" << map.sid
			<< L"/z" << tile_id.z
			<< L'/' << (tile_id.x >> 10)
			<< L"/x" << tile_id.x
			<< L'/' << (tile_id.y >> 10)
			<< L"/y" << tile_id.y << L'.' << map.ext;

		std::wstringstream request;
		request << L"/maps/gettile?map=" << map.sid
			<< L"&z=" << tile_id.z
			<< L"&x=" << tile_id.x
			<< L"&y=" << tile_id.y;

		try
		{
			/* Загружаем тайл с сервера ... */
			my::http::reply reply;
			get(reply, request.str());

			tile::ptr ptr;

			if (reply.status_code != 200)
			{
				/* Если тайл отсутствует на сервере, создаём "пустой" тайл,
					чтобы не пытаться загружать повторно */
				ptr.reset( new tile() );
			}
			else
			{
				/* При успешной загрузке с сервера, создаём тайл из буфера */
				ptr.reset( new tile(reply.body.c_str(), reply.body.size()) );
				/* ... и сохраняем на диске */
				reply.save(tile_path.str());
			}

			add_to_cache(tile_id, ptr);

			/* Удаляем тайл из очереди */
			{
				LOCK_LOG(L"unique worker->mutex - server_loader_proc()");
				unique_lock<mutex> lock = this_worker->create_lock();
				server_queue_.remove(tile_id);
			}

		}
		catch (...)
		{
			/* Игнорируем любые ошибки связи */
		}

	} /* while (!finish()) */
}

void wxCartographer::anim_thread_proc(my::worker::ptr this_worker)
{
	asio::io_service io_service;
	asio::deadline_timer timer(io_service, my::time::utc_now());

	while (!finish())
	{
		++animator_debug_counter_;

		anim_speed_sw_.start();

#if 0
		/* Мигание для "мигающих" объектов */
		flash_alpha_ += (flash_new_alpha_ - flash_alpha_) / flash_step_;
		if (--flash_step_ == 0)
		{
			flash_step_ = def_anim_steps_;
			/* При выходе из паузы, меняем направление мигания */
			if ((flash_pause_ = !flash_pause_) == false)
				flash_new_alpha_ = (flash_new_alpha_ == 0 ? 1 : 0);
		}
#endif

		Refresh(false);

		anim_speed_sw_.finish();
		anim_freq_sw_.finish();

		if (anim_freq_sw_.total().total_milliseconds() >= 500)
		{
			anim_speed_sw_.push();
			anim_speed_sw_.pop_back();

			anim_freq_sw_.push();
			anim_freq_sw_.pop_back();

			anim_speed_ = my::time::div(
				anim_speed_sw_.full_avg(), posix_time::milliseconds(1) );
			anim_freq_ = my::time::div(
				anim_freq_sw_.full_avg(), posix_time::milliseconds(1) );
		}

		anim_freq_sw_.start();

		boost::posix_time::ptime time = timer.expires_at() + anim_period_;
		boost::posix_time::ptime now = my::time::utc_now();

		/* Теоретически время следующей прорисовки должно быть относительным
			от времени предыдущей, но на практике могут возникнуть торможения,
			и, тогда, программа будет пытаться запустить прорисовку в прошлом.
			В этом случае следующий запуск делаем относительно текущего времени */
		timer.expires_at( now > time ? now : time );
		timer.wait();
	}
}

void wxCartographer::get(my::http::reply &reply,
	const std::wstring &request)
{
	asio::ip::tcp::socket socket(io_service_);
	socket.connect(server_endpoint_);

	std::string full_request
		= "GET "
		+ my::http::percent_encode(my::utf8::encode(request))
		+ " HTTP/1.1\r\n\r\n";

	reply.get(socket, full_request);
}

unsigned int wxCartographer::load_and_save(const std::wstring &request,
	const std::wstring &local_filename)
{
	my::http::reply reply;

	get(reply, request);

	if (reply.status_code == 200)
		reply.save(local_filename);

	return reply.status_code;
}

unsigned int wxCartographer::load_and_save_xml(const std::wstring &request,
	const std::wstring &local_filename)
{
	my::http::reply reply;
	get(reply, request);

	if (reply.status_code == 200)
	{
		/* Т.к. xml-файл выдаётся сервером в "неоформленном"
			виде, приводим его в порядок перед сохранением */
		xml::wptree pt;
		reply.to_xml(pt);

		std::wstringstream out;
		xml::xml_writer_settings<wchar_t> xs(L' ', 4, L"utf-8");

		xml::write_xml(out, pt, xs);

		/* При сохранении конвертируем в utf-8 */
		reply.body = "\xEF\xBB\xBF" + my::utf8::encode(out.str());
		reply.save(local_filename);
	}

	return reply.status_code;
}

wxDouble wxCartographer::size_for_z(wxDouble z)
{
	/* Размер всей карты в тайлах.
		Для дробного z - чуть посложнее, чем для целого */
	int iz = (int)z;
	return (wxDouble)(1 << (iz - 1)) * (1.0 + z - iz);
}

wxDouble wxCartographer::lon_to_tile_x(wxDouble lon, wxDouble z)
{
	return (lon + 180.0) * size_for_z(z) / 360.0;
}

wxDouble wxCartographer::lat_to_tile_y(wxDouble lat, wxDouble z,
	map::projection_t projection)
{
	wxDouble s = std::sin( lat / 180.0 * M_PI );
	wxDouble y;

	switch (projection)
	{
		case map::spheroid:
			y = (0.5 - atanh(s) / (2*M_PI)) * size_for_z(z);
			break;

		case map::ellipsoid:
			y = (0.5 - (atanh(s) - EXCT*atanh(EXCT*s)) / (2*M_PI)) * size_for_z(z);
			break;

		default:
			assert(projection == map::spheroid || projection == map::ellipsoid);
	}

	return y;
}

wxDouble wxCartographer::lon_to_scr_x(wxDouble lon, wxDouble z,
	wxDouble fix_lon, wxDouble fix_scr_x)
{
	wxDouble fix_tile_x = lon_to_tile_x(fix_lon, z);
	wxDouble tile_x = lon_to_tile_x(lon, z);
	return (tile_x - fix_tile_x) * 256.0 + fix_scr_x;
}

wxDouble wxCartographer::lat_to_scr_y(wxDouble lat, wxDouble z,
	map::projection_t projection, wxDouble fix_lat, wxDouble fix_scr_y)
{
	wxDouble fix_tile_y = lat_to_tile_y(fix_lat, z, projection);
	wxDouble tile_y = lat_to_tile_y(lat, z, projection);
	return (tile_y - fix_tile_y) * 256.0 + fix_scr_y;
}

wxDouble wxCartographer::tile_x_to_lon(wxDouble x, wxDouble z)
{
	return x / size_for_z(z) * 360.0 - 180.0;
}

wxDouble wxCartographer::tile_y_to_lat(wxDouble y, wxDouble z,
	map::projection_t projection)
{
	wxDouble lat;
	wxDouble sz = size_for_z(z);
	wxDouble tmp = atan( exp( (0.5 - y / sz) * (2 * M_PI) ) );

	switch (projection)
	{
		case map::spheroid:
			lat = tmp * 360.0 / M_PI - 90.0;
			break;

		case map::ellipsoid:
		{
			tmp = tmp * 2.0 - M_PI / 2.0;
			wxDouble yy = y - sz / 2.0;
			wxDouble tmp2;
			do
			{
				tmp2 = tmp;
				tmp = asin(1.0 - ((1.0 + sin(tmp))*pow(1.0-EXCT*sin(tmp),EXCT)) / (exp((2.0*yy)/-(sz/(2.0*M_PI)))*pow(1.0+EXCT*sin(tmp),EXCT)) );

			} while( abs(tmp - tmp2) > 0.00000001 );

			lat = tmp * 180.0 / M_PI;
		}
		break;

		default:
			assert(projection == map::spheroid || projection == map::ellipsoid);
	}

	return lat;
}

wxDouble wxCartographer::scr_x_to_lon(wxDouble x, wxDouble z,
	wxDouble fix_lon, wxDouble fix_scr_x)
{
	wxDouble fix_tile_x = lon_to_tile_x(fix_lon, z);
	return tile_x_to_lon( fix_tile_x + (x - fix_scr_x) / 256.0, z );
}

wxDouble wxCartographer::scr_y_to_lat(wxDouble y, wxDouble z,
	map::projection_t projection, wxDouble fix_lat, wxDouble fix_scr_y)
{
	wxDouble fix_tile_y = lat_to_tile_y(fix_lat, z, projection);
	return tile_y_to_lat( fix_tile_y + (y - fix_scr_y) / 256.0, z, projection );
}

void wxCartographer::sort_queue(tiles_queue &queue, my::worker::ptr worker)
{
	tile::id fix_tile; /* Тайл в центре экрана */

	/* Копируем все нужные параметры, обеспечив блокировку */
	{
		LOCK_LOG(L"recursive params_mutex - sort_queue()");
		recursive_mutex::scoped_lock l(params_mutex_);
		fix_tile.map_id = active_map_id_;
		fix_tile.z = (int)(z_ + 0.5);
		fix_tile.x = (wxCoord)lon_to_tile_x(fix_lon_, (wxDouble)fix_tile.z);
		fix_tile.y = (wxCoord)lat_to_tile_y(fix_lat_, (wxDouble)fix_tile.z,
			maps_[fix_tile.map_id].projection);
	}

	sort_queue(queue, fix_tile, worker);
}

void wxCartographer::sort_queue(tiles_queue &queue,
	const tile::id &fix_tile, my::worker::ptr worker)
{
	if (worker)
	{
		LOCK_LOG(L"unique worker->mutex - sort_queue()");
		unique_lock<mutex> lock = worker->create_lock();
		queue.sort( boost::bind(
			&wxCartographer::sort_by_dist, fix_tile, _1, _2) );
	}
}

bool wxCartographer::sort_by_dist( tile::id fix_tile,
	const tiles_queue::item_type &first,
	const tiles_queue::item_type &second )
{
	tile::id first_id = first.key();
	tile::id second_id = second.key();

	/* Вперёд тайлы для активной карты */
	if (first_id.map_id != second_id.map_id)
		return first_id.map_id == fix_tile.map_id;

	/* Вперёд тайлы близкие по масштабу */
	if (first_id.z != second_id.z)
		return std::abs(first_id.z - fix_tile.z)
			< std::abs(second_id.z - fix_tile.z);

	/* Дальше остаются тайлы на одной карте, с одним масштабом */

	/* Для расчёта растояний координаты тайлов должны быть в одном масштабе! */
	while (fix_tile.z < first_id.z)
		++fix_tile.z, fix_tile.x <<= 1, fix_tile.y <<= 1;
	while (fix_tile.z > first_id.z)
		--fix_tile.z, fix_tile.x >>= 1, fix_tile.y >>= 1;

	int dx1 = first_id.x - fix_tile.x;
	int dy1 = first_id.y - fix_tile.y;
	int dx2 = second_id.x - fix_tile.x;
	int dy2 = second_id.y - fix_tile.y;
	return std::sqrt( (double)(dx1*dx1 + dy1*dy1) )
		< std::sqrt( (double)(dx2*dx2 + dy2*dy2) );
}

void wxCartographer::paint_debug_info(wxDC &gc,
	wxCoord width, wxCoord height)
{
	/* Отладочная информация */
	//gc.SetPen(*wxWHITE_PEN);
	//gc.DrawLine(0, height/2, width, height/2);
	//gc.DrawLine(width/2, 0, width/2, height);

	gc.SetTextForeground(*wxWHITE);
	gc.SetFont( wxFont(6, wxFONTFAMILY_DEFAULT,
		wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

	paint_debug_info_int(gc, width, height);
}

void wxCartographer::paint_debug_info(wxGraphicsContext &gc,
	wxCoord width, wxCoord height)
{
	/* Отладочная информация */
	gc.SetPen(*wxWHITE_PEN);
	gc.StrokeLine(0, height/2, width, height/2);
	gc.StrokeLine(width/2, 0, width/2, height);

	gc.SetFont( wxFont(6, wxFONTFAMILY_DEFAULT,
		wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL), *wxWHITE);

	paint_debug_info_int(gc, width, height);
}

template<class DC>
void wxCartographer::paint_debug_info_int(DC &gc,
	wxCoord width, wxCoord height)
{
	wxCoord x = 8;
	wxCoord y = 8;
	wchar_t buf[200];

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"speed: %0.1f ms", anim_speed_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"freq: %0.1f ms", anim_freq_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"animator: %d", animator_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"painter: %d", painter_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"draw_tile: %d", draw_tile_debug_dounter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"builder: %d", builder_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"file_loader: %d", file_loader_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"server_loader: %d", server_loader_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf(buf, sizeof(buf)/sizeof(*buf), L"z: %0.1f", z_);
	gc.DrawText(buf, x, y), y += 12;

	int d;
	int m;
	double s;
	TO_DEG(fix_lat_, d, m, s);
	swprintf(buf, sizeof(buf)/sizeof(*buf), L"lat: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;

	TO_DEG(fix_lon_, d, m, s);
	swprintf(buf, sizeof(buf)/sizeof(*buf), L"lon: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;
}

void wxCartographer::paint_map(wxDC &gc, wxCoord width, wxCoord height,
	int map_id, int z, wxDouble fix_lat, wxDouble fix_lon,
	wxDouble fix_scr_x, wxDouble fix_scr_y)
{
	/*
		Подготовка фона для заданного z:
			width, height - размеры окна
			map_id - номер карты
			z - масштаб
			fix_lat, fix_lon - географические координаты фиксированной точки
			fix_scr_x, fix_scr_y - фиксированная точка
	*/

	wxCartographer::map map = maps_[map_id];

	/* "Тайловые" координаты центра экрана */
	wxDouble fix_tile_x = lon_to_tile_x(fix_lon, z);
	wxDouble fix_tile_y = lat_to_tile_y(fix_lat, z, map.projection);

	/* Тайл в центре экрана */
	tile::id fix_tile(map_id, z, (int)fix_tile_x, (int)fix_tile_y);

	/* И сразу его в очередь на загрузку - глядишь,
		к моменту отрисовки он уже и загрузится */
	get_tile(fix_tile);

	wxCoord x;
	wxCoord y;

	/* Определяем тайл верхнего левого угла экрана */
	x = (wxCoord)(fix_scr_x - (fix_tile_x - fix_tile.x) * 256.0 + 0.5);
	y = (wxCoord)(fix_scr_y - (fix_tile_y - fix_tile.y) * 256.0 + 0.5);

	int first_x = fix_tile.x;
	int first_y = fix_tile.y;

	while (x > 0)
		x -= 256, --first_x;
	while (y > 0)
		y -= 256, --first_y;

	/* Определяем тайл нижнего правого угла экрана */
	x += 256; /* x,y - координаты нижнего правого угла тайла */
	y += 256;
	int last_x = first_x;
	int last_y = first_y;

	while (x < width)
		x += 256, ++last_x;
	while (y < height)
		y += 256, ++last_y;


	x -= 256; /* x,y - координаты верхнего левого угла правого нижнего тайла */
	y -= 256;


	/* Рисуем */
	tile::id tile_id(map_id, z, last_x, 0);

	draw_tile_debug_dounter_ = 0;

	gc.SetPen(*wxBLACK_PEN);
	gc.SetBrush(*wxBLACK_BRUSH);

	#ifndef NDEBUG
	gc.SetTextForeground(*wxWHITE);
	gc.SetFont( wxFont(6, wxFONTFAMILY_DEFAULT,
		wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
	#endif

	for (wxCoord tx = x; tile_id.x >= first_x; --tile_id.x, tx -= 256)
	{
		tile_id.y = last_y;

		for (wxCoord ty = y; tile_id.y >= first_y; --tile_id.y, ty -= 256)
		{
			tile::ptr tile_ptr = get_tile(tile_id);
			if (tile_ptr && tile_ptr->ok())
			{
				++draw_tile_debug_dounter_;
				gc.DrawBitmap(tile_ptr->bitmap(), tx, ty);
			}
			else
			{
				/* Чёрный тайл */
				gc.DrawRectangle(tx, ty, 256, 256);
			}

			#ifndef NDEBUG
			wxCoord sx = tx + 2;
			wxCoord sy = ty + 2;
			wchar_t buf[200];

			swprintf(buf, sizeof(buf)/sizeof(*buf),
				L"x=%d, y=%d", tile_id.x, tile_id.y);
			gc.DrawText(buf, sx, sy), sy += 12;

			if (tile_ptr)
			{
				gc.DrawText(tile_ptr->ok() ? L"ok" : L"null", sx, sy), sy += 12;

				if (tile_ptr->need_for_load())
					gc.DrawText(L"need_for_load", sx, sy), sy += 12;

				gc.DrawText(tile_ptr->loaded() ? L"loaded" : L"builded", sx, sy), sy += 12;

				if (tile_ptr->need_for_build())
					gc.DrawText(L"need_for_build", sx, sy), sy += 12;

				swprintf(buf, sizeof(buf)/sizeof(*buf), L"level: %d", tile_ptr->level());
				gc.DrawText(buf, sx, sy), sy += 12;
			}
			#endif

		}
	}

	/* Перестраиваем очереди загрузки тайлов.
		К этому моменту все необходимые тайлы уже в файловой очереди
		благодаря get_tile(). Но если её так и оставить, то файлы будут
		загружаться с правого нижнего угла, а нам хотелось бы, чтоб с центра */
	sort_queue(file_queue_, fix_tile, file_loader_);

	/* Серверную очередь тоже корректируем */
	sort_queue(server_queue_, fix_tile, server_loader_);
}

void wxCartographer::repaint(wxDC &dc_win)
{
	LOCK_LOG(L"unique paint_mutex - repaint()");
	mutex::scoped_lock l(paint_mutex_);

	wxCoord width, height;
	GetClientSize(&width, &height);

	if (!buffer_.IsOk()
		|| buffer_.GetWidth() != width || buffer_.GetHeight() != height)
	{
		/* Вот такая хитрая комбинация в сравнении с
			buffer_.Create(width, height); ускоряет вывод:
			1) на чёрном экране (DrawRectangle) в 5 раз;
			2) на заполненном экране (DrawBitmap) в 2 раза. */
		wxImage image(width, height, false);
		image.InitAlpha();
		buffer_ = wxBitmap(image);

		//buffer_.Create(width, height);
	}

	{
		wxMemoryDC dc(buffer_);
		wxGCDC gc(dc);

		/* Очищаем */
		gc.SetBrush(*wxBLACK_BRUSH);
		gc.Clear();

		/* Копируем все нужные параметры */
		//int map_id;
		//wxDouble z;
		//wxDouble lat, lon;

		LOCK_LOG(L"recursive params_mutex - repaint()");
		recursive_mutex::scoped_lock l(params_mutex_);
		//map_id = active_map_id_;
		//z = z_;
		//lat = lat_;
		//lon = lon_;

		/* Рисуем */
		++painter_debug_counter_;
		paint_map(gc, width, height, active_map_id_, z_,
			fix_lat_, fix_lon_, width * fix_kx_, height * fix_ky_);

		//if (on_paint_)
			//on_paint_(gc, width, height);

		#ifndef NDEBUG
		paint_debug_info(gc, width, height);
		#endif

		dc.SelectObject(wxNullBitmap);
	}

	/* Перерисовываем окно */
	dc_win.DrawBitmap( buffer_, 0, 0);

	#ifdef _WINDOWS
	#if 0
	Gdiplus::Graphics *gr_win = (Gdiplus::Graphics*)gc_win->GetNativeContext();
	HDC hdc_win = gr_win->GetHDC();
	HDC hdc_buf = gr_buf->GetHDC();

	BLENDFUNCTION bf = {AC_SRC_OVER, 0, 128, AC_SRC_ALPHA};
	AlphaBlend(hdc_win, 0, 0, width, height, hdc_buf, 0, 0, width, height, bf);

	gr_win->ReleaseHDC(hdc_win);
	gr_buf->ReleaseHDC(hdc_buf);
	#endif
	#endif

	#if 0
	paint_debug_info(*gc_win.get(), width, height);
	#endif
}

void wxCartographer::move_fix_to_scr_xy(wxDouble scr_x, wxDouble scr_y)
{
	LOCK_LOG(L"recursive params_mutex - move_fix_to_scr_xy()");
	recursive_mutex::scoped_lock l(params_mutex_);

	fix_kx_ = scr_x / widthd();
	fix_ky_ = scr_y / heightd();
}

void wxCartographer::set_fix_to_scr_xy(wxDouble scr_x, wxDouble scr_y)
{
	LOCK_LOG(L"recursive params_mutex - set_fix_to_scr_xy()");
	recursive_mutex::scoped_lock l(params_mutex_);

	fix_lat_ = scr_y_to_lat(scr_y, z_, maps_[active_map_id_].projection,
		fix_lat_, heightd() * fix_ky_);
	fix_lon_ = scr_x_to_lon(scr_x, z_, fix_lon_, widthd() * fix_kx_);

	move_fix_to_scr_xy(scr_x, scr_y);
}

void wxCartographer::on_paint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	repaint(dc);

	event.Skip(false);
}

void wxCartographer::on_erase_background(wxEraseEvent& event)
{
	event.Skip(false);
}

void wxCartographer::on_size(wxSizeEvent& event)
{
	Refresh(false);
}

void wxCartographer::on_left_down(wxMouseEvent& event)
{
	SetFocus();

	set_fix_to_scr_xy( (wxDouble)event.GetX(), (wxDouble)event.GetY() );

	CaptureMouse();
}

void wxCartographer::on_left_up(wxMouseEvent& event)
{
	if (HasCapture())
	{
		set_fix_to_scr_xy( widthd() / 2.0, heightd() / 2.0 );
		ReleaseMouse();
	}
}

void wxCartographer::on_capture_lost(wxMouseCaptureLostEvent& event)
{
	set_fix_to_scr_xy( widthd() / 2.0, heightd() / 2.0 );
	Refresh(false);
}

void wxCartographer::on_mouse_move(wxMouseEvent& event)
{
	if (HasCapture())
	{
		move_fix_to_scr_xy( (wxDouble)event.GetX(), (wxDouble)event.GetY() );
		Refresh(false);
	}
}

void wxCartographer::on_mouse_wheel(wxMouseEvent& event)
{
	{
		LOCK_LOG(L"recursive params_mutex - on_mouse_wheel()");
		recursive_mutex::scoped_lock l(params_mutex_);

		int z = (int)z_ + event.GetWheelRotation() / event.GetWheelDelta();

		if (z < 1) z = 1;
		if (z > 30) z = 30;

		z_ = (wxDouble)z;
	}

	Refresh(false);
}

void wxCartographer::GetMaps(std::vector<wxCartographer::map> &Maps)
{
	Maps.clear();

	for (maps_list::iterator iter = maps_.begin();
		iter != maps_.end(); ++iter)
	{
		Maps.push_back(iter->second);
	}
}

wxCartographer::map wxCartographer::GetActiveMap()
{
	LOCK_LOG(L"recursive params_mutex - GetActiveMap()");
	recursive_mutex::scoped_lock l(params_mutex_);
	return maps_[active_map_id_];
}

bool wxCartographer::SetActiveMap(const std::wstring &MapName)
{
	int map_num_id = maps_name_to_id_[MapName];

	if (map_num_id)
	{
		LOCK_LOG(L"recursive params_mutex - SetActiveMap()");
		recursive_mutex::scoped_lock l(params_mutex_);
		active_map_id_ = map_num_id;

		Refresh(false);

		return true;
	}

	return false;
}

wxCoord wxCartographer::LatToY(wxDouble Lat)
{
	LOCK_LOG(L"recursive params_mutex - LatToY()");
	recursive_mutex::scoped_lock l(params_mutex_);
	return (wxCoord)(lat_to_scr_y(Lat, z_, maps_[active_map_id_].projection,
		fix_lat_, heightd() * fix_ky_) + 0.5);
}

wxCoord wxCartographer::LonToX(wxDouble Lon)
{
	LOCK_LOG(L"recursive params_mutex - LonToX()");
	recursive_mutex::scoped_lock l(params_mutex_);
	return (wxCoord)(lon_to_scr_x(Lon, z_,
		fix_lon_, widthd() * fix_kx_) + 0.5);
}

int wxCartographer::GetZ(void)
{
	LOCK_LOG(L"recursive params_mutex - GetZ()");
	recursive_mutex::scoped_lock l(params_mutex_);
	return (int)(z_ + 0.5);
}

void wxCartographer::SetZ(int z)
{
	LOCK_LOG(L"recursive params_mutex - SetZ()");
	recursive_mutex::scoped_lock l(params_mutex_);
	z_ = z;
	Refresh(false);
}

wxDouble wxCartographer::GetLat()
{
	LOCK_LOG(L"recursive params_mutex - GetLat()");
	recursive_mutex::scoped_lock l(params_mutex_);
	return fix_lat_;
}

wxDouble wxCartographer::GetLon()
{
	LOCK_LOG(L"recursive params_mutex - GetLon()");
	recursive_mutex::scoped_lock l(params_mutex_);
	return fix_lon_;
}

void wxCartographer::MoveTo(int z, wxDouble lat, wxDouble lon)
{
	LOCK_LOG(L"recursive params_mutex - MoveTo()");
	recursive_mutex::scoped_lock l(params_mutex_);
	z_ = z;
	fix_lat_ = lat;
	fix_lon_ = lon;
	Refresh(false);
}
