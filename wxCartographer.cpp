#include "wxCartographer.h"

#include <my_exception.h>
#include <my_ptr.h>
#include <my_utf8.h>
#include <my_time.h>
#include <my_xml.h>
#include <my_fs.h> /* boost::filesystem */

#include <sstream>
#include <vector>

#include <boost/bind.hpp>

#include <wx/dcclient.h> /* wxPaintDC */
#include <wx/dcmemory.h> /* wxMemoryDC */
#include <wx/graphics.h> /* wxGraphicsContext */

wxCartographer::wxCartographer(wxWindow *window, const std::wstring &server,
	const std::wstring &port, std::size_t cache_size, long flags)
	: window_(window)
	, cache_(cache_size)
	, queue_(100)
{
	try
	{
		/* Резолвим сервер */
		asio::ip::tcp::resolver resolver(io_service_);
		asio::ip::tcp::resolver::query query(
			my::ip::punycode_encode(server),
			my::ip::punycode_encode(port));
		server_endpoint_ = *resolver.resolve(query);

		std::wstring request;

		/* Загружаем список доступных карт */
		try
		{
			request = L"/maps/maps.xml";

			my::http::reply reply;
			get(reply, request);

			xml::wptree pt;
			reply.to_xml(pt);
	
			std::pair<xml::wptree::assoc_iterator,
				xml::wptree::assoc_iterator> p
					= pt.get_child(L"maps").equal_range(L"map");

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
				
				/* Сохраняем соответствие строкового идентикатора числовому */
				maps_id_[mp.id] = id;

				p.first++;
			}
		}
		catch(std::exception &e)
		{
			throw my::exception(L"Ошибка загрузки списка карт")
				<< my::param(L"request", request)
				<< my::exception(e);
		} /* Загружаем список доступных карт */

		
		/* Запускаем загрузчика тиайлов */
		loader_ = new_worker("loader");
		boost::thread( boost::bind(
			&wxCartographer::loader_proc, this, loader_) );

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

	queue_[tile_id(1,2,0,0)] = 0;
	queue_[tile_id(1,2,0,1)] = 0;
	queue_[tile_id(1,2,1,0)] = 0;
	queue_[tile_id(1,2,1,1)] = 0;
	wake_up(loader_);
}

wxCartographer::~wxCartographer()
{
	/* Как обычно, самое весёлое занятие - это
		умудриться остановить всю эту махину */

	/* Оповещаем о завершении работы */
	lets_finish();

	/* Освобождаем ("увольняем") всех "работников" */
	dismiss(loader_);

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

void wxCartographer::loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile_id id;

		/* Берём идентификатор очередного тайла */
		{
			unique_lock<mutex> l(queue_mutex_);
			
			tiles_queue::iterator iter = queue_.begin();
			if (iter != queue_.end())
			{
				id = iter->key();
				queue_.erase(iter);
			}
		}

		/* Если очередь пуста - засыпаем */
		if (!id)
		{
			sleep(this_worker);
			continue;
		}

		/* Если тайл из очереди уже есть в кэше, ищем дальше */
		if (cache_.find(id) != cache_.end())
			continue;

		/* Загружаем тайл с диска */
		std::wstringstream tile_path;

		map &mp = maps_[id.map_id];

		tile_path << fs::initial_path().file_string()
			<< L"\\maps\\" << mp.id
			<< L"\\z" << id.z
			<< L'\\' << (id.x >> 10)
			<< L"\\x" << id.x
			<< L'\\' << (id.y >> 10)
			<< L"\\y" << id.y << L'.' << mp.ext;

		tile::ptr ptr( new tile(tile_path.str()) );

		/* Если загрузить с диска не удалось - загружаем с сервера */
		if (!ptr->loaded())
		{
			std::wstringstream request;
			request << L"/maps/gettile?map=" << mp.id
				<< L"&z=" << id.z
				<< L"&x=" << id.x
				<< L"&y=" << id.y;
			
			try
			{
				load_file(request.str(), tile_path.str());
				ptr.reset( new tile(tile_path.str()) );
			}
			catch (...)
			{
				//
			}
		}

        /* Вносим изменения в список загруженных тайлов */
		if (ptr->loaded())
		{
			unique_lock<shared_mutex> l(cache_mutex_);
			cache_[id] = ptr;
		}

		//if (on_update_)
		//	on_update_();

	} /* while (true) */
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
}

void wxCartographer::OnPaint(wxPaintEvent& event)
{
	mutex::scoped_lock l(paint_mutex_);

	/* Создание wxPaintDC в обработчике wxPaintEvent обязательно,
		даже если мы не будем им пользоваться! */
	wxPaintDC dc(window_);

	if (!check_buffer())
		Repaint();
	//else
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
}

void wxCartographer::get(my::http::reply &reply,
	const std::wstring &request)
{
	asio::ip::tcp::socket socket(io_service_);
	socket.connect(server_endpoint_);
	
	std::string full_request = "GET "
		+ my::http::percent_encode(my::utf8::encode(request))
		+ " HTTP/1.1\r\n\r\n";

	reply.get(socket, full_request);
}

unsigned int wxCartographer::load_file(const std::wstring &file,
	const std::wstring &file_local)
{
	my::http::reply reply;

    get(reply, file);

	if (reply.status_code == 200)
		reply.save(file_local);
	
	return reply.status_code;
}
