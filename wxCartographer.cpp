#include "wxCartographer.h"

/* windows */
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

#include <my_exception.h>
#include <my_ptr.h>
#include <my_utf8.h>
#include <my_time.h>
#include <my_xml.h>
#include <my_fs.h> /* boost::filesystem */

#include <cmath> /* sin, sqrt */
#include <cwchar> /* swprintf_s */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

#include <wx/dcclient.h> /* wxPaintDC */
#include <wx/dcmemory.h> /* wxMemoryDC */
#include <wx/rawbmp.h> /* wxNativePixelData */

template<typename Real>
Real atanh(Real n)
{
	return 0.5 * std::log( (1.0 + n) / (1.0 - n) );
}

wxCartographer::wxCartographer(wxWindow *window, const std::wstring &server,
	const std::wstring &port, std::size_t cache_size,
	std::wstring cache_path, bool only_cache,
	const std::wstring &init_map, int init_z, wxDouble init_lat, wxDouble init_lon,
	int anim_period, int def_anim_steps)
	: window_(window)
	, cache_(cache_size)
	, cache_path_( fs::system_complete(cache_path).string() )
	, only_cache_(only_cache)
	, builder_debug_counter_(0)
	, animator_debug_counter_(0)
	, draw_tile_debug_dounter_(0)
	, file_loader_debug_counter_(0)
	, server_loader_debug_counter_(0)
	, file_queue_(100)
	, server_queue_(100)
	, buffer_(100,100)
	, z_(init_z)
	, lat_(init_lat)
	, lon_(init_lon)
	, active_map_id_(0)
	, anim_period_( posix_time::milliseconds(anim_period) )
	, def_anim_steps_(def_anim_steps)
	, anim_speed_(0)
	, anim_freq_(0)
{
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
					my::ip::punycode_encode(server),
					my::ip::punycode_encode(port));
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

				if (active_map_id_ == 0 || map.name == init_map)
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
	catch(std::exception &e)
	{
		throw my::exception(L"Ошибка создания Картографа")
			<< my::param(L"server", server)
			<< my::param(L"port", port)
			<< my::exception(e);
	}

	window_->Bind(wxEVT_PAINT, &wxCartographer::OnPaint, this);
	window_->Bind(wxEVT_ERASE_BACKGROUND, &wxCartographer::OnEraseBackground, this);
	window_->Bind(wxEVT_LEFT_DOWN, &wxCartographer::OnLeftDown, this);
	window_->Bind(wxEVT_LEFT_UP, &wxCartographer::OnLeftUp, this);
	window_->Bind(wxEVT_MOTION, &wxCartographer::OnMouseMove, this);
	window_->Bind(wxEVT_MOUSEWHEEL, &wxCartographer::OnMouseWheel, this);

	//get_tile( tile::id(1, 12, 1792, 709) );
}

wxCartographer::~wxCartographer()
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
	
	
	#ifdef _DEBUG
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
		unique_lock<shared_mutex> l(cache_mutex_);

		/* Ищем тайл в кэше */
		tile::ptr old = cache_[tile_id];

		/* Если отсутствует, то просто добавляем */
		if (!old)
			cache_[tile_id] = ptr;

		/* Если новый файл "лучше" старого - меняем содержимое */
		else if (ptr->level() < old->level())
		{
			/* Если старый тайл не нуждался в загрузке,
				сохраняем этот флаг и у нового */
			if (!old->need_for_load())
				ptr->reset_need_for_load();

			cache_[tile_id] = ptr;
		}
		/* Если "хуже" - оставляем старый, но если новый
			не нуждается в загрузке, тогда и старый тоже */
		else
		{
			if (!ptr->need_for_load())
				old->reset_need_for_load();
		}
	}

	/* Оповещаем об изменении кэша */
	Update();
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
	shared_lock<shared_mutex> l(cache_mutex_);

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

