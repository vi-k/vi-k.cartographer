#include "wxCartographer.h"
#include "handle_exception.h"

#if defined(_MSC_VER)
#define __swprintf swprintf_s
#else
#define __swprintf snwprintf
#endif

#include <cmath> /* std::sin, std::sqrt */
#include <cwchar> /* swprintf */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

MYLOCKINSPECTOR_INIT()

#define EXCT 0.081819790992 /* эксцентриситет эллипса */

template<typename Real>
Real atanh(Real x)
{
	return 0.5 * log( (1.0 + x) / (1.0 - x) );
}

BEGIN_EVENT_TABLE(wxCartographer, wxGLCanvas)
	EVT_PAINT(wxCartographer::on_paint)
	EVT_ERASE_BACKGROUND(wxCartographer::on_erase_background)
	EVT_SIZE(wxCartographer::on_size)
	EVT_LEFT_DOWN(wxCartographer::on_left_down)
	EVT_LEFT_UP(wxCartographer::on_left_up)
	EVT_MOUSE_CAPTURE_LOST(wxCartographer::on_capture_lost)
	EVT_MOTION(wxCartographer::on_mouse_move)
	EVT_MOUSEWHEEL(wxCartographer::on_mouse_wheel)
	//EVT_KEY_DOWN(wxCartographer::OnKeyDown)
END_EVENT_TABLE()

wxCartographer::wxCartographer(wxWindow *parent, const std::wstring &server_addr,
	const std::wstring &server_port, std::size_t cache_size,
	std::wstring cache_path, bool only_cache,
	const std::wstring &init_map, int init_z, double init_lat, double init_lon,
	on_paint_proc_t on_paint_proc,
	int anim_period, int def_min_anim_steps)
	: wxGLCanvas(parent, wxID_ANY, NULL /* attribs */,
		wxDefaultPosition, wxDefaultSize,
		wxFULL_REPAINT_ON_RESIZE)
	, gl_context_(this)
	, magic_id_(0)
	, load_texture_debug_counter_(0)
	, delete_texture_debug_counter_(0)
	, cache_path_( fs::system_complete(cache_path).string() )
	, only_cache_(only_cache)
	, cache_(cache_size)
	, cache_active_tiles_(0)
	, basis_map_id_(0)
	, basis_z_(0)
	, basis_tile_x1_(0)
	, basis_tile_y1_(0)
	, basis_tile_x2_(0)
	, basis_tile_y2_(0)
	, builder_debug_counter_(0)
	, file_iterator_(cache_.end())
	, file_loader_dbg_loop_(0)
	, file_loader_dbg_load_(0)
	, server_iterator_(cache_.end())
	, server_loader_dbg_loop_(0)
	, server_loader_dbg_load_(0)
	, anim_period_( posix_time::milliseconds(anim_period) )
	, def_min_anim_steps_(def_min_anim_steps)
	, anim_speed_(0)
	, anim_freq_(0)
	, animator_debug_counter_(0)
	, buffer_(100,100)
	, draw_tile_debug_counter_(0)
	, active_map_id_(0)
	, z_(init_z)
	, new_z_(z_)
	, z_step_(0)
	, fix_kx_(0.5)
	, fix_ky_(0.5)
	, fix_lat_(init_lat)
	, fix_lon_(init_lon)
	, painter_debug_counter_(0)
	, move_mode_(false)
	, force_repaint_(false)
	, on_paint_handler_(on_paint_proc)
{
	try
	{
		magic_init();

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
					my::ip::punycode_encode(server_addr),
					my::ip::punycode_encode(server_port));
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
					map.ext = L".jpg";
				else if (map.tile_type == L"image/png")
					map.ext = L".png";
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
		file_loader_ = new_worker(L"file_loader"); /* Название - только для отладки */
		boost::thread( boost::bind(
			&wxCartographer::file_loader_proc, this, file_loader_) );

		/* Запускаем серверный загрузчик тайлов */
		if (!only_cache_)
		{
			server_loader_ = new_worker(L"server_loader"); /* Название - только для отладки */
			boost::thread( boost::bind(
				&wxCartographer::server_loader_proc, this, server_loader_) );
		}

		/* Запускаем анимацию */
		if (anim_period)
		{
			/* Чтобы при расчёте средних скорости и частоты анимации данные
				не скакали слишком быстро, будем хранить 10 последних расчётов
				и вычислять общее среднее */
			for (int i = 0; i < 10; i++)
			{
				anim_speed_sw_.push();
				anim_freq_sw_.push();
			}
			animator_ = new_worker(L"animator");
			boost::thread( boost::bind(
				&wxCartographer::anim_thread_proc, this, animator_) );
		}
	}
	catch(std::exception &e)
	{
		throw my::exception(L"Ошибка создания Картографа")
			<< my::param(L"serverAddr", server_addr)
			<< my::param(L"serverPort", server_port)
			<< my::exception(e);
	}

	refresh();
}

