#include "Base.h"

#include <wchar.h> /* swprintf, wcschr */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

namespace cartographer
{

my::log log(L"cartographer.log", my::log::clean);

wxDEFINE_EVENT(MY_EVENT, wxCommandEvent);

BEGIN_EVENT_TABLE(Base, wxGLCanvas)
	EVT_COMMAND(wxID_ANY, MY_EVENT, Base::on_my_event)
	EVT_PAINT(Base::on_paint)
	EVT_ERASE_BACKGROUND(Base::on_erase_background)
	EVT_SIZE(Base::on_size)
	EVT_LEFT_DOWN(Base::on_left_down)
	EVT_LEFT_UP(Base::on_left_up)
	EVT_MOUSE_CAPTURE_LOST(Base::on_capture_lost)
	EVT_MOTION(Base::on_mouse_move)
	EVT_MOUSEWHEEL(Base::on_mouse_wheel)
	//EVT_KEY_DOWN(Base::OnKeyDown)
END_EVENT_TABLE()

Base::Base(wxWindow *parent, const std::wstring &server_addr,
	const std::wstring &init_map, std::size_t cache_size)
	: my::employer(L"Cartographer_employer")
	, wxGLCanvas(parent, wxID_ANY, NULL /* attribs */,
		wxDefaultPosition, wxDefaultSize,
		wxFULL_REPAINT_ON_RESIZE)
	, gl_context_(this)
	, magic_id_(0)
	, load_texture_debug_counter_(0)
	, MY_MUTEX_DEF(delete_texture_mutex_,true)
	, delete_texture_debug_counter_(0)
	, cache_path_( fs::system_complete(L"cache").string() )
	, cache_(cache_size)
	, cache_active_tiles_(0)
	, basis_map_id_(0)
	, basis_z_(0)
	, basis_tile_x1_(0)
	, basis_tile_y1_(0)
	, basis_tile_x2_(0)
	, basis_tile_y2_(0)
	, MY_MUTEX_DEF(cache_mutex_,false)
	, builder_debug_counter_(0)
	, file_iterator_(cache_.end())
	, file_loader_dbg_loop_(0)
	, file_loader_dbg_load_(0)
	, server_iterator_(cache_.end())
	, server_loader_dbg_loop_(0)
	, server_loader_dbg_load_(0)
	, anim_period_( posix_time::milliseconds(50) )
	, def_min_anim_steps_(5)
	, anim_speed_(0)
	, anim_freq_(0)
	, animator_debug_counter_(0)
	, draw_tile_debug_counter_(0)
	, MY_MUTEX_DEF(paint_mutex_,true)
	, MY_MUTEX_DEF(params_mutex_,true)
	, map_id_(0)
	, map_pr_(Unknown_Projection)
	, z_(1.0)
	, new_z_(z_)
	, z_step_(0)
	, center_pos_( ratio(0.5, 0.5) )
	, central_cross_step_(0)
	, central_cross_alpha_(0.0)
	, painter_debug_counter_(0)
	, move_mode_(false)
	, flash_alpha_(0.0)
	, flash_new_alpha_(0.0)
	, flash_pause_(true)
	, flash_step_(1)
	, paint_thread_id_( boost::this_thread::get_id() )
	, on_image_delete_( boost::bind(&Base::on_image_delete_proc, this, _1) )
{
	try
	{
		SetCurrent(gl_context_);

		magic_init();

		std::wstring request;
		std::wstring file;

		bool load_from_server = (server_addr != L"cache");

		/* Загружаем с сервера список доступных карт */
		try
		{
			request = L"/maps/maps.xml";
			file = cache_path_ + L"/maps.xml";

			/* Загружаем с сервера на диск (кэшируем) */
			if (load_from_server)
			{
				const wchar_t *cstr = server_addr.c_str();
				const wchar_t *delim = wcschr(cstr, L':');
				std::wstring addr;
				std::wstring port;

				if (delim)
				{
					addr = std::wstring(cstr, delim - cstr);
					port = std::wstring(delim + 1);
				}
				else
				{
					addr = server_addr.empty() ?
						std::wstring(L"127.0.0.1") : server_addr;
					port = L"27543";
				}

				/* Резолвим сервер */
				asio::ip::tcp::resolver resolver(io_service_);
				asio::ip::tcp::resolver::query query(
					my::ip::punycode_encode(addr),
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
				map_info map;
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

				if (projection == L"spheroid" || projection == L"Sphere_Mercator")
					map.pr = Sphere_Mercator;
				else if (projection == L"ellipsoid" || projection == L"WGS84_Mercator")
					map.pr = WGS84_Mercator;
				else
					throw my::exception(L"Неизвестный тип проекции")
						<< my::param(L"map", map.sid)
						<< my::param(L"projection", projection);

				/* Сохраняем описание карты в списке */
				int id = get_new_map_id(); /* новый идентификатор */
				maps_[id] = map;

				if (map_id_ == 0 || map.name == init_map)
				{
					map_id_ = id;
					map_pr_ = map.pr;
				}

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
			&Base::file_loader_proc, this, file_loader_) );

		/* Запускаем серверный загрузчик тайлов */
		if (load_from_server)
		{
			server_loader_ = new_worker(L"server_loader"); /* Название - только для отладки */
			boost::thread( boost::bind(
				&Base::server_loader_proc, this, server_loader_) );
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

		animator_ = new_worker(L"animator");
		boost::thread( boost::bind(
			&Base::anim_thread_proc, this, animator_) );
	}
	catch(std::exception &e)
	{
		throw my::exception(L"Ошибка создания Картографа")
			<< my::param(L"server_addr", server_addr)
			<< my::exception(e);
	}

	update();
}

Base::~Base()
{
	if (!finish())
		stop();

	cache_.clear();
	delete_textures();
	magic_deinit();

	/*TODO: assert не срабатывает! */
	//assert( load_texture_debug_counter_ == delete_texture_debug_counter_ );
}

void Base::stop()
{
	/* Оповещаем о завершении работы */
	lets_finish();

	/* Освобождаем ("увольняем") всех "работников" */
	dismiss(file_loader_);
	dismiss(server_loader_);
	dismiss(animator_);

	/* Ждём завершения */
	#ifndef NDEBUG
	debug_wait_for_finish(L"Cartographer", posix_time::seconds(5));
	#endif

	wait_for_finish();
}

void Base::update()
{
	//Refresh(false);
}

void Base::magic_init()
{
	unsigned char magic_data[4] = {255, 255, 255, 255};

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

void Base::magic_deinit()
{
	glDeleteTextures(1, &magic_id_);
	check_gl_error();
}

void Base::magic_exec()
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

void Base::check_gl_error()
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

void Base::load_textures()
{
	shared_lock<shared_mutex> lock(cache_mutex_);

	tiles_cache::iterator iter = cache_.begin();

	int count = 0;

	while (iter != cache_.end() && ++count <= cache_active_tiles_)
	{
		tile::ptr tile_ptr = iter->value();

		if (tile_ptr->ok())
		{
			tile_ptr->convert_to_gl_texture();
			check_gl_error();
			++load_texture_debug_counter_;
		}

		++iter;
	}
}

void Base::delete_texture_later(GLuint texture_id)
{
	if (boost::this_thread::get_id() == paint_thread_id_)
	{
		delete_texture(texture_id);
	}
	else
	{
		unique_lock<mutex> lock(delete_texture_mutex_);
		delete_texture_queue_.push_back(texture_id);
	}
}

void Base::delete_texture(GLuint texture_id)
{
	glDeleteTextures(1, &texture_id);
	check_gl_error();
	++delete_texture_debug_counter_;
}

void Base::delete_textures()
{
	unique_lock<mutex> lock(delete_texture_mutex_);

	while (delete_texture_queue_.size())
	{
		GLuint texture_id = delete_texture_queue_.front();
		delete_texture_queue_.pop_front();
		delete_texture(texture_id);
	}
}

bool Base::check_tile_id(const tile::id &tile_id)
{
	int sz = tiles_count(tile_id.z);

	return tile_id.z >= 1
		&& tile_id.x >= 0 && tile_id.x < sz
		&& tile_id.y >= 0 && tile_id.y < sz;
}

tile::ptr Base::find_tile(const tile::id &tile_id)
{
	/* Блокируем кэш для чтения */
	shared_lock<shared_mutex> lock(cache_mutex_);

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

void Base::paint_tile(const tile::id &tile_id, int level)
{
	int z = tile_id.z - level;

	if (z <= 0)
		return;

	tile::ptr tile_ptr = get_tile( tile::id(
		tile_id.map_id, z, tile_id.x >> level, tile_id.y >> level) );

	GLuint texture_id = tile_ptr ? tile_ptr->texture_id() : 0;

	if (texture_id == 0)
		paint_tile(tile_id, level + 1);
	else
	{
		int mask = 0;
		int kw = 1;

		for (int i = 0; i < level; ++i)
		{
			mask = (mask << 1) | 1;
			kw <<= 1;
		}

		double w = 1.0 / (double)kw;
		double x = (tile_id.x & mask) * w;
		double y = (tile_id.y & mask) * w;

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glBegin(GL_QUADS);
			glTexCoord2d( x, y );
			glVertex3d( (double)tile_id.x, (double)tile_id.y, 0.0 );
			glTexCoord2d( x + w, y );
			glVertex3d( (double)tile_id.x + 1.0, (double)tile_id.y, 0.0 );
			glTexCoord2d( x + w, y + w );
			glVertex3d( (double)tile_id.x + 1.0, (double)tile_id.y + 1.0, 0.0 );
			glTexCoord2d( x, y + w);
			glVertex3d( (double)tile_id.x, (double)tile_id.y + 1.0, 0.0 );
		glEnd();

		/*-
		magic_exec();

		glColor3d(1.0, 1.0, 1.0);
		glBegin(GL_LINE_LOOP);
			glVertex3d( (double)tile_id.x, (double)tile_id.y, 0.0 );
			glVertex3d( (double)tile_id.x + 1.0, (double)tile_id.y, 0.0 );
			glVertex3d( (double)tile_id.x + 1.0, (double)tile_id.y + 1.0, 0.0 );
			glVertex3d( (double)tile_id.x, (double)tile_id.y + 1.0, 0.0 );
		glEnd();
		-*/

		check_gl_error();
	}
}

tile::ptr Base::get_tile(const tile::id &tile_id)
{
	//if ( !check_tile_id(tile_id))
	//	return tile::ptr();

	tile::ptr tile_ptr = find_tile(tile_id);

	return tile_ptr;
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void Base::file_loader_proc(my::worker::ptr this_worker)
{
	MY_REGISTER_THREAD(L"cartographer::file_loader");

	while (!finish())
	{
		tile::id tile_id;
		tile::ptr tile_ptr;

		++file_loader_dbg_loop_;

		/* Ищем в кэше тайл, требующий загрузки */
		{
			shared_lock<shared_mutex> lock(cache_mutex_);

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

		/* Загружаем тайл с диска */
		std::wstringstream path;

		map_info &map = maps_[tile_id.map_id];

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

		/* Если файла нет на диске, загружаем с сервера */
		if (!fs::exists(filename))
		{
			/* Но только если нет файла-метки об отсутствии тайла и там */
			if ( fs::exists(path.str() + L".tne") )
				tile_ptr->set_state(tile::ready);
			else
				tile_ptr->set_state(tile::server_loading);
		}
		else
		{
			if (!tile_ptr->load_from_file(filename))
			{
				tile_ptr->set_state(tile::ready);
				log << L"Ошибка загрузки wxImage: " << filename << log;
			}
		}

	} /* while (!finish()) */
}

/* Загрузчик тайлов с сервера. При пустой очереди - засыпает */
void Base::server_loader_proc(my::worker::ptr this_worker)
{
	MY_REGISTER_THREAD(L"cartographer::server_loader");

	while (!finish())
	{
		tile::id tile_id;
		tile::ptr tile_ptr;

		++server_loader_dbg_loop_;

		/* Ищем в кэше тайл, требующий загрузки */
		{
			shared_lock<shared_mutex> lock(cache_mutex_);

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

		map_info &map = maps_[tile_id.map_id];

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
				tile_ptr->set_state(image::ready);
				reply.save(path.str() + L".tne");
			}
			else if (reply.status_code == 200)
			{
				/* При успешной загрузке с сервера, создаём тайл из буфера
					и сохраняем файл на диске */
				if ( tile_ptr->load_from_mem(reply.body.c_str(), reply.body.size()) )
					reply.save(path.str() + map.ext);
				else
				{
					tile_ptr->set_state(tile::ready);
					log << L"Ошибка загрузки wxImage: " << request.str() << log;
				}
			}
		}
		catch (...)
		{
			/* Игнорируем любые ошибки связи */
		}

	} /* while (!finish()) */
}

void Base::anim_thread_proc(my::worker::ptr this_worker)
{
	MY_REGISTER_THREAD(L"cartographer::animator");

	asio::io_service io_service;
	asio::deadline_timer timer(io_service, my::time::utc_now());

	while (!finish())
	{
		++animator_debug_counter_;

		log << L"cartographer::anim_thread_proc()" << log;

		{
			unique_lock<recursive_mutex> lock(params_mutex_);

			if (z_step_)
			{
				z_ += (new_z_ - z_) / z_step_;
				--z_step_;
			}
		}

		/* Мигание для "мигающих" объектов */
		flash_alpha_ += (flash_new_alpha_ - flash_alpha_) / flash_step_;
		if (--flash_step_ == 0)
		{
			flash_step_ = def_min_anim_steps_ ? def_min_anim_steps_ : 1;
			/* При выходе из паузы, меняем направление мигания */
			if ((flash_pause_ = !flash_pause_) == false)
				flash_new_alpha_ = (flash_new_alpha_ == 0 ? 1 : 0);
		}

		//Refresh(false);
		//Update();

		{
			/* Без этой блокировки случалось так, что отрисовка выполнялась
				быстрее, чем поток успел дойти до sleep(): repaint() будил
				ещё не заснувший поток, после чего animator спокойно засыпал,
				но уже навечно */
			unique_lock<mutex> lock(this_worker->get_mutex());

			log << L"send_my_event(MY_ID_REPAINT)" << log;
			send_my_event(MY_ID_REPAINT);

			log << L"sleep(this_worker)" << log;
			sleep(this_worker, lock);
			log << L"wake_up(this_worker)" << log;
		}

		boost::posix_time::ptime time = timer.expires_at() + anim_period_;
		boost::posix_time::ptime now = my::time::utc_now();

		/* Теоретически время следующей прорисовки должно быть относительным
			от времени предыдущей, но на практике могут возникнуть торможения,
			и, тогда, программа будет пытаться запустить прорисовку в прошлом.
			В этом случае следующий запуск делаем относительно текущего времени */
		if (now > time)
		{
			log << L"without timer" << log;
			timer.expires_at(now);
		}
		else
		{
			log << L"timer.expires_at("
				<< my::time::to_wstring(time) << L')' << log;
			timer.expires_at(time);
			timer.wait();
		}
	}
}

void Base::get(my::http::reply &reply,
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

unsigned int Base::load_and_save(const std::wstring &request,
	const std::wstring &local_filename)
{
	my::http::reply reply;

	get(reply, request);

	if (reply.status_code == 200)
		reply.save(local_filename);

	return reply.status_code;
}

unsigned int Base::load_and_save_xml(const std::wstring &request,
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

#if 0
template<class DC>
void Base::paint_debug_info_int(DC &gc, int width, int height)
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
	DDToDMS(fix_lat_, &d, &m, &s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lat: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;

	DDToDMS(fix_lon_, &d, &m, &s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lon: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;
}
#endif

void Base::repaint()
{
	log << L"repaint()" << log;

	unique_lock<mutex> l1(paint_mutex_);
	unique_lock<recursive_mutex> l2(params_mutex_);

	++painter_debug_counter_;

	/* Измеряем скорость выполнения функции */
	anim_speed_sw_.start();

	/* Размеры окна + обновляем размеры экрана для центральной точки */
	const size screen_size = get_screen_size();
	center_pos_.set_size(screen_size);

	/* Текущий масштаб. При перемещениях
		между масштабами - масштаб верхнего слоя */
	const int z_i = (int)z_;
	const double dz = z_ - (double)z_i; /* "Расстояние" от верхнего слоя */
	const double alpha = 1.0 - dz; /* Прозрачность верхнего слоя */
	int basis_z = z_i;

	/* Центральный тайл */
	const point central_tile = screen_pos_.get_tiles_pos(map_pr_, z_i);

	/* Позиции экрана и его центра */
	const point screen_pos = screen_pos_.get_world_pos(map_pr_);
	const point center_pos = center_pos_.get_pos();

	/**
		Рассчитываем основание пирамиды выводимых тайлов
	*/
	int z_i_tile_x1;
	int z_i_tile_y1;
	int z_i_tile_x2;
	int z_i_tile_y2;

	{
		const int central_tile_x_i = (int)central_tile.x;
		const int central_tile_y_i = (int)central_tile.y;

		/* Координаты его верхнего левого угла */
		point tile_corner = center_pos
			- (central_tile - point(central_tile_x_i, central_tile_y_i)) * 256.0;

		/* Определяем начало основания (верхний левый угол) */
		int basis_tile_x1 = central_tile_x_i;
		int basis_tile_y1 = central_tile_y_i;

		while (tile_corner.x > 0.0)
			tile_corner.x -= 256.0, --basis_tile_x1;
		while (tile_corner.y > 0.0)
			tile_corner.y -= 256.0, --basis_tile_y1;

		/* Определяем конец основания (нижний правый угол) */
		int basis_tile_x2 = basis_tile_x1;
		int basis_tile_y2 = basis_tile_y1;

		while (tile_corner.x < screen_size.width)
			tile_corner.x += 256.0, ++basis_tile_x2;
		while (tile_corner.y < screen_size.height)
			tile_corner.y += 256.0, ++basis_tile_y2;

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

			int sz = tiles_count(basis_z);
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
		z_i_tile_x1 = basis_tile_x1;
		z_i_tile_y1 = basis_tile_y1;
		z_i_tile_x2 = basis_tile_x2;
		z_i_tile_y2 = basis_tile_y2;

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
		if ( basis_map_id_ != map_id_
			|| basis_z_ != basis_z
			|| basis_tile_x1_ != basis_tile_x1
			|| basis_tile_y1_ != basis_tile_y1
			|| basis_tile_x2_ != basis_tile_x2
			|| basis_tile_y2_ != basis_tile_y2 )
		{
			unique_lock<shared_mutex> lock(cache_mutex_);

			int tiles_count = 0; /* Считаем кол-во тайлов в пирамиде */

			/* Сохраняем новое основание */
			basis_map_id_ = map_id_;
			basis_z_ = basis_z;
			basis_tile_x1_ = basis_tile_x1;
			basis_tile_y1_ = basis_tile_y1;
			basis_tile_x2_ = basis_tile_x2;
			basis_tile_y2_ = basis_tile_y2;

			/* Добавляем новые тайлы */
			while (basis_z)
			{
				for (int tile_x = basis_tile_x1; tile_x < basis_tile_x2; ++tile_x)
				{
					for (int tile_y = basis_tile_y1; tile_y < basis_tile_y2; ++tile_y)
					{
						tile::id tile_id(map_id_, basis_z, tile_x, tile_y);
						tiles_cache::iterator iter = cache_.find(tile_id);

						tile::ptr tile_ptr;

						if (iter != cache_.end())
							tile_ptr = iter->value();
						else
						{
							tile_ptr = tile::ptr( new tile(on_image_delete_) );
							tile_ptr->set_state(tile::file_loading);
						}

						cache_.insert(tile_id, tile_ptr);

						++tiles_count;
					}
				}

				--basis_z;

				basis_tile_x1 >>= 1;
				basis_tile_y1 >>= 1;

				if (basis_tile_x2 & 1)
					++basis_tile_x2;
				basis_tile_x2 >>= 1;

				if (basis_tile_y2 & 1)
					++basis_tile_y2;
				basis_tile_y2 >>= 1;
			}

			/*TODO: Сортируем */
			////

			/* Устанавливаем итераторы загрузчиков на начало. Будим их */
			file_iterator_ = server_iterator_ = cache_.begin();
			wake_up(file_loader_);
			wake_up(server_loader_);

			cache_active_tiles_ = tiles_count;

		}
	}

	/**
		Рисуем
	*/

	/* Настройка OpenGL */
	{
		SetCurrent(gl_context_);

		glEnable(GL_TEXTURE_2D);
		glShadeModel(GL_SMOOTH);
		glClearColor(0.0f, 0.2f, 0.5f, 1.0f);
		glClearDepth(1.0);
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);

		glViewport(0, 0, screen_size.width, screen_size.height);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Уровень GL_PROJECTION */
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		/* Зрителя помещаем в центральную точку (тайловые координаты) */
		size sz = screen_size / 256.0;
		point pos = -center_pos_.get_pos_for(sz);

		#ifdef TEST_FRUSTRUM
		glFrustum( pos.x, pos.x + sz.width,
			-pos.y - sz.height, -pos.y, 0.8, 3.0 );
		#else
		glOrtho( pos.x, pos.x + sz.width,
			-pos.y - sz.height, -pos.y, -1.0, 2.0 );
		#endif

		/* С вертикалью работаем сверху вниз, а не снизу вверх */
		glScaled(1.0 + dz, -1.0 - dz, 1.0);
	}

	/* Выводим нижний слой */
	if (dz > 0.01)
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		/* Тайлы заднего фона меньше в два раза */
		glScaled(0.5, 0.5, 1.0);
		glTranslated(-2.0 * central_tile.x, -2.0 * central_tile.y, 0.0);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		/* Границы нижнего слоя в данном случае равны основанию пирамиды тайлов */
		for (int x = basis_tile_x1_; x < basis_tile_x2_; ++x)
			for (int y = basis_tile_y1_; y < basis_tile_y2_; ++y)
				paint_tile( tile::id(map_id_, basis_z_, x, y) );
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslated(-central_tile.x, -central_tile.y, 0.0);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	for (int x = z_i_tile_x1; x < z_i_tile_x2; ++x)
		for (int y = z_i_tile_y1; y < z_i_tile_y2; ++y)
			paint_tile( tile::id(map_id_, z_i, x, y) );

	log << L"repaint() after paint map" << log;

	/* Меняем проекцию на проекцию экрана */
	{
		glColor4d(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(0.0, screen_size.width, -screen_size.height, 0.0, -1.0, 2.0);
		glScaled(1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	magic_exec();

	log << L"repaint() before on_paint" << log;

	/* Картинка пользователя */
	if (on_paint_handler_)
	{
		on_paint_handler_(z_, screen_size);
		magic_exec();
	}

	log << L"repaint() after on_paint" << log;

	/* Показываем fix-точку при изменении масштаба */
	if (central_cross_step_ || dz > 0.1)
	{
		if (central_cross_step_ == 0)
		{
			central_cross_step_ = def_min_anim_steps_ ? 2 * def_min_anim_steps_ : 1;
			central_cross_alpha_ = 1.0;
		}
		else
		{
			central_cross_alpha_ -= central_cross_alpha_ / central_cross_step_;
			--central_cross_step_;
		}

		glLineWidth(3);
		glColor4d( 1.0, 0.0, 0.0, central_cross_alpha_ );

		glBegin(GL_LINES);
			glVertex3d( center_pos.x - 8, center_pos.y - 8, 0.0 );
			glVertex3d( center_pos.x + 8, center_pos.y + 8, 0.0 );
			glVertex3d( center_pos.x - 8, center_pos.y + 8, 0.0 );
			glVertex3d( center_pos.x + 8, center_pos.y - 8, 0.0 );
		glEnd();

		check_gl_error();
	}

	log << L"repaint() before after_paint()" << log;

	after_repaint(screen_size);

	log << L"repaint() after after_paint()" << log;

	glFlush();
	SwapBuffers();
	check_gl_error();

	log << L"repaint() after SwapBuffers()" << log;

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

	log << L"wake_up(animator)" << log;
	wake_up(animator_);

	log << L"~ repaint()" << log;
}

size Base::get_screen_size()
{
	wxCoord w, h;
	GetClientSize(&w, &h);
	return size( (double)w, (double)h);
}

point Base::get_screen_max_point()
{
	wxCoord w, h;
	GetClientSize(&w, &h);
	return point( (double)w, (double)h);
}

void Base::move_screen_to(const point &pos)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	center_pos_.set_pos(pos);
}

void Base::set_screen_pos(const point &pos)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	screen_pos_ = screen_to_coord(
		pos, map_pr_, z_, screen_pos_.get_world_pos(map_pr_),
		center_pos_.get_pos() );

	/* move_screen_to() */
	center_pos_.set_pos(pos);
}

void Base::set_z(int z)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	if (z < 1)
		z = 1;

	if (z > 19)
		z = 19;

	new_z_ = z;
	z_step_ = def_min_anim_steps_ ? 2 * def_min_anim_steps_ : 1;

	update();
}

void Base::send_my_event(int cmd_id)
{
    wxCommandEvent event(MY_EVENT);
    event.SetInt(cmd_id);
	AddPendingEvent(event);
}

void Base::on_my_event(wxCommandEvent& event)
{
	switch (event.GetInt())
	{
		case MY_ID_REPAINT:
			repaint();
			break;

		default:
			break;
	}
}

void Base::on_paint(wxPaintEvent &event)
{
	wxPaintDC dc(this);

	repaint();

	event.Skip(false);
}

void Base::on_erase_background(wxEraseEvent& event)
{
	event.Skip(false);
}

void Base::on_size(wxSizeEvent& event)
{
	update();
}

void Base::on_left_down(wxMouseEvent& event)
{
	SetFocus();

	set_screen_pos( point(event.GetX(), event.GetY()) );

	move_mode_ = true;

	#ifdef BOOST_WINDOWS
	CaptureMouse();
	#endif

	update();

	event.Skip(true);
}

void Base::on_left_up(wxMouseEvent& event)
{
	if (move_mode_)
	{
		//set_screen_pos( point( get_screen_size() / 2.0 ) );
		move_mode_ = false;

		#ifdef BOOST_WINDOWS
		ReleaseMouse();
		#endif

		update();
	}

	event.Skip(true);
}

void Base::on_capture_lost(wxMouseCaptureLostEvent& event)
{
	move_mode_ = false;
}

void Base::on_mouse_move(wxMouseEvent& event)
{
	mouse_pos_.x = (double)event.GetX();
	mouse_pos_.y = (double)event.GetY();

	if (move_mode_)
	{
		move_screen_to(mouse_pos_);
		update();
	}

	event.Skip(true);
}

void Base::on_mouse_wheel(wxMouseEvent& event)
{
	{
		unique_lock<recursive_mutex> lock(params_mutex_);

		int z = (int)new_z_ + event.GetWheelRotation() / event.GetWheelDelta();

		if (z < 1)
			z = 1;

		if (z > 30)
			z = 30.0;

		set_z(z);
	}

	update();

	event.Skip(true);
}

void Base::on_image_delete_proc(image &img)
{
	GLuint texture_id = img.texture_id();
	if (texture_id)
		delete_texture_later(texture_id);
}

} /* namespace cartographer */
