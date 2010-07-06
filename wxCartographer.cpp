#include "wxCartographer.h"

#include <my_exception.h>
#include <my_ptr.h>
#include <my_utf8.h>
#include <my_time.h>
#include <my_xml.h>
#include <my_fs.h> /* boost::filesystem */

#include <cmath>
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

#include <wx/dcclient.h> /* wxPaintDC */
#include <wx/dcmemory.h> /* wxMemoryDC */

template<typename Real>
Real atanh(Real n)
{
	return 0.5 * std::log( (1.0 + n) / (1.0 - n) );
}

wxCartographer::wxCartographer(wxWindow *window, const std::wstring &server,
	const std::wstring &port, std::size_t cache_size,
	std::wstring cache_path, unsigned long flags)
	: window_(window)
	, cache_(cache_size)
	, file_queue_(100)
	, server_queue_(100)
	, cache_path_( fs::system_complete(cache_path).string() )
	, flags_(flags)
	, z_(1)
	, active_map_id_(0)
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
			if ( !(flags_ & wxCart_ONLYCACHE) )
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
				map mp;
				mp.id = p.first->second.get<std::wstring>(L"id");
				mp.name = p.first->second.get<std::wstring>(L"name", L"");
				mp.is_layer = p.first->second.get<bool>(L"layer", 0);
			
				mp.tile_type = p.first->second.get<std::wstring>(L"tile-type");
				if (mp.tile_type == L"image/jpeg")
					mp.ext = L"jpg";
				else if (mp.tile_type == L"image/png")
					mp.ext = L"png";
				else
					throw my::exception(L"Неизвестный тип тайла")
						<< my::param(L"map", mp.id)
						<< my::param(L"tile-type", mp.tile_type);

				std::wstring projection
					= p.first->second.get<std::wstring>(L"projection");

				if (projection == L"spheroid") /* Google */
					mp.projection = map::spheroid;
				else if (projection == L"ellipsoid") /* Yandex */
					mp.projection = map::ellipsoid;
				else
					throw my::exception(L"Неизвестный тип проекции")
						<< my::param(L"map", mp.id)
						<< my::param(L"projection", projection);

				/* Сохраняем описание карты в списке */
				int id = get_new_map_id(); /* новый идентификатор */
				maps_[id] = mp;

				if (active_map_id_ == 0)
					active_map_id_ = id;
				
				/* Сохраняем соответствие строкового идентикатора числовому */
				maps_id_[mp.id] = id;

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

		
		/* Запускаем "файловый" загрузчик тайлов */
		file_loader_ = new_worker("file_loader"); /* Название - только для отладки */
		boost::thread( boost::bind(
			&wxCartographer::file_loader_proc, this, file_loader_) );

		/* Запускаем "серверный" загрузчик тайлов */
		if ( !(flags_ & wxCart_ONLYCACHE))
		{
			server_loader_ = new_worker("server_loader"); /* Название - только для отладки */
			boost::thread( boost::bind(
				&wxCartographer::server_loader_proc, this, server_loader_) );
		}
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

	/*-
	add_to_file_queue( tile_id(1,2,0,0) );
	add_to_file_queue( tile_id(1,2,0,1) );
	add_to_file_queue( tile_id(1,2,1,0) );
	add_to_file_queue( tile_id(1,2,1,1) );
	add_to_file_queue( tile_id(1,2,1,2) );
	-*/

	z_ = 12.0;
	lat_ = 48.49481206;
	lon_ = 135.101012;

	wxDouble y = lat_to_y(lat_, 12, map::ellipsoid);
	wxDouble x = lon_to_x(lon_, 12);
	add_to_file_queue( tile_id(1, 12, x, y) );
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

    /* Ждём завершения */
	
	#ifdef _DEBUG /* Для отладки - вдруг кто зависнет */
	{
		posix_time::ptime start = my::time::utc_now();
		while (!check_for_finish())
		{
			if (my::time::utc_now() - start >= posix_time::seconds(5))
			{
				std::vector<std::string> v;
				/* Брекпоинт на следующую строчку */
				workers_state(v);
			}
		}
	}
    #endif

	wait_for_finish();
}

void wxCartographer::add_to_cache(const tile_id &id, tile::ptr ptr)
{
	{
		unique_lock<shared_mutex> l(cache_mutex_);
		cache_[id] = ptr;
	}

	/* Оповещаем об изменении кэша */
	//if (on_update_)
	//	on_update_();
}