wxCartographer::~wxCartographer()
{
	if (!finish())
		stop();
}

void wxCartographer::stop()
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
	debug_wait_for_finish(L"wxCartographer", posix_time::seconds(5));
	#endif

	wait_for_finish();

	cache_.clear();
	delete_textures();
	magic_deinit();

	assert( load_texture_debug_counter_ == delete_texture_debug_counter_);
}

void wxCartographer::magic_init()
{
	unsigned char magic_data[4] = {255, 255, 255, 255};
	
	SetCurrent(gl_context_);

	glGenTextures(1, &magic_id_);

	glBindTexture(GL_TEXTURE_2D, magic_id_);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
		0, GL_RGBA, GL_UNSIGNED_BYTE, magic_data);

	check_gl_error();
}

void wxCartographer::magic_deinit()
{
	glDeleteTextures(1, &magic_id_);
	check_gl_error();
}

void wxCartographer::magic_exec()
{
	/* Замечено, что ко всем отрисовываемым объектам примешивается цвет
		последней выведенной точки последней выведенной текстуры.
		Избавиться не удалось, поэтому делаем ход конём - выводим
		в никуда белую текстуру */
	
	glColor4d(1.0, 1.0, 1.0, 0.0);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, magic_id_);
	glBegin(GL_QUADS);
		glTexCoord2i(0, 0); glVertex3i( 0,  0, 0);
		glTexCoord2i(1, 0); glVertex3i(-1,  0, 0);
		glTexCoord2i(1, 0); glVertex3i(-1, -1, 0);
		glTexCoord2i(0, 1); glVertex3i( 0, -1, 0);
	glEnd();
}

void wxCartographer::check_gl_error()
{
	GLenum errLast = GL_NO_ERROR;

	for ( ;; )
	{
		GLenum err = glGetError();
		if ( err == GL_NO_ERROR )
			return;

		// normally the error is reset by the call to glGetError() but if
		// glGetError() itself returns an error, we risk looping forever here
		// so check that we get a different error than the last time
		if ( err == errLast )
			throw my::exception(L"OpenGL error state couldn't be reset");

		errLast = err;
		//throw my::exception(L"OpenGL error %1%") % err;
		assert(err == GL_NO_ERROR);
	}
}

GLuint wxCartographer::load_texture(raw_image &image)
{
	++load_texture_debug_counter_;
	return load_raw_to_gl(image);
}

void wxCartographer::load_textures()
{
	my::shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );
	
	tiles_cache::iterator iter = cache_.begin();

	int count = 0;

	while (iter != cache_.end() /*&& ++count <= cache_active_tiles_*/)
	{
		tile::ptr tile_ptr = iter->value();

		if (tile_ptr->state() == tile::texture_generating)
		{
			GLuint id = load_texture(tile_ptr->image());
			tile_ptr->set_texture_id(id);
		}

		++iter;
	}
}

void wxCartographer::post_delete_texture(GLuint texture_id)
{
	my::locker locker( MYLOCKERPARAMS(delete_texture_mutex_, 5, MYCURLINE) );
	delete_texture_queue_.push_back(texture_id);
}

void wxCartographer::delete_texture(GLuint texture_id)
{
	++delete_texture_debug_counter_;
	unload_from_gl(texture_id);
}

void wxCartographer::delete_textures()
{
	my::locker locker( MYLOCKERPARAMS(delete_texture_mutex_, 5, MYCURLINE) );
		
	while (delete_texture_queue_.size())
	{
		GLuint texture_id = delete_texture_queue_.front();
		delete_texture_queue_.pop_front();
		delete_texture(texture_id);
	}
}

bool wxCartographer::check_tile_id(const tile::id &tile_id)
{
	int sz = size_for_z_i(tile_id.z);

	return tile_id.z >= 1
		&& tile_id.x >= 0 && tile_id.x < sz
		&& tile_id.y >= 0 && tile_id.y < sz;
}