wxCartographer::tile::ptr wxCartographer::build_tile(const tile::id &tile_id)
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

	/* Добавляем в очередь на загрузку - однозначно,
		лучше загруженный тайл, чем построенный */
	if ( need_for_load(tile_ptr) )
		add_to_file_queue(tile_id);

	/* Если тайл не нуждается в построении, пропускаем */
	if ( !need_for_build(tile_ptr) )
		return tile_ptr;

	++builder_debug_counter_;

	/* Строим предка */
	tile::ptr parent_ptr = build_tile( tile::id(
		tile_id.map_id, tile_id.z - 1,
		tile_id.x >> 1, tile_id.y >> 1) );

	/* Если не удалось построить предка или наш тайл уже построен от него
		- возвращаемся с тем, что есть */
	if (!parent_ptr || tile_ptr && tile_ptr->level() -1 == parent_ptr->level())
		return tile_ptr;

	/* Строим тайл */
	tile_ptr.reset( new tile(parent_ptr->level() + 1) );

	wxMemoryDC tile_dc( tile_ptr->bitmap() );
	wxMemoryDC parent_dc( parent_ptr->bitmap() );

	tile_dc.StretchBlit( 0, 0, 256, 256, &parent_dc,
		(tile_id.x & 1) * 128, (tile_id.y & 1) * 128, 128, 128 );

	/* Рамка вокруг построенного тайла */
	/*-
	tile_dc.SetPen(*wxWHITE_PEN);
	tile_dc.SetBrush(*wxTRANSPARENT_BRUSH);
	tile_dc.DrawRectangle(0, 0, 256, 256);
	-*/

	add_to_cache(tile_id, tile_ptr);

	return tile_ptr;
}

wxCartographer::tile::ptr wxCartographer::get_tile(const tile::id &tile_id)
{
	/* Строим и загружаем тайл */
	return build_tile(tile_id);
}