void wxCartographer::add_to_file_queue(const tile_id &id)
{
	/* Копированием указателя на "работника", гарантируем,
		что он не будет удалён, пока выполняется функция */
	my::worker::ptr worker = file_loader_;
	if (worker)
	{
		unique_lock<mutex> lock = worker->create_lock();
		file_queue_[id] = 0; /* Не важно значение, важно само присутствие */
		wake_up(worker, lock); /* Будим работника, если он спит */
	}
}

void wxCartographer::add_to_server_queue(const tile_id &id)
{
	if ( !(flags_ & wxCart_ONLYCACHE) )
	{
		/* Копированием указателя на "работника", гарантируем,
			что он не будет удалён, пока выполняется функция */
		my::worker::ptr worker = server_loader_;
		if (worker)
		{
			unique_lock<mutex> lock = worker->create_lock();
			server_queue_[id] = 0; /* Не важно значение, важно само присутствие */
			wake_up(worker, lock); /* Будим работника, если он спит */
		}
	}
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void wxCartographer::file_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile_id id;

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

			id = iter->key();
			file_queue_.erase(iter);

			/* Для дальнейших действий блокировка нам не нужна */ 
		}

		/* Если тайл из очереди уже есть в кэше, пропускаем */
		{
			/* Блокируем кэш для чтения */
			shared_lock<shared_mutex> l(cache_mutex_);
			if (cache_.find(id) != cache_.end())
				continue;
		}

		/* Загружаем тайл с диска */
		std::wstringstream tile_path;

		map &mp = maps_[id.map_id];

		tile_path << cache_path_
			<< L"/" << mp.id
			<< L"/z" << id.z
			<< L'/' << (id.x >> 10)
			<< L"/x" << id.x
			<< L'/' << (id.y >> 10)
			<< L"/y" << id.y << L'.' << mp.ext;

		tile::ptr ptr( new tile(tile_path.str()) );

		if (ptr->loaded())
		    /* При успехе операции - сохраняем тайл в кэше */
			add_to_cache(id, ptr);
		else
			/* Если с диска загрузить не удалось - загружаем с сервера */
			add_to_server_queue(id);

	} /* while (!finish()) */
}

/* Загрузчик тайлов с сервера. При пустой очереди - засыпает */
void wxCartographer::server_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile_id id;

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

			id = iter->key();
			server_queue_.erase(iter);

			/* Для дальнейших действий блокировка нам не нужна */ 
		}

		/* Если тайл из очереди уже есть в кэше, пропускаем */
		{
			/* Блокируем кэш для чтения */
			shared_lock<shared_mutex> l(cache_mutex_);
			if (cache_.find(id) != cache_.end())
				continue;
		}

		/* Загружаем тайл с сервера */
		std::wstringstream tile_path; /* Путь к локальному файлу  */

		map &mp = maps_[id.map_id];

		tile_path << cache_path_
			<< L"/" << mp.id
			<< L"/z" << id.z
			<< L'/' << (id.x >> 10)
			<< L"/x" << id.x
			<< L'/' << (id.y >> 10)
			<< L"/y" << id.y << L'.' << mp.ext;

		std::wstringstream request;
		request << L"/maps/gettile?map=" << mp.id
			<< L"&z=" << id.z
			<< L"&x=" << id.x
			<< L"&y=" << id.y;
			
		try
		{
			/* Загружаем с сервера ... */
			if ( load_and_save(request.str(), tile_path.str()) == 200 /*HTTP_OK*/)
			{
				/* ... и сразу с диска */
				tile::ptr ptr( new tile(tile_path.str()) );

				/* При успехе операции - сохраняем в кэше */
				if (ptr->loaded())
					add_to_cache(id, ptr);
			}
		}
		catch (...)
		{
			/* Связь с сервером может отсутствовать,
				игнорируем сей печальный факт */
		}

	} /* while (!finish()) */
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

wxCartographer::tile::ptr wxCartographer::get_tile(int map_id, int z, int x, int y)
{
	tile_id id(map_id, z, x, y);
	tile::ptr ptr;

	{
		/* Блокируем кэш для чтения */
		shared_lock<shared_mutex> l(cache_mutex_);

		tiles_cache::iterator iter = cache_.find(id);

		/* Если в кэше тайла нет - добавляем в очередь
			для загрузки и возвращаем пустой указатель */
		if (iter == cache_.end())
			add_to_file_queue(id);

		/* Если в кэше есть - возвращаем тайл */
		else
			ptr = iter->value();
	}

	return ptr;
}