wxCartographer::tile::ptr wxCartographer::find_tile(const tile::id &tile_id)
{
	/* Блокируем кэш для чтения */
	my::shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

void wxCartographer::paint_tile(const tile::id &tile_id, int level)
{
	int z = tile_id.z - level;
	
	if (z <= 0)
		return;

	tile::ptr tile_ptr = get_tile( tile::id(
		tile_id.map_id, z, tile_id.x >> level, tile_id.y >> level) );

	if (!tile_ptr || tile_ptr->state() != tile::ready)
		paint_tile(tile_id, level + 1);
	else
	{
		GLuint id = tile_ptr->texture_id();

		int mask = 0;
		double w = 1.0;

		for (int i = 0; i < level; ++i)
		{
			mask = (mask << 1) | 1;
			w /= 2.0;
		}

		double x = (tile_id.x & mask) * w;
		double y = (tile_id.y & mask) * w;

		glBindTexture(GL_TEXTURE_2D, id);
		glBegin(GL_QUADS);
			glTexCoord2d(x,     y    ); glVertex3i(tile_id.x,     tile_id.y,     0);
			glTexCoord2d(x + w, y    ); glVertex3i(tile_id.x + 1, tile_id.y,     0);
			glTexCoord2d(x + w, y + w); glVertex3i(tile_id.x + 1, tile_id.y + 1, 0);
			glTexCoord2d(x,     y + w); glVertex3i(tile_id.x,     tile_id.y + 1, 0);
		glEnd();

		check_gl_error();
			
		/* Рамка вокруг тайла, если родного нет */
		/*-
		if (level) {
			Gdiplus::Pen pen(Gdiplus::Color(160, 160, 160), 1);
			canvas->DrawRectangle(&pen, canvas_x, canvas_y, 255, 255);
		}
		-*/
	}
}

wxCartographer::tile::ptr wxCartographer::get_tile(const tile::id &tile_id)
{
	//if ( !check_tile_id(tile_id))
	//	return tile::ptr();

	tile::ptr tile_ptr = find_tile(tile_id);

	return tile_ptr;
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void wxCartographer::file_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile::id tile_id;
		tile::ptr tile_ptr;

		++file_loader_dbg_loop_;

		/* Ищем в кэше тайл, требующий загрузки */
		{
			my::shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );

			while (file_iterator_ != cache_.end())
			{
				tile_ptr = file_iterator_->value();

				if (tile_ptr->state() == tile::file_loading)
				{
					tile_id = file_iterator_->key();
					++file_iterator_;
					break;
				}

				++file_iterator_;
			}
		}

		/* Если нет такого - засыпаем */
		if (!tile_id)
		{
			sleep(this_worker);
			continue;
		}

		++file_loader_dbg_load_;

		/*-
		main_log << L"z=" << tile_id.z
			<< L" x=" << tile_id.x
			<< L" y=" << tile_id.y
			<< L" cache=" << cache_.size()
			<< main_log;
		-*/

		/* Загружаем тайл с диска */
		std::wstringstream path;

		wxCartographer::map &map = maps_[tile_id.map_id];

		path << cache_path_
			<< L"/" << map.sid
			<< L"/z" << tile_id.z
			<< L'/' << (tile_id.x >> 10)
			<< L"/x" << tile_id.x
			<< L'/' << (tile_id.y >> 10)
			<< L"/y" << tile_id.y;

		std::wstring filename = path.str() + map.ext;

		/* В любой момент наш тайл может быть вытеснен из кэша,
			не обращаем на это внимание, т.к. tile::ptr - это не что иное,
			как shared_ptr, т.е. мы можем быть уверены, что тайл хоть
			и "висит в воздухе", ожидая удаления, но он так и будет висеть,
			пока мы его не освободим */

		/* Если файла нет, устанавливаем метку, чтобы загружался с сервера */
		if (!fs::exists(filename))
		{
			/* Но только если нет файла-метки об отсутствии тайла и там */
			if ( !fs::exists(path.str() + L".tne") )
				tile_ptr->set_state(tile::server_loading);
			else
				tile_ptr->set_state(tile::absent);
		}
		else if (!tile_ptr->load_from_file(filename))
		{
			tile_ptr->set_state(tile::fail);
			main_log << L"Ошибка загрузки wxImage: " << filename << main_log;
		}

	} /* while (!finish()) */
}