void wxCartographer::add_to_file_queue(const tile::id &tile_id)
{
	/* Копированием указателя на "работника" гарантируем,
		что он не будет удалён, пока выполняется функция */
	my::worker::ptr worker = file_loader_;
	if (worker)
	{
		unique_lock<mutex> lock = worker->create_lock();
		file_queue_[tile_id] = 0; /* Не важно значение, важно само присутствие */
		wake_up(worker, lock); /* Будим работника, если он спит */
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
			unique_lock<mutex> lock = this_worker->create_lock();
			
			tiles_queue::iterator iter = file_queue_.begin();

			/* Если очередь пуста - засыпаем */
			if (iter == file_queue_.end())
			{
				sleep(this_worker, lock);
				continue;
			}

			tile_id = iter->key();
			file_queue_.erase(iter);

			/* Для дальнейших действий блокировка нам не нужна */ 
		}

		/* Если тайл уже загружен, пропускаем */
		if ( !need_for_load( find_tile(tile_id) ) )
			continue;

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
			unique_lock<mutex> lock = this_worker->create_lock();
			
			tiles_queue::iterator iter = server_queue_.begin();

			/* Если очередь пуста - засыпаем */
			if (iter == server_queue_.end())
			{
				sleep(this_worker, lock);
				continue;
			}

			tile_id = iter->key();
			server_queue_.erase(iter);

			/* Для дальнейших действий блокировка нам не нужна */ 
		}

		/* Если тайл уже загружен, пропускаем */
		if ( !need_for_load( find_tile(tile_id) ) )
			continue;

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
		}
		catch (...)
		{
			/* Связь с сервером может отсутствовать,
				игнорируем сей печальный факт */
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

		repaint();

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

bool wxCartographer::check_buffer()
{
	int w, h;
	window_->GetClientSize(&w, &h);

	if (!buffer_.IsOk() || buffer_.GetWidth() != w || buffer_.GetHeight() != h)
	{
		buffer_.Create(w, h);
		return false;
	}

	return true;
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

wxDouble wxCartographer::lon_to_x(wxDouble lon, wxDouble z)
{
	return (lon + 180.0) * size_for_z(z) / 360.0;
}

wxDouble wxCartographer::lat_to_y(wxDouble lat, wxDouble z,
	map::projection_t projection)
{
	wxDouble s = std::sin( lat / 180.0 * M_PI );
	wxDouble res;

	switch (projection)
	{
		case map::spheroid:
			res = size_for_z(z) * (0.5 - atanh(s) / (2*M_PI));
			break;
		
		case map::ellipsoid:
			#define EXCT 0.081819790992
			res = size_for_z(z) * (0.5 - (atanh(s) - EXCT*atanh(EXCT*s)) / (2*M_PI));
			break;
		
		default:
			assert(projection != map::spheroid && projection != map::ellipsoid);
	}

	return res;
}

void wxCartographer::sort_queue(tiles_queue &queue, my::worker::ptr worker)
{
	tile::id central_tile; /* Тайл в центре экрана */

	/* Копируем все нужные параметры, обеспечив блокировку */
	{
		shared_lock<shared_mutex> l(params_mutex_);
		central_tile.map_id = active_map_id_;
		central_tile.z = (int)(z_ + 0.5);
		central_tile.x = (wxCoord)lon_to_x(lon_, (wxDouble)central_tile.z);
		central_tile.y = (wxCoord)lat_to_y(lat_, (wxDouble)central_tile.z,
			maps_[central_tile.map_id].projection);
	}

	sort_queue(queue, central_tile, worker);
}

void wxCartographer::sort_queue(tiles_queue &queue,
	const tile::id &tile, my::worker::ptr worker)
{
	if (worker)
	{
		unique_lock<mutex> lock = worker->create_lock();
		queue.sort( boost::bind(
			&wxCartographer::sort_by_dist, tile, _1, _2) );
	}
}

bool wxCartographer::sort_by_dist( tile::id tile,
	const tiles_queue::item_type &first,
	const tiles_queue::item_type &second )
{
	tile::id first_id = first.key();
	tile::id second_id = second.key();

	/* Вперёд тайлы для активной карты */
	if (first_id.map_id != second_id.map_id)
		return first_id.map_id == tile.map_id;

	/* Вперёд тайлы близкие по масштабу */
	if (first_id.z != second_id.z)
		return std::abs(first_id.z - tile.z)
			< std::abs(second_id.z - tile.z);

	/* Дальше остаются тайлы на одной карте, с одним масштабом */

	/* Для расчёта растояний координаты тайлов должны быть в одном масштабе! */
	while (tile.z < first_id.z)
		++tile.z, tile.x <<= 1, tile.y <<= 1;
	while (tile.z > first_id.z)
		--tile.z, tile.x >>= 1, tile.y >>= 1;

	int dx1 = first_id.x - tile.x;
	int dy1 = first_id.y - tile.y;
	int dx2 = second_id.x - tile.x;
	int dy2 = second_id.y - tile.y;
	return std::sqrt( (double)(dx1*dx1 + dy1*dy1) )
		< std::sqrt( (double)(dx2*dx2 + dy2*dy2) );
}

void wxCartographer::paint_debug_info(wxDC &gc,
	wxCoord width, wxCoord height)
{
	/* Отладочная информация */
	gc.SetPen(*wxWHITE_PEN);
	gc.DrawLine(0, height/2, width, height/2);
	gc.DrawLine(width/2, 0, width/2, height);

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

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"speed: %0.1f ms", anim_speed_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"freq: %0.1f ms", anim_freq_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"animator: %d", animator_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"draw_tile: %d", draw_tile_debug_dounter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"builder: %d", builder_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"file_loader: %d", file_loader_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"server_loader: %d", server_loader_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	swprintf_s(buf, sizeof(buf)/sizeof(*buf), L"z: %0.1f", z_);
	gc.DrawText(buf, x, y), y += 12;
}

void wxCartographer::paint_map(wxDC &gc, wxCoord width, wxCoord height,
	int map_id, int z, wxDouble lat, wxDouble lon)
{
	/*
		Подготовка фона для заданного z:
			width, height - размеры окна
			map_id - номер карты
			z - масштаб
			lat, lon - географические координаты центра карты
	*/

	wxCartographer::map map = maps_[map_id];

	/* "Тайловые" координаты центра экрана */
	wxDouble scr_x = lon_to_x(lon, z);
	wxDouble scr_y = lat_to_y(lat, z, map.projection);

	/* Тайл в центре экрана */
	tile::id central_tile(map_id, z, (int)scr_x, (int)scr_y);

	/* И сразу его в очередь на загрузку - глядишь,
		к моменту отрисовки он уже и загрузится */
	get_tile(central_tile);

	wxCoord x;
	wxCoord y;

	/* Определяем тайл верхнего левого угла экрана */
	x = (wxCoord)(width / 2.0 - (scr_x - central_tile.x) * 256.0 + 0.5);
	y = (wxCoord)(height / 2.0 - (scr_y - central_tile.y) * 256.0 + 0.5);

	int first_x = central_tile.x;
	int first_y = central_tile.y;

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
		}
	}

	/* Перестраиваем очереди загрузки тайлов.
		К этому моменту все необходимые тайлы уже в файловой очереди
		благодаря get_tile(). Но если её так и оставить, то файлы будут
		загружаться с правого нижнего угла, а нам хотелось бы, чтоб с центра */
	sort_queue(file_queue_, central_tile, file_loader_);

	/* Серверную очередь тоже кооректируем */
	sort_queue(server_queue_, central_tile, server_loader_);
}