void wxCartographer::prepare_background(wxBitmap &bitmap, wxDouble width,
	wxDouble height, int map_id, int z, wxDouble lat, wxDouble lon)
{
	/*
		Подготовка фона для заданного z:
			width, height - размеры окна
			map_id - номер карты
			z - масштаб
			lat, lon - географические координаты центра карты
	*/

	wxCartographer::map map = maps_[map_id];

	/* Координаты центра карты (в тайлах) */
	wxDouble y = lat_to_y(lat, (wxDouble)z, map.projection);
	wxDouble x = lon_to_x(lon, (wxDouble)z);
	
	/* "Центральный" тайл */
	int tile_y = (int)y;
	int tile_x = (int)x;

	/* Экранные координаты верхнего левого угла "центрального" тайла */
	wxDouble win_y = height / 2.0 - (y - tile_y) * 256.0;
	wxDouble win_x = width / 2.0 - (x - tile_x) * 256.0;

	/* Определяем тайл нижнего правого угла экрана */
	int last_tile_y = tile_y;
	int last_tile_x = tile_x;

	while (win_y < height)
		win_y += 256.0, ++last_tile_y;
	while (win_x < width)
		win_x += 256.0, ++last_tile_x;

	/* Определяем тайл верхнего левого угла экрана */
	int first_tile_y = last_tile_y;
	int first_tile_x = last_tile_x;

	while (win_y > 0.0)
		win_y -= 256.0, --first_tile_y;
	while (win_x > 0.0)
		win_x -= 256.0, --first_tile_x;

	bitmap.Create(
		(last_tile_x - first_tile_x + 1) * 256,
		(last_tile_y - first_tile_y + 1) * 256);
	
	//wxMemoryDC dc(bitmap);
	//scoped_ptr<wxGraphicsContext> gc( wxGraphicsContext::Create(dc) );
	scoped_ptr<wxGraphicsContext> gc( wxGraphicsContext::Create(bitmap) );

	for (tile_y = first_tile_y, y = win_y; tile_y <= last_tile_y; ++tile_y, y += 256.0)
	{
		for (tile_x = first_tile_x, y = win_x; tile_x <= last_tile_x; ++tile_x, x += 256.0)
		{
			tile::ptr tile_ptr = get_tile(map_id, z, tile_x, tile_y);
			
			if (tile_ptr)
				gc->DrawBitmap(tile_ptr->bitmap(), x, y, 256.0, 256.0);
		}
	}
}