/* Загрузчик тайлов с сервера. При пустой очереди - засыпает */
void wxCartographer::server_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile::id tile_id;
		tile::ptr tile_ptr;

		++server_loader_dbg_loop_;

		/* Ищем в кэше тайл, требующий загрузки */
		{
			my::shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );

			while (server_iterator_ != cache_.end())
			{
				tile_ptr = server_iterator_->value();

				if (tile_ptr->state() == tile::server_loading)
				{
					tile_id = server_iterator_->key();
					++server_iterator_;
					break;
				}

				/* Не даём обогнать загрузчик файлов */
				if (tile_ptr->state() == tile::file_loading)
					break;

				++server_iterator_;
			}
		}

		/* Если нет такого - засыпаем */
		if (!tile_id)
		{
			sleep(this_worker);
			continue;
		}

		++server_loader_dbg_load_;

		/* Загружаем тайл с сервера */

		wxCartographer::map &map = maps_[tile_id.map_id];

		std::wstringstream path; /* Путь к локальному файлу  */
		path << cache_path_
			<< L"/" << map.sid
			<< L"/z" << tile_id.z
			<< L'/' << (tile_id.x >> 10)
			<< L"/x" << tile_id.x
			<< L'/' << (tile_id.y >> 10)
			<< L"/y" << tile_id.y;

		std::wstringstream request;
		request << L"/maps/gettile?map=" << map.sid
			<< L"&z=" << tile_id.z
			<< L"&x=" << tile_id.x
			<< L"&y=" << tile_id.y;

		/* В любой момент наш тайл может быть вытеснен из кэша,
			не обращаем на это внимание, т.к. tile::ptr - это не что иное,
			как shared_ptr, т.е. мы можем быть уверены, что тайл хоть
			и "висит в воздухе", ожидая удаления, но он так и будет висеть,
			пока мы его не освободим */

		try
		{
			/* Загружаем тайл с сервера ... */
			my::http::reply reply;
			get(reply, request.str());

			if (reply.status_code == 404)
			{
				/* Тайла нет на сервере - создаём файл-метку */
				tile_ptr->set_state(tile::absent);
				reply.save(path.str() + L".tne");
			}
			else if (reply.status_code == 200)
			{
				/* При успешной загрузке с сервера, создаём тайл из буфера */
				if ( tile_ptr->load_from_mem(reply.body.c_str(), reply.body.size()) )
				{
					/* При успешной загрузке сохраняем файл на диске */
					reply.save(path.str() + map.ext);
				}
				else
				{
					tile_ptr->set_state(tile::fail);
					main_log << L"Ошибка загрузки wxImage: " << request.str() << main_log;
				}
	
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

		{
			my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

			if (z_step_)
			{
				z_ += (new_z_ - z_) / z_step_;
				--z_step_;
			}
		}

#if 0
		/* Мигание для "мигающих" объектов */
		flash_alpha_ += (flash_new_alpha_ - flash_alpha_) / flash_step_;
		if (--flash_step_ == 0)
		{
			flash_step_ = def_min_anim_steps_ ? def_min_anim_steps_ : 1;
			/* При выходе из паузы, меняем направление мигания */
			if ((flash_pause_ = !flash_pause_) == false)
				flash_new_alpha_ = (flash_new_alpha_ == 0 ? 1 : 0);
		}
#endif

		/* Ждём, пока отрисует прошлое состояние */
		{
			my::locker locker( MYLOCKERPARAMS(paint_mutex_, 5, MYCURLINE) );
		}

		//refresh();
		Refresh(false);

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

double wxCartographer::lon_to_tile_x(double lon, double z)
{
	return (lon + 180.0) * size_for_z_d(z) / 360.0;
}

double wxCartographer::lat_to_tile_y(double lat, double z,
	map::projection_t projection)
{
	double s = std::sin( lat / 180.0 * M_PI );
	double y;

	switch (projection)
	{
		case map::spheroid:
			y = (0.5 - atanh(s) / (2*M_PI)) * size_for_z_d(z);
			break;

		case map::ellipsoid:
			y = (0.5 - (atanh(s) - EXCT*atanh(EXCT*s)) / (2*M_PI)) * size_for_z_d(z);
			break;

		default:
			assert(projection == map::spheroid || projection == map::ellipsoid);
	}

	return y;
}

double wxCartographer::lon_to_scr_x(double lon, double z,
	double fix_lon, double fix_scr_x)
{
	double fix_tile_x = lon_to_tile_x(fix_lon, z);
	double tile_x = lon_to_tile_x(lon, z);
	return (tile_x - fix_tile_x) * 256.0 + fix_scr_x;
}

double wxCartographer::lat_to_scr_y(double lat, double z,
	map::projection_t projection, double fix_lat, double fix_scr_y)
{
	double fix_tile_y = lat_to_tile_y(fix_lat, z, projection);
	double tile_y = lat_to_tile_y(lat, z, projection);
	return (tile_y - fix_tile_y) * 256.0 + fix_scr_y;
}

double wxCartographer::tile_x_to_lon(double x, double z)
{
	return x / size_for_z_d(z) * 360.0 - 180.0;
}

double wxCartographer::tile_y_to_lat(double y, double z,
	map::projection_t projection)
{
	double lat;
	double sz = size_for_z_d(z);
	double tmp = std::atan( std::exp( (0.5 - y / sz) * (2 * M_PI) ) );

	switch (projection)
	{
		case map::spheroid:
			lat = tmp * 360.0 / M_PI - 90.0;
			break;

		case map::ellipsoid:
		{
			tmp = tmp * 2.0 - M_PI / 2.0;
			double yy = y - sz / 2.0;
			double tmp2;
			do
			{
				tmp2 = tmp;
				tmp = std::asin(1.0 - ((1.0 + std::sin(tmp))*std::pow(1.0-EXCT*std::sin(tmp),EXCT)) / (exp((2.0*yy)/-(sz/(2.0*M_PI)))*std::pow(1.0+EXCT*std::sin(tmp),EXCT)) );

			} while( std::abs(tmp - tmp2) > 0.00000001 );

			lat = tmp * 180.0 / M_PI;
		}
		break;

		default:
			assert(projection == map::spheroid || projection == map::ellipsoid);
	}

	return lat;
}

double wxCartographer::scr_x_to_lon(double x, double z,
	double fix_lon, double fix_scr_x)
{
	double fix_tile_x = lon_to_tile_x(fix_lon, z);
	return tile_x_to_lon( fix_tile_x + (x - fix_scr_x) / 256.0, z );
}

double wxCartographer::scr_y_to_lat(double y, double z,
	map::projection_t projection, double fix_lat, double fix_scr_y)
{
	double fix_tile_y = lat_to_tile_y(fix_lat, z, projection);
	return tile_y_to_lat( fix_tile_y + (y - fix_scr_y) / 256.0, z, projection );
}

#if 0
void wxCartographer::sort_queue(tiles_queue &queue, my::worker::ptr worker)
{
	tile::id fix_tile; /* Тайл в центре экрана */

	/* Копируем все нужные параметры, обеспечив блокировку */
	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

		fix_tile.map_id = active_map_id_;
		fix_tile.z = (int)(z_ + 0.5);
		fix_tile.x = (wxCoord)lon_to_tile_x(fix_lon_, (double)fix_tile.z);
		fix_tile.y = (wxCoord)lat_to_tile_y(fix_lat_, (double)fix_tile.z,
			maps_[fix_tile.map_id].projection);
	}

	sort_queue(queue, fix_tile, worker);
}

void wxCartographer::sort_queue(tiles_queue &queue,
	const tile::id &fix_tile, my::worker::ptr worker)
{
	if (worker)
	{
		my::locker locker( MYLOCKERPARAMS(worker->get_mutex(), 5, MYCURLINE) );

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
#endif

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

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"speed: %0.1f ms", anim_speed_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"freq: %0.1f ms", anim_freq_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"animator: %d", animator_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"textures loaded: %d", load_texture_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"textures deleted: %d", delete_texture_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"painter: %d", painter_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"draw_tile: %d", draw_tile_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"builder: %d", builder_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"file_loader: loop=%d load=%d", file_loader_dbg_loop_, file_loader_dbg_load_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"server_loader: loop=%d load=%d", server_loader_dbg_loop_, server_loader_dbg_load_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"z: %0.1f", z_);
	gc.DrawText(buf, x, y), y += 12;

	int d;
	int m;
	double s;
	TO_DEG(fix_lat_, &d, &m, &s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lat: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;

	TO_DEG(fix_lon_, &d, &m, &s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lon: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;
}

void wxCartographer::repaint(wxPaintDC &dc)
{
	my::locker locker1( MYLOCKERPARAMS(paint_mutex_, 5, MYCURLINE) );
	my::recursive_locker locker2( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	++painter_debug_counter_;

	/* Измеряем скорость выполнения функции */
	anim_speed_sw_.start();

	/* Размеры окна */
	int width_i, height_i;
	get_viewport_size(&width_i, &height_i);

	double width_d = (double)width_i;
	double height_d = (double)height_i;

	/* Активная карта */
	int map_id = active_map_id_;
	wxCartographer::map map = maps_[map_id];

	/* Текущий масштаб. При перемещениях
		между масштабами - масштаб верхнего слоя */
	int z_i = (int)z_;
	int basis_z = z_i;
	double dz = z_ - (double)z_i; /* "Расстояние" от верхнего слоя */
	double alpha = 1.0 - dz; /* Прозрачность верхнего слоя */


	/**
		Готовим буфер
	*/

	if (!buffer_.IsOk()
		|| buffer_.GetWidth() != width_i || buffer_.GetHeight() != height_i)
	{
		/* Вот такая хитрая комбинация в сравнении с
			buffer_.Create(width, height); ускоряет вывод:
			1) на чёрном экране (DrawRectangle) в 5 раз;
			2) на заполненном экране (DrawBitmap) в 2 раза. */
		wxImage image(width_i, height_i, false);
		image.InitAlpha();
		buffer_ = wxBitmap(image);

		//buffer_.Create(width, height);
	}


	/**
		Рассчитываем основание пирамиды выводимых тайлов
	*/

	/* "Тайловые" координаты fix-точки */
	double fix_tile_x_d = lon_to_tile_x(fix_lon_, z_i);
	double fix_tile_y_d = lat_to_tile_y(fix_lat_, z_i, map.projection);
	int fix_tile_x_i = (int)fix_tile_x_d;
	int fix_tile_y_i = (int)fix_tile_y_d;

	/* Экранные координаты fix-точки */
	double fix_scr_x = width_d * fix_kx_;
	double fix_scr_y = height_d * fix_ky_;

	/* Координаты его верхнего левого угла */
	int x = (int)(fix_scr_x - (fix_tile_x_d - (double)fix_tile_x_i) * 256.0 + 0.5);
	int y = (int)(fix_scr_y - (fix_tile_y_d - (double)fix_tile_y_i) * 256.0 + 0.5);

	/* Определяем начало основания (верхний левый угол) */
	int basis_tile_x1 = fix_tile_x_i;
	int basis_tile_y1 = fix_tile_y_i;

	while (x > 0)
		x -= 256, --basis_tile_x1;
	while (y > 0)
		y -= 256, --basis_tile_y1;

	/* Определяем конец основания (нижний правый угол) */
	int basis_tile_x2 = basis_tile_x1;
	int basis_tile_y2 = basis_tile_y1;

	while (x < width_i)
		x += 256, ++basis_tile_x2;
	while (y < height_i)
		y += 256, ++basis_tile_y2;

	/* Отсекаем выходы за пределы видимости */
	{
		if (basis_tile_x1 < 0)
			basis_tile_x1 = 0;
		if (basis_tile_y1 < 0)
			basis_tile_y1 = 0;
		if (basis_tile_x2 < 0)
			basis_tile_x2 = 0;
		if (basis_tile_y2 < 0)
			basis_tile_y2 = 0;

		int sz = size_for_z_i(basis_z);
		if (basis_tile_x1 > sz)
			basis_tile_x1 = sz;
		if (basis_tile_y1 > sz)
			basis_tile_y1 = sz;
		if (basis_tile_x2 > sz)
			basis_tile_x2 = sz;
		if (basis_tile_y2 > sz)
			basis_tile_y2 = sz;
	}

	/* Сохраняем границы верхнего слоя */
	int z_i_tile_x1 = basis_tile_x1;
	int z_i_tile_y1 = basis_tile_y1;
	int z_i_tile_x2 = basis_tile_x2;
	int z_i_tile_y2 = basis_tile_y2;

	/* При переходе между масштабами основанием будет нижний слой */
	//if (dz > 0.01)
	{
		basis_tile_x1 <<= 1;
		basis_tile_y1 <<= 1;
		basis_tile_x2 <<= 1;
		basis_tile_y2 <<= 1;
		++basis_z;
	}

	/* Если основание изменилось - перестраиваем пирамиду */
	if ( basis_map_id_ != map_id
		|| basis_z_ != basis_z
		|| basis_tile_x1_ != basis_tile_x1
		|| basis_tile_y1_ != basis_tile_y1
		|| basis_tile_x2_ != basis_tile_x2
		|| basis_tile_y2_ != basis_tile_y2 )
	{
		my::not_shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );

		int tiles_count = 0; /* Считаем кол-во тайлов в пирамиде */

		/* Сохраняем новое основание */
		basis_map_id_ = map_id;
		basis_z_ = basis_z;
		basis_tile_x1_ = basis_tile_x1;
		basis_tile_y1_ = basis_tile_y1;
		basis_tile_x2_ = basis_tile_x2;
		basis_tile_y2_ = basis_tile_y2;

		/* Добавляем новые тайлы */
		while (basis_z)
		{
			int xc = (basis_tile_x1 + basis_tile_x2) / 2;
			int yc = (basis_tile_y1 + basis_tile_y2) / 2;

			for (int tile_x = basis_tile_x1; tile_x < basis_tile_x2; ++tile_x)
			{
				for (int tile_y = basis_tile_y1; tile_y < basis_tile_y2; ++tile_y)
				{
					tile::id tile_id(active_map_id_, basis_z, tile_x, tile_y);
					tiles_cache::iterator iter = cache_.find(tile_id);
					
					if (iter == cache_.end())
						cache_[ tile_id ] = tile::ptr(new tile(*this, tile::file_loading));
					else
						cache_.up(tile_id);

					++tiles_count;
				}
			}

			--basis_z;
			basis_tile_x1 >>= 1;
			basis_tile_y1 >>= 1;
			basis_tile_x2 >>= 1;
			basis_tile_y2 >>= 1;
		}

		/* Сортируем */
		////

		/* Устанавливаем итераторы загрузчиков на начало. Будим их */
		file_iterator_ = server_iterator_ = cache_.begin();
		wake_up(file_loader_);
		wake_up(server_loader_);

		cache_active_tiles_ = tiles_count;
	}

	
	/**
		Рисуем
	*/

	/* Настройка OpenGL */
	{
		SetCurrent(gl_context_);

		glEnable(GL_TEXTURE_2D);
		glShadeModel(GL_SMOOTH);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClearDepth(1.0);

		glEnable(GL_BLEND);

		glViewport(0, 0, width_i, height_i);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Уровень GL_PROJECTION */
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		/* Зрителя помещаем в fix-точку */
		double w = width_d / 256.0;
		double h = height_d / 256.0;
		double x = -w * fix_kx_;
		double y = -h * fix_ky_;

		#ifdef TEST_FRUSTRUM
		glFrustum(x, x + w, -y - h, -y, 0.8, 3.0);
		#else
		glOrtho(x, x + w, -y - h, -y, -1.0, 2.0);
		#endif

		/* С вертикалью работаем по старинке - сверху вниз, а не снизу вверх */
		glScaled(1.0 + dz, -1.0 - dz, 1.0);
	}

	/* Выводим нижний слой */
	if (dz > 0.01)
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		/* Тайлы заднего фона меньше в два раза */
		glScaled(0.5, 0.5, 1.0);
		glTranslated(-2.0 * fix_tile_x_d, -2.0 * fix_tile_y_d, 0.0);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		/* Границы нижнего слоя в данном случае равны основанию пирамиды тайлов */
		for (int x = basis_tile_x1_; x < basis_tile_x2_; ++x)
			for (int y = basis_tile_y1_; y < basis_tile_y2_; ++y)
				paint_tile( tile::id(map_id, basis_z_, x, y) );
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslated(-fix_tile_x_d, -fix_tile_y_d, 0.0);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	for (int x = z_i_tile_x1; x < z_i_tile_x2; ++x)
		for (int y = z_i_tile_y1; y < z_i_tile_y2; ++y)
			paint_tile( tile::id(map_id, z_i, x, y) );

	/* Картинка пользователя */
	{
		magic_exec();

		glColor4d(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(0.0, width_d, -height_d, 0.0, -1.0, 2.0);
		glScaled(1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		//wxMemoryDC dc;
		//dc.SelectObject(buffer_);

		{
			wxGCDC gc(dc);

			/* Очищаем */
			//gc.SetBackground(*wxBLACK_BRUSH);
			//gc.Clear();

			if (on_paint_handler_)
				on_paint_handler_(gc, width_i, height_i);

			#ifndef NDEBUG
			//paint_debug_info(gc, width_i, height_i);
			#endif
		}

		//dc.SelectObject(wxNullBitmap);
	}

	//wxImage image = buffer_.ConvertToImage();

	/* Перерисовываем окно */
	//dc.DrawBitmap(buffer_, 0, 0);

	glFlush();
	SwapBuffers();
	check_gl_error();

	paint_debug_info(dc, width_i, height_i);

	/* Перестраиваем очереди загрузки тайлов. Чтобы загрузка
		начиналась с центра экрана, а не с краёв */
	//sort_queue(file_queue_, fix_tile, file_loader_);

	/* Серверную очередь тоже корректируем */
	//sort_queue(server_queue_, fix_tile, server_loader_);

	/* Удаляем текстуры, вышедшие из употребления */
	delete_textures();

	/* Загружаем текстуры из пирамиды, делаем это в конце функции,
		чтобы не тормозить отрисовку */
	load_textures();


	/* Измеряем скорость и частоту отрисовки:
		anim_speed - средняя скорость выполнения repaint()
		anim_freq - средняя частота запуска repaint() */
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
}

void wxCartographer::move_fix_to_scr_xy(double scr_x, double scr_y)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	double w, h;
	get_viewport_size(&w, &h);

	fix_kx_ = scr_x / w;
	fix_ky_ = scr_y / h;
}

void wxCartographer::set_fix_to_scr_xy(double scr_x, double scr_y)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	double w, h;
	get_viewport_size(&w, &h);

	fix_lat_ = scr_y_to_lat(scr_y, z_, maps_[active_map_id_].projection,
		fix_lat_, h * fix_ky_);
	fix_lon_ = scr_x_to_lon(scr_x, z_, fix_lon_, w * fix_kx_);

	move_fix_to_scr_xy(scr_x, scr_y);
}

void wxCartographer::on_paint(wxPaintEvent &event)
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
	refresh();
}

void wxCartographer::on_left_down(wxMouseEvent& event)
{
	SetFocus();

	set_fix_to_scr_xy( (double)event.GetX(), (double)event.GetY() );

	move_mode_ = true;

	#ifdef BOOST_WINDOWS
	CaptureMouse();
	#endif

	refresh();
}

void wxCartographer::on_left_up(wxMouseEvent& event)
{
	if (move_mode_)
	{
		double w, h;
		get_viewport_size(&w, &h);

		set_fix_to_scr_xy( w/2.0, h/2.0 );
		move_mode_ = false;

		#ifdef BOOST_WINDOWS
		ReleaseMouse();
		#endif

		refresh();
	}
}

void wxCartographer::on_capture_lost(wxMouseCaptureLostEvent& event)
{
	move_mode_ = false;
}

void wxCartographer::on_mouse_move(wxMouseEvent& event)
{
	if (move_mode_)
	{
		move_fix_to_scr_xy( (double)event.GetX(), (double)event.GetY() );
		refresh();
	}
}

void wxCartographer::on_mouse_wheel(wxMouseEvent& event)
{
	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

		int z = (int)new_z_ + event.GetWheelRotation() / event.GetWheelDelta();

		if (z < 1)
			z = 1;

		if (z > 30)
			z = 30.0;
		
		set_current_z(z);
	}

	refresh();
}

void wxCartographer::refresh()
{
	//Refresh(false);
}

void wxCartographer::get_maps(std::vector<wxCartographer::map> &maps)
{
	maps.clear();

	for (maps_list::iterator iter = maps_.begin();
		iter != maps_.end(); ++iter)
	{
		maps.push_back(iter->second);
	}
}

wxCartographer::map wxCartographer::get_active_map()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return maps_[active_map_id_];
}