void wxCartographer::repaint()
{
	mutex::scoped_lock l(paint_mutex_);

	wxCoord width, height;
	window_->GetClientSize(&width, &height);

	//static wxAlphaPixelData *pdata;
	//static char *data = 0;

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

		//delete[] data;
		//data = new char[width*height*4];
		//buffer_ = wxBitmap(data, width, height, 32);
		
		//delete pdata;
		//pdata = new wxAlphaPixelData(buffer_);
	}

	wxMemoryDC dc(buffer_);
	wxGCDC gc(dc);
	
	/* Очищаем */
	gc.SetBrush(*wxBLACK_BRUSH);
	gc.DrawRectangle(0, 0, width, height);

	/* Копируем все нужные параметры */
	int map_id;
	wxDouble z;
	wxDouble lat, lon;

	{
		shared_lock<shared_mutex> l(params_mutex_);
		map_id = active_map_id_;
		z = z_;
		lat = lat_;
		lon = lon_;
	}

	/* Рисуем */
	paint_map(gc, width, height, map_id, z, lat, lon);

	Gdiplus::Graphics *gr_buf = (Gdiplus::Graphics*)gc.GetGraphicsContext()->GetNativeContext();
	Gdiplus::Color color(128, 255, 0, 0);
	Gdiplus::Pen pen(color, 10);
	gr_buf->DrawLine(&pen, 0, 0, 200, 200);

	#ifdef _DEBUG
	paint_debug_info(gc, width, height);
	#endif

	/*-
	{
		wxImage image = buffer_.ConvertToImage();
		void *data = image.GetAlpha();
		memset(data, 128, width * height);
		buffer_ = wxBitmap(image);
	}
	-*/

	/* Перерисовываем окно */
	scoped_ptr<wxGraphicsContext> gc_win( wxGraphicsContext::Create(window_) );
	gc_win->SetBrush(*wxBLACK_BRUSH);
	gc_win->DrawRectangle(0.0, 0.0, width, height);

	//gc_win->DrawBitmap(buffer_, 0.0, 0.0, width, height);
	Gdiplus::Graphics *gr_win = (Gdiplus::Graphics*)gc_win->GetNativeContext();
	HDC hdc_win = gr_win->GetHDC();
	HDC hdc_buf = gr_buf->GetHDC();

	BLENDFUNCTION bf = {AC_SRC_OVER, 0, 128, AC_SRC_ALPHA};
	AlphaBlend(hdc_win, 0, 0, width, height, hdc_buf, 0, 0, width, height, bf);

	gr_win->ReleaseHDC(hdc_win);
	gr_buf->ReleaseHDC(hdc_buf);

	#if 0
	paint_debug_info(*gc_win.get(), width, height);
	#endif
}

void wxCartographer::OnPaint(wxPaintEvent& event)
{
	mutex::scoped_lock l(paint_mutex_);

	/* Создание wxPaintDC в обработчике wxPaintEvent обязательно,
		даже если мы не будем им пользоваться! */
	wxPaintDC dc(window_);
	dc.DrawBitmap(buffer_, 0, 0);

	event.Skip(false);
}

void wxCartographer::OnEraseBackground(wxEraseEvent& event)
{
	event.Skip(false);
}

void wxCartographer::OnLeftDown(wxMouseEvent& event)
{
	window_->SetFocus();
}

void wxCartographer::OnLeftUp(wxMouseEvent& event)
{
}

void wxCartographer::OnMouseMove(wxMouseEvent& event)
{
}

void wxCartographer::OnMouseWheel(wxMouseEvent& event)
{
	{
		unique_lock<shared_mutex> l(params_mutex_);

		int z = (int)z_ + event.GetWheelRotation() / event.GetWheelDelta();
	
		if (z < 1) z = 1;
		if (z > 30) z = 30;
	
		z_ = (wxDouble)z;
	}

	Update();
}

void wxCartographer::Update()
{
	/* Копированием указателя на "работника" гарантируем,
		что он не будет удалён, пока выполняется функция */
	my::worker::ptr worker = animator_;
	if (worker)
		wake_up(worker); /* Будим работника, если он спит */
}

void wxCartographer::GetMaps(std::vector<wxCartographer::map> &maps)
{
	maps.clear();

	for (maps_list::iterator iter = maps_.begin();
		iter != maps_.end(); ++iter)
	{
		maps.push_back(iter->second);
	}
}

wxCartographer::map wxCartographer::GetActiveMap()
{
	shared_lock<shared_mutex> l(params_mutex_);
	return maps_[active_map_id_];
}

bool wxCartographer::SetActiveMap(const std::wstring &map_name)
{
	int map_num_id = maps_name_to_id_[map_name];
	
	if (map_num_id)
	{
		unique_lock<shared_mutex> l(params_mutex_);
		active_map_id_ = map_num_id;
		return true;
	}

	return false;
}