void wxCartographer::paint_map(wxGraphicsContext *gc, wxDouble width, wxDouble height,
	int map_id, wxDouble lat, wxDouble lon, wxDouble z)
{
	/* Прорисовка карты */

#if 0
	/* Суть - сначала закидываем тайлы в background-буфер в масштабе 1:1,
		а затем его проецируем в buffer уже с нужным масштабом */
	wxCartographer::map map = maps_[map_id];

	/* Рисуем фон дважды для плавного перехода между масштабами */
	for (int i = 0; i < 1; ++i)
	{
		int iz = (int)z;
		wxDouble za = z - iz; /* Прозрачность, для плавного перехода между масштабами */
		wxDouble k = 1.0 + za; /* Коэффициент увеличения размера тайлов */

		if (i == 0)
			za = 1.0 - za;
		else
			++iz, k /= 2.0;

		/* Размер "мира" в тайлах */
		//int sz = size_for_int_z(iz);

		/* Реальные размеры тайла */
		wxDouble tile_width = 256.0 * k;

		/* Тайл в центре карты */
		wxDouble x = lon_to_x(lon, (wxDouble)iz);
		wxDouble y = lat_to_y(lat, (wxDouble)iz, map.projection);
		int tile_x = (int)x;
		int tile_y = (int)y;

		tile::ptr tile_ptr = get_tile(map_id, z, tile_x, tile_y);

		if (tile_ptr)
			gc->DrawBitmap(tile_ptr->bitmap(), 0.0, 0.0, tile_width, tile_width);
	}

	if (begin_x < 0) begin_x = 0;
	if (begin_y < 0) begin_y = 0;

	/* Кол-во тайлов, попавших в окно */
	int count_x;
	int count_y;
	{
		/* Размеры окна за вычетом первого тайла */
		float W = bounds.Width - ( x_ + w * (begin_x + 1) );
		float H = bounds.Height - ( y_ + w * (begin_y + 1) );

		/* Округляем в большую сторону, т.е. включаем в окно тайлы,
			частично попавшие в окно. Не забываем и про первый тайл,
			который исключили */
		count_x = (int)ceil(W / w) + 1;
		count_y = (int)ceil(H / w) + 1;

		/* По x - карта бесконечна, по y - ограничена */
		if (begin_x + count_x > sz)
			count_x = sz - begin_x;
		if (begin_y + count_y > sz)
			count_y = sz - begin_y;
	}

	int bmp_w = count_x * 256;
	int bmp_h = count_y * 256;

	/* Если что-то изменилось, перерисовываем */
	size_t hash = 0;
	boost::hash_combine(hash, boost::hash_value(x_));
	boost::hash_combine(hash, boost::hash_value(y_));
	boost::hash_combine(hash, boost::hash_value(w));
	boost::hash_combine(hash, boost::hash_value(bmp_w));
	boost::hash_combine(hash, boost::hash_value(bmp_h));
	boost::hash_combine(hash, boost::hash_value(server_.active_map_id()));

	if (hash != background_hash_)
	{
		background_hash_ = hash;

		/* Если буфер маловат, увеличиваем */
		if (!bitmap_ || (int)bitmap_->GetWidth() < bmp_w
			|| (int)bitmap_->GetHeight() < bmp_h)
		{
			/* Параметры буфера такие же как у экрана ((HWND)0) */
			Gdiplus::Graphics screen((HWND)0, FALSE);
			bitmap_.reset( new Gdiplus::Bitmap(bmp_w, bmp_h, &screen) );
		}

		Gdiplus::Graphics bmp_canvas( bitmap_.get() );

		/* Очищаем буфер */
		{
			Gdiplus::SolidBrush brush( Gdiplus::Color(0, 0, 0, 0) );
			bmp_canvas.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
			bmp_canvas.FillRectangle(&brush, 0, 0, bmp_w, bmp_h);
			bmp_canvas.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
		}

		for (int ix = 0; ix < count_x; ix++)
		{
			for (int iy = 0; iy < count_y; iy++)
			{
				int x = ix + begin_x;
				int y = iy + begin_y;
				server_.paint_tile(&bmp_canvas, ix * 256, iy * 256, z, x, y);
			}
		}
	}
	
	Gdiplus::RectF rect( x_ + begin_x * w, y_ + begin_y * w,
		bmp_w * k, bmp_h * k );
	canvas->DrawImage( bitmap_.get(), rect,
		0.0f, 0.0f, (float)bmp_w, (float)bmp_h,
		Gdiplus::UnitPixel, NULL, NULL, NULL );
#endif
}

void wxCartographer::Repaint()
{
	wxMemoryDC dc(buffer_);
	scoped_ptr<wxGraphicsContext> gc( wxGraphicsContext::Create(dc) );

	check_buffer();
	wxDouble width = (wxDouble)buffer_.GetWidth();
	wxDouble height = (wxDouble)buffer_.GetHeight();

	/* Очищаем */
	gc->SetBrush(*wxBLACK_BRUSH);
	gc->DrawRectangle(0.0, 0.0, width, height);

	prepare_background(background_, width, height,
		active_map_id_, z_, lat_, lon_);
	
	gc->DrawBitmap(background_, 0.0, 0.0, width, height);

	/* Перерисовываем окно */
	scoped_ptr<wxGraphicsContext> gc_win( wxGraphicsContext::Create(window_) );
	gc_win->DrawBitmap(buffer_, 0.0, 0.0, width, height);
}

void wxCartographer::OnPaint(wxPaintEvent& event)
{
	mutex::scoped_lock l(paint_mutex_);

	/* Создание wxPaintDC в обработчике wxPaintEvent обязательно,
		даже если мы не будем им пользоваться! */
	wxPaintDC dc(window_);

	if (!check_buffer())
		Repaint();
	else
		dc.DrawBitmap(buffer_, 0, 0);

	event.Skip(false);
}

void wxCartographer::OnEraseBackground(wxEraseEvent& event)
{
	event.Skip(false);
}

void wxCartographer::OnLeftDown(wxMouseEvent& event)
{
}

void wxCartographer::OnLeftUp(wxMouseEvent& event)
{
}

void wxCartographer::OnMouseMove(wxMouseEvent& event)
{
}

void wxCartographer::OnMouseWheel(wxMouseEvent& event)
{
	int z = (int)z_ + event.GetWheelRotation() / event.GetWheelDelta();
	
	if (z < 1) z = 1;
	if (z > 18) z = 18;
	
	z_ = (wxDouble)z;
	Repaint();
}