bool wxCartographer::set_active_map(const std::wstring &map_name)
{
	int map_num_id = maps_name_to_id_[map_name];

	if (map_num_id)
	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

		active_map_id_ = map_num_id;

		refresh();

		return true;
	}

	return false;
}

wxCartographer::point wxCartographer::ll_to_xy(double lat, double lon)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	double w, h;
	get_viewport_size(&w, &h);

	point pt;
	pt.x = lon_to_scr_x(lon, z_, fix_lon_, w * fix_kx_);
	pt.y = lat_to_scr_y(lat, z_, maps_[active_map_id_].projection, fix_lat_, h * fix_ky_);

	return pt;
}

wxCartographer::point wxCartographer::xy_to_ll(double x, double y)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	double w, h;
	get_viewport_size(&w, &h);

	point pt;
	pt.lon = scr_x_to_lon(x, z_, fix_lon_, w * fix_kx_);
	pt.lat = scr_y_to_lat(y, z_, maps_[active_map_id_].projection, fix_lat_, h * fix_ky_);

	return pt;
}

double wxCartographer::get_current_z(void)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );
	return z_;
}

void wxCartographer::set_current_z(int z)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	new_z_ = z;
	z_step_ = def_min_anim_steps_ ? 2 * def_min_anim_steps_ : 1;

	refresh();
}

wxCartographer::point wxCartographer::get_fix_ll()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	point pt;
	pt.lat = fix_lat_;
	pt.lon = fix_lon_;

	return pt;
}

wxCartographer::point wxCartographer::get_fix_xy()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	double w, h;
	get_viewport_size(&w, &h);

	point pt;
	pt.x = w * fix_kx_;
	pt.y = h * fix_ky_;

	return pt;
}

void wxCartographer::move_to(int z, double lat, double lon)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	fix_lat_ = lat;
	fix_lon_ = lon;
	set_current_z(z);
}

bool wxCartographer::convert_to_raw(const wxImage &src, raw_image &dest)
{
	if (!src.IsOk())
		return false;

	unsigned char *src_rgb = src.GetData();
	unsigned char *src_a = src.GetAlpha();

	if (src_a)
		dest.create(src.GetWidth(), src.GetHeight(), 32, GL_RGBA);
	else
		dest.create(src.GetWidth(), src.GetHeight(), 24, GL_RGB);

	unsigned char *ptr = dest.data();
	unsigned char *end = dest.end();

	if (!src_a)
		memcpy(ptr, src_rgb, end - ptr);
	else
	{
		while (ptr != end)
		{
			*ptr++ = *src_rgb++;
			*ptr++ = *src_rgb++;
			*ptr++ = *src_rgb++;
			*ptr++ = *src_a++;
		}
	}

	return true;
}

bool wxCartographer::load_raw_from_file(const std::wstring &filename,
	raw_image &image)
{
	wxImage wx_image(filename);
	return convert_to_raw(wx_image, image);
}

bool wxCartographer::load_raw_from_mem(const void *data, std::size_t size,
	raw_image &image)
{
	wxImage wx_image;
	wxMemoryInputStream stream(data, size);
	return wx_image.LoadFile(stream, wxBITMAP_TYPE_ANY) &&
		convert_to_raw(wx_image, image);
}

GLuint wxCartographer::load_raw_to_gl(raw_image &image)
{
	GLuint id;

	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(),
		0, (GLenum)image.tag(), GL_UNSIGNED_BYTE, image.data());

	check_gl_error();

	return id;
}

void wxCartographer::unload_from_gl(GLuint texture_id)
{
	glDeleteTextures(1, &texture_id);
	check_gl_error();
}

bool wxCartographer::load_image(const std::wstring &filename, raw_image &image, bool clear)
{
	if (load_raw_from_file(filename, image))
	{
		GLuint id = load_raw_to_gl(image);
		image.clear(true);
		image.set_tag( (int)id );
		return id != 0;
	}

	return false;
}

void wxCartographer::unload_image(raw_image &image)
{
	GLuint id = image.tag();
	glDeleteTextures(1, &id);
	check_gl_error();
	image.set_tag(0);
}

void wxCartographer::draw_image(const raw_image &image, double x, double y, double w, double h)
{
	glBindTexture(GL_TEXTURE_2D, (GLuint)image.tag());
	glBegin(GL_QUADS);
		glTexCoord2d(0.0, 0.0); glVertex3d(x,     y,     0);
		glTexCoord2d(1.0, 0.0); glVertex3d(x + w, y,     0);
		glTexCoord2d(1.0, 1.0); glVertex3d(x + w, y + h, 0);
		glTexCoord2d(0.0, 1.0); glVertex3d(x,     y + h, 0);
	glEnd();

	magic_exec();

	check_gl_error();
}
