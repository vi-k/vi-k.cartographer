//#define TEST_FRUSTRUM

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

static void CheckGLError();

raw_image* wxCartographer::convert_to_raw(const wxImage &src)
{
	raw_image *dest = new raw_image();

	unsigned char *rgb = src.GetData();
	unsigned char *alpha = src.GetAlpha();

	if (alpha)
		dest = new raw_image(src.GetWidth(), src.GetHeight(), 32, GL_RGBA);
	else
		dest = new raw_image(src.GetWidth(), src.GetHeight(), 24, GL_RGB);

	unsigned char *ptr = dest->data();
	unsigned char *end = dest->end();

	if (!alpha)
		memcpy(ptr, rgb, end - ptr);
	else
	{
		while (ptr != end)
		{
    		*ptr++ = *rgb++;
    		*ptr++ = *rgb++;
    		*ptr++ = *rgb++;
    		*ptr++ = *alpha++;
		}
	}

	return dest;
}

void wxCartographer::post_load_texture(tile::ptr tile_ptr)
{
    /* Сообщение основному потоку о загрузке текстуры */
	wxCommandEvent *event = new wxCommandEvent(WXCART_LOAD_TEXTURE_EVENT);
	tile::ptr *tile_ptr_ptr = new tile::ptr(tile_ptr);
	event->SetClientData( (void*)tile_ptr_ptr );
	QueueEvent(event);
}

void wxCartographer::on_load_texture(wxCommandEvent& event)
{
	/* Обработка сообщения о загрузке текстуры */
	tile::ptr *tile_ptr_ptr = (tile::ptr*)event.GetClientData();

	++texturer_debug_counter_;

	tile::ptr tile_ptr = *tile_ptr_ptr;
	delete tile_ptr_ptr;
	
	if (tile_ptr && tile_ptr->texture_id() == 0)
	{
		raw_image *image = tile_ptr->image();
		GLuint texture_id = load_texture(image);
		tile_ptr->set_texture_id(texture_id);
				
		/* Если текстура загружена, то картинка нам больше не нужна */
		tile_ptr->reset_image();
	}
}

GLuint wxCartographer::load_texture(raw_image *image)
{
	GLuint id;

	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width(), image->height(),
		0, (GLint)image->type(), GL_UNSIGNED_BYTE, image->data());

	CheckGLError();

	return id;
}

void wxCartographer::post_delete_texture(GLuint texture_id)
{
	wxCommandEvent *event = new wxCommandEvent(WXCART_DELETE_TEXTURE_EVENT);
    event->SetInt(texture_id);
	QueueEvent(event);
}

void wxCartographer::on_delete_texture(wxCommandEvent& event)
{
	delete_texture( (GLuint)event.GetInt() );
}

void wxCartographer::delete_texture(GLuint texture_id)
{
	glDeleteTextures(1, &texture_id);
	CheckGLError();
}

void LoadImage(const char *filename, raw_image &image)
{
    wxImage wx_image(filename);

	if (!wx_image.IsOk())
    	throw "!IsOk()";

    unsigned char *rgb = wx_image.GetData();
    unsigned char *alpha = wx_image.GetAlpha();

	if (alpha)
		image.create(wx_image.GetWidth(), wx_image.GetHeight(), 32, GL_RGBA);
	else
		image.create(wx_image.GetWidth(), wx_image.GetHeight(), 24, GL_RGB);

    unsigned char *ptr = image.data();
    unsigned char *end = image.end();

    if (!alpha)
		memcpy(ptr, rgb, end - ptr);
	else
	{
		unsigned char *end2 = ptr + 4 * 2560;

		while (ptr != end2)
		{
    		*ptr++ = 0;
    		*ptr++ = 128;
    		*ptr++ = 255;
    		*ptr++ = 255;
		}

		while (ptr != end)
		{
    		*ptr++ = *rgb++;
    		*ptr++ = *rgb++;
    		*ptr++ = *rgb++;
    		*ptr++ = *alpha++;
		}
	}
}

static void CheckGLError()
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
        throw my::exception(L"OpenGL error %1%") % err;
    }
}

wxDEFINE_EVENT(WXCART_LOAD_TEXTURE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WXCART_DELETE_TEXTURE_EVENT, wxCommandEvent);

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
	EVT_COMMAND(wxID_ANY, WXCART_LOAD_TEXTURE_EVENT, wxCartographer::on_load_texture)
	EVT_COMMAND(wxID_ANY, WXCART_DELETE_TEXTURE_EVENT, wxCartographer::on_delete_texture)
	EVT_IDLE(wxCartographer::on_idle)
END_EVENT_TABLE()

wxCartographer::wxCartographer(wxWindow *parent, const std::wstring &serverAddr,
	const std::wstring &serverPort, std::size_t cacheSize,
	std::wstring cachePath, bool onlyCache,
	const std::wstring &initMap, int initZ, double initLat, double initLon,
	OnPaintProc_t onPaintProc,
	int animPeriod, int defMinAnimSteps)
	: wxGLCanvas(parent, wxID_ANY, NULL /* attribs */,
                 wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE)
	, gl_context_(this)
	, texturer_debug_counter_(0)
	, cache_path_( fs::system_complete(cachePath).string() )
	, only_cache_(onlyCache)
	, cache_(cacheSize)
	, builder_debug_counter_(0)
	, file_queue_(300)
	, file_loader_dbg_loop_(0)
	, file_loader_dbg_load_(0)
	, server_queue_(300)
	, server_loader_dbg_loop_(0)
	, server_loader_dbg_load_(0)
	, anim_period_( posix_time::milliseconds(animPeriod) )
	, def_min_anim_steps_(defMinAnimSteps)
	, anim_speed_(0)
	, anim_freq_(0)
	, animator_debug_counter_(0)
	, background1_()
	, background2_()
	, buffer_(100,100)
	, draw_tile_debug_counter_(0)
	, active_map_id_(0)
	, z_(initZ)
	, new_z_(z_)
	, z_step_(0)
	, fix_kx_(0.5)
	, fix_ky_(0.5)
	, fix_lat_(initLat)
	, fix_lon_(initLon)
	, painter_debug_counter_(0)
	, backgrounder_debug_counter_(0)
	, move_mode_(false)
	, force_repaint_(false)
	, on_paint_(onPaintProc)
	, on_idle_debug_counter_(0)
{
	try
	{
		init_gl();

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
			animator_ = new_worker(L"animator");
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

	Repaint();
}

void wxCartographer::init_gl()
{
	SetCurrent(gl_context_);

	#ifdef TEST_FRUSTRUM
	glEnable(GL_DEPTH_TEST);
	#endif

	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.2f, 0.5f, 1.0f);
	glClearDepth(1.0);

	glGenTextures(WXSIZEOF(m_textures), m_textures);

	for ( unsigned i = 0; i < WXSIZEOF(m_textures); i++ )
	{
		raw_image image;

		#define EXT ".jpg"

		switch (i)
		{
			case 0:
				LoadImage("y11357.png", image);
				break;
			case 1:
				LoadImage("y11340.png", image);
				break;
			case 2:
				LoadImage("z15x14338y5674" EXT, image);
				break;
			case 3:
				LoadImage("z15x14338y5675" EXT, image);
				break;
			case 4:
				LoadImage("z15x14339y5674" EXT, image);
				break;
			case 5:
				LoadImage("z15x14339y5675" EXT, image);
				break;
			case 6:
				LoadImage("z14x7169y2837.png", image);
				break;
		}
		//LoadImage(i == 0 ? "y22649.jpg" : "y22650.jpg", image);

        glBindTexture(GL_TEXTURE_2D, m_textures[i]);

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(),
			0, (GLint)image.type(), GL_UNSIGNED_BYTE, image.data());
    }

	glEnable(GL_BLEND);

    CheckGLError();
}

void wxCartographer::draw_gl(int widthi, int heighti)
{
	double widthd = (double)widthi;
	double heightd = (double)heighti;

	/* Активная карта */
	wxCartographer::map map = maps_[active_map_id_];

	/* Текущий масштаб. При перемещениях
		между масштабами - верхний масштаб */
	int zi = (int)z_;
	double dz = z_ - (double)zi;
	double alpha = 1.0 - dz;

	/* "Тайловые" координаты fix-точки */
	double fix_tile_x = lon_to_tile_x(fix_lon_, zi);
	double fix_tile_y = lat_to_tile_y(fix_lat_, zi, map.projection);

	tile::id fix_tile(active_map_id_, zi, (int)fix_tile_x, (int)fix_tile_y);

	/* И сразу его в очередь на загрузку - глядишь,
		к моменту отрисовки он уже и загрузится */
	get_tile(fix_tile);


	/* Экранные координаты fix-точки */
	//double fix_scr_x = widthd * fix_kx_;
	//double fix_scr_y = heightd * fix_ky_;

	//glDeleteTextures(WXSIZEOF(m_textures), m_textures);


	/*
		Подготовка OpenGL
	*/

	glViewport(0, 0, widthi, heighti);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); /* Пока не разобрался - зачем */

	{
		/* Уровень GL_PROJECTION */
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		/* Зрителя помещаем в fix-точку */
		double w = widthd / 256.0;
		double h = heightd / 256.0;
		double x = -w * fix_kx_;
		double y = -h * fix_ky_;

		#ifdef TEST_FRUSTRUM
		glFrustum(x, x + w, -y - h, -y, 0.8, 3.0);
		#else
		glOrtho(x, x + w, -y - h, -y, -1.0, 2.0);
		#endif

		/* С вертикалью работаем по старинке
			- сверху вниз, а не снизу вверх */
		glScaled(1.0, -1.0, 1.0);
		glScaled(1.0 + dz, 1.0 + dz, 1.0);
		glScaled(0.5, 0.5, 1.0);
		glTranslated(0.0, 0.0, -1.0);

		/* Уровень GL_MODELVIEW */
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslated(-2.0 * fix_tile_x, -2.0 * fix_tile_y, 0.0);
	}

	int x2 = 2 * fix_tile.x;
	int y2 = 2 * fix_tile.y;

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	for (int i = -3; i <= 3; ++i)
	{
		for (int j = -2; j <= 2; ++j)
		{
			paint_tile( tile::id(fix_tile.map_id, fix_tile.z + 1, 2*i + x2,     2*j + y2    ) );
			paint_tile( tile::id(fix_tile.map_id, fix_tile.z + 1, 2*i + x2,     2*j + y2 + 1) );
			paint_tile( tile::id(fix_tile.map_id, fix_tile.z + 1, 2*i + x2 + 1, 2*j + y2    ) );
			paint_tile( tile::id(fix_tile.map_id, fix_tile.z + 1, 2*i + x2 + 1, 2*j + y2 + 1) );
			/*-
			draw_tile(2*i + x2,     2*j + y2,     1.0, -0.1, m_textures[2]);
			draw_tile(2*i + x2,     2*j + y2 + 1, 1.0, -0.1, m_textures[3]);
			draw_tile(2*i + x2 + 1, 2*j + y2,     1.0, -0.1, m_textures[4]);
			draw_tile(2*i + x2 + 1, 2*j + y2 + 1, 1.0, -0.1, m_textures[5]);
			-*/
		}
	}

	/* * * * * * */
	glMatrixMode(GL_PROJECTION);
	glScaled(2.0, 2.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
	glTranslated(-fix_tile_x, -fix_tile_y, 0.0);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	//glColor4f(1.0f, 1.0f, 1.0f, alpha);
	//glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	for (int i = -3; i <= 3; ++i)
		for (int j = -2; j <= 2; ++j)
			paint_tile( tile::id(fix_tile.map_id, fix_tile.z, fix_tile.x + i, fix_tile.y + j) );
			//draw_tile(fix_tile.x + i, fix_tile.y + j, alpha, 0.0, m_textures[6]);

	glFlush();

    CheckGLError();

	/* Перестраиваем очереди загрузки тайлов.
		К этому моменту все необходимые тайлы уже в файловой очереди
		благодаря get_tile(). Но если её так и оставить, то файлы будут
		загружаться с правого нижнего угла, а нам хотелось бы, чтоб с центра */
	sort_queue(file_queue_, fix_tile, file_loader_);

	/* Серверную очередь тоже корректируем */
	sort_queue(server_queue_, fix_tile, server_loader_);
}

void wxCartographer::draw_tile(int tile_x, int tile_y, double alpha,
	double z, GLuint texture)
{
	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
        glNormal3f( 0.0f, 0.0f, 1.0f);
		glTexCoord2i(0, 0); glVertex3i(tile_x,     tile_y,     z);
		glTexCoord2i(1, 0); glVertex3i(tile_x + 1, tile_y,     z);
		glTexCoord2i(1, 1); glVertex3i(tile_x + 1, tile_y + 1, z);
		glTexCoord2i(0, 1); glVertex3i(tile_x,     tile_y + 1, z);
    glEnd();

	#ifdef TEST_FRUSTRUM
	glColor4d(0.0, 0.0, 0.0, alpha);
	glLineWidth(3);

	glBegin(GL_LINES);
		glVertex3d(tile_x, tile_y, z);
		glVertex3d(tile_x, tile_y, z + 0.2);

		glVertex3d(tile_x, tile_y + 1.0, z);
		glVertex3d(tile_x, tile_y + 1.0, z + 0.2);

		glVertex3d(tile_x + 1.0, tile_y, z);
		glVertex3d(tile_x + 1.0, tile_y, z + 0.2);

		glVertex3d(tile_x + 1.0, tile_y + 1.0, z);
		glVertex3d(tile_x + 1.0, tile_y + 1.0, z + 0.2);
	glEnd();
	#endif

	CheckGLError();
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
	debug_wait_for_finish(L"wxCartographer", posix_time::seconds(5));
    #endif

	wait_for_finish();
}

void wxCartographer::add_to_cache(const tile::id &tile_id, tile::ptr ptr)
{
	/* Загружать текстуру можно только в основном потоке */
	post_load_texture(ptr);

	{
		my::not_shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );
		cache_[tile_id] = ptr;
	}
	
	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );
		force_repaint_ = true;
	}

	Repaint();
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
	my::shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

bool wxCartographer::tile_in_queue(const tiles_queue &queue,
	my::worker::ptr worker, const tile::id &tile_id)
{
	if (worker)
	{
		my::locker locker( MYLOCKERPARAMS(worker->get_mutex(), 5, MYCURLINE) );

		tiles_queue::const_iterator iter = queue.find(tile_id);
		return iter != queue.end();
	}

	return false;
}

void wxCartographer::paint_tile(const tile::id &tile_id, int level)
{
	int z = tile_id.z - level;
	
	if (z <= 0)
		return;

	tile::ptr ptr = get_tile( tile::id(
		tile_id.map_id, z, tile_id.x >> level, tile_id.y >> level) );

	if (!ptr)
		paint_tile(tile_id, level + 1);
	else
	{
		GLuint id = ptr->texture_id();

		if (id == 0)
			id = load_texture(ptr->image());

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
			glNormal3f(0.0f, 0.0f, 1.0f);
			glTexCoord2d(x,     y    ); glVertex3i(tile_id.x,     tile_id.y,     0);
			glTexCoord2d(x + w, y    ); glVertex3i(tile_id.x + 1, tile_id.y,     0);
			glTexCoord2d(x + w, y + w); glVertex3i(tile_id.x + 1, tile_id.y + 1, 0);
			glTexCoord2d(x,     y + w); glVertex3i(tile_id.x,     tile_id.y + 1, 0);
		glEnd();

		CheckGLError();
			
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
	if ( !check_tile_id(tile_id))
		return tile::ptr();

	tile::ptr tile_ptr;

	/* Ищем в кэше */
	my::not_shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );
	tiles_cache::iterator iter = cache_.find(tile_id);

	/* Если не находим, добавляем в очередь на загрузку */
	if (iter == cache_.end())
		add_to_file_queue(tile_id);
	else
	{
		tile_ptr = iter->value();

		cache_.up(tile_id); /* Если уже был такой - переносим его наверх списка */

		/* Если текстура ещё на загружена, делаем вид, что тайла нет в кэше,
			чтобы нарисовался предок */
		if (tile_ptr && tile_ptr->texture_id() == 0)
			tile_ptr = tile::ptr();
	}

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
			my::locker locker( MYLOCKERPARAMS(worker->get_mutex(), 5, MYCURLINE) );

			file_queue_[tile_id] = 0; /* Не важно значение, важно само присутствие */
			wake_up(worker, locker); /* Будим работника, если он спит */
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
			my::locker locker( MYLOCKERPARAMS(worker->get_mutex(), 5, MYCURLINE) );

			server_queue_[tile_id] = 0; /* Не важно значение, важно само присутствие */
			wake_up(worker, locker); /* Будим работника, если он спит */
		}
	}
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void wxCartographer::file_loader_proc(my::worker::ptr this_worker)
{
	while (!finish())
	{
		tile::id tile_id;

		++file_loader_dbg_loop_;

		/* Берём идентификатор первого тайла из очереди */
		{
			/* Блокировкой гарантируем, что очередь не изменится */
			my::locker locker( MYLOCKERPARAMS(this_worker->get_mutex(), 5, MYCURLINE) );

			tiles_queue::iterator iter = file_queue_.begin();

			/* Если очередь пуста - засыпаем */
			if (iter == file_queue_.end())
			{
				sleep(this_worker, locker);
				continue;
			}

			tile_id = iter->key();

			/* Для дальнейших действий блокировка нам не нужна */
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

		/* Если файла нет - загружаем с сервера */
		if (!fs::exists(filename))
		{
			/* Но только если нет файла-метки об отсутствии тайла и там */
			if ( fs::exists(path.str() + L".tne") )
				add_to_cache(tile_id, tile::ptr());
			else
				add_to_server_queue(tile_id);
		}
		else
		{
			tile::ptr ptr( new tile(*this, filename) );

			/* При успешной загрузке с диска - сохраняем тайл в кэше */
			if (ptr->image())
				add_to_cache(tile_id, ptr);
			else
			{
				add_to_cache(tile_id, tile::ptr());
				main_log << L"Ошибка загрузки wxImage: " << filename << main_log;
			}
		}

		/* Удаляем тайл из очереди */
		{
			my::locker locker( MYLOCKERPARAMS(this_worker->get_mutex(), 5, MYCURLINE) );

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

		++server_loader_dbg_loop_;

		/* Берём идентификатор первого тайла из очереди */
		{
			/* Блокировкой гарантируем, что очередь не изменится */
			my::locker locker( MYLOCKERPARAMS(this_worker->get_mutex(), 5, MYCURLINE) );

			tiles_queue::iterator iter = server_queue_.begin();

			/* Если очередь пуста - засыпаем */
			if (iter == server_queue_.end())
			{
				sleep(this_worker, locker);
				continue;
			}

			tile_id = iter->key();

			/* Для дальнейших действий блокировка нам не нужна */
		}

		++server_loader_dbg_load_;

		/* Загружаем тайл с сервера */
		std::wstringstream path; /* Путь к локальному файлу  */

		wxCartographer::map &map = maps_[tile_id.map_id];

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

		try
		{
			/* Загружаем тайл с сервера ... */
			my::http::reply reply;
			get(reply, request.str());

			tile::ptr ptr;

			/* Тайла нет на сервере - создаём файл-метку */
			if (reply.status_code == 404)
			{
				reply.save(path.str() + L".tne");
			}
			
			/* При успешной загрузке с сервера, создаём тайл из буфера */
			else if (reply.status_code == 200)
			{
				ptr.reset( new tile(*this, reply.body.c_str(), reply.body.size()) );
				
				/* Если тайл нормальный, сохраняем на диске */
				if (ptr->image())
					reply.save(path.str() + map.ext);

				/* Если нет - очищаем. В кэш добавим "нулевой" указатель */
				else
				{
					ptr.reset();
					main_log << L"Ошибка загрузки wxImage: " << request.str() << main_log;
				}
	
			}
			
			/* Добавляем в кэш в любом случае, чтобы не загружать повторно */
			add_to_cache(tile_id, ptr);

			/* Удаляем тайл из очереди */
			{
				my::locker locker( MYLOCKERPARAMS(this_worker->get_mutex(), 5, MYCURLINE) );

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

		Repaint();

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
	return (lon + 180.0) * size_for_z(z) / 360.0;
}

double wxCartographer::lat_to_tile_y(double lat, double z,
	map::projection_t projection)
{
	double s = std::sin( lat / 180.0 * M_PI );
	double y;

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
	return x / size_for_z(z) * 360.0 - 180.0;
}

double wxCartographer::tile_y_to_lat(double y, double z,
	map::projection_t projection)
{
	double lat;
	double sz = size_for_z(z);
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

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"background rebuild: %d", backgrounder_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"load textures: %d", texturer_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"painter: %d", painter_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"draw_tile: %d", draw_tile_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"builder: %d", builder_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"file_loader: loop=%d load=%d queue=%d", file_loader_dbg_loop_, file_loader_dbg_load_, file_queue_.size());
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"server_loader: loop=%d load=%d queue=%d", server_loader_dbg_loop_, server_loader_dbg_load_, server_queue_.size());
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"on_idle: %d", on_idle_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"z: %0.1f", z_);
	gc.DrawText(buf, x, y), y += 12;

	int d;
	int m;
	double s;
	TO_DEG(fix_lat_, d, m, s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lat: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;

	TO_DEG(fix_lon_, d, m, s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lon: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;
}

void wxCartographer::prepare_background(wxCartographerBuffer &buffer,
	wxCoord width, wxCoord height, bool force_repaint, int map_id, int z,
	double fix_tile_x, double fix_tile_y,
	double fix_scr_x, double fix_scr_y)
{
#if 0
	/*
		Подготовка фона для заданного z:
			width, height - размеры окна
			map_id - номер карты
			z - масштаб
			fix_lat, fix_lon - географические координаты фиксированной точки
			fix_scr_x, fix_scr_y - фиксированная точка
	*/

	/*! Блокировка должна быть обеспечена извне !*/

	/* Тайл в центре экрана */
	tile::id fix_tile(map_id, z, (int)fix_tile_x, (int)fix_tile_y);

	/* И сразу его в очередь на загрузку - глядишь,
		к моменту отрисовки он уже и загрузится */
	get_tile(fix_tile);

	/* Координаты верхнего левого угла первого тайла */
	wxCoord x = (wxCoord)(fix_scr_x - (fix_tile_x - fix_tile.x) * 256.0 + 0.5);
	wxCoord y = (wxCoord)(fix_scr_y - (fix_tile_y - fix_tile.y) * 256.0 + 0.5);

	/* Определяем первый тайл (верхний левый угог экрана) */
	int first_tile_x = fix_tile.x;
	int first_tile_y = fix_tile.y;

	while (x > 0)
		x -= 256, --first_tile_x;
	while (y > 0)
		y -= 256, --first_tile_y;

	/* Определяем последний тайл (нижний правый угол экрана) */
	int last_tile_x = first_tile_x;
	int last_tile_y = first_tile_y;

	while (x < width)
		x += 256, ++last_tile_x;
	while (y < height)
		y += 256, ++last_tile_y;


	/* Итого:
		first_tile_x, first_tile_y - первый (верхний левый) тайл экрана
		last_tile_x, last_tile_y - последний (нижний правый) тайл экрана */

	/* Проверяем, необходимо ли обновлять буфер */
	if (!force_repaint
		&& buffer.map_id == map_id
		&& buffer.z == z
		&& buffer.first_tile_x <= first_tile_x
		&& buffer.first_tile_y <= first_tile_y
		&& buffer.last_tile_x >= last_tile_x
		&& buffer.last_tile_y >= last_tile_y)
	{
		return;
	}

	++backgrounder_debug_counter_;

	/* Немного расширяем экран, чтобы лишний раз не рисовать
		при сдвигах карты */
	--first_tile_x;
	--first_tile_y;
	++last_tile_x;
	++last_tile_y;

	wxCoord buf_width = (last_tile_x - first_tile_x) * 256;
	wxCoord buf_height = (last_tile_y - first_tile_y) * 256;

	if (!buffer.bitmap.IsOk()
		|| buffer.bitmap.GetWidth() < buf_width
		|| buffer.bitmap.GetHeight() < buf_height)
	{
		wxImage image(buf_width, buf_height, false);
		image.InitAlpha();
		buffer.bitmap = wxBitmap(image);

		//buffer.bitmap.Create(buf_width, buf_height);
	}

	wxMemoryDC dc(buffer.bitmap);
	wxGCDC gc(dc);

	/* Рисуем */
	tile::id tile_id(map_id, z, first_tile_x, 0);

	draw_tile_debug_counter_ = 0;

	gc.SetPen(*wxBLACK_PEN);
	gc.SetBrush(*wxBLACK_BRUSH);

	#ifndef NDEBUG
	gc.SetTextForeground(*wxWHITE);
	gc.SetFont( wxFont(6, wxFONTFAMILY_DEFAULT,
		wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
	#endif

	for (x = 0; tile_id.x < last_tile_x; ++tile_id.x, x += 256)
	{
		tile_id.y = first_tile_y;

		for (y = 0; tile_id.y < last_tile_y; ++tile_id.y, y += 256)
		{
			tile::ptr tile_ptr = get_tile(tile_id);

			if (tile_ptr && tile_ptr->ok())
			{
				++draw_tile_debug_counter_;
				gc.DrawBitmap(tile_ptr->bitmap(), x, y);
			}
			else
			{
				/* Чёрный тайл */
				gc.DrawRectangle(x, y, 256, 256);
			}

			#ifndef NDEBUG
			wxCoord sx = x + 2;
			wxCoord sy = y + 2;
			wchar_t buf[200];

			__swprintf(buf, sizeof(buf)/sizeof(*buf),
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

				__swprintf(buf, sizeof(buf)/sizeof(*buf), L"level: %d", tile_ptr->level());
				gc.DrawText(buf, sx, sy), sy += 12;
			}
			#endif

		}
	}

	buffer.map_id = map_id;
	buffer.z = z;
	buffer.first_tile_x = first_tile_x;
	buffer.first_tile_y = first_tile_y;
	buffer.last_tile_x = last_tile_x;
	buffer.last_tile_y = last_tile_y;
#endif
}

void wxCartographer::paint_map(wxGCDC &dc, wxCoord width, wxCoord height)
{
	/*! Блокировка должна быть обеспечена извне !*/

	wxCartographer::map map = maps_[active_map_id_];

	int zi = (int)z_;

	/* "Тайловые" координаты центра экрана */
	double fix_tile_x = lon_to_tile_x(fix_lon_, zi);
	double fix_tile_y = lat_to_tile_y(fix_lat_, zi, map.projection);

	/* Экранные координаты центра экрана */
	double fix_scr_x = width * fix_kx_;
	double fix_scr_y = height * fix_ky_;

	prepare_background(background1_, width, height, force_repaint_,
		active_map_id_, zi, fix_tile_x, fix_tile_y, fix_scr_x, fix_scr_y);

	force_repaint_ = false;

	double k = 1.0 + z_ - zi;
	double real_tile_sz = 256.0 * k;
	wxCoord x = (wxCoord)(fix_scr_x
        - (fix_tile_x - background1_.first_tile_x) * real_tile_sz + 0.5);
	wxCoord y = (wxCoord)(fix_scr_y
        - (fix_tile_y - background1_.first_tile_y) * real_tile_sz + 0.5);

	/* Выводим буфер */
	//if (z_ - zi < 0.01)
        //dc.DrawBitmap( background1_.bitmap, x, y);
    //else
        dc.GetGraphicsContext()->DrawBitmap( background1_.bitmap, x, y,
            (wxCoord)(background1_.bitmap.GetWidth() * k),
            (wxCoord)(background1_.bitmap.GetHeight() * k));

	/* Перестраиваем очереди загрузки тайлов.
		К этому моменту все необходимые тайлы уже в файловой очереди
		благодаря get_tile(). Но если её так и оставить, то файлы будут
		загружаться с правого нижнего угла, а нам хотелось бы, чтоб с центра */

	/* Тайл в центре экрана */
	tile::id fix_tile(active_map_id_, zi, (int)fix_tile_x, (int)fix_tile_y);

	sort_queue(file_queue_, fix_tile, file_loader_);

	/* Серверную очередь тоже корректируем */
	sort_queue(server_queue_, fix_tile, server_loader_);
}

void wxCartographer::repaint(wxDC &dc)
{
	/* Задача функции - подготовить буфер (создать, изменить размер,
		очистить), вызвать функции прорисовки карты (paint_map)
		и прорисовки объектов на карте пользователем (on_paint_),
		перенести всё это на экран */

	my::locker locker( MYLOCKERPARAMS(paint_mutex_, 5, MYCURLINE) );

	/* Измеряем скорость отрисовки */
	anim_speed_sw_.start();

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

	draw_gl(width, height);
    SwapBuffers();

    paint_debug_info(dc, width, height);

#if 0
	{
        wxMemoryDC dc;
        dc.SelectObject(buffer_);

		{
		    wxGCDC gc(dc);

            /* Очищаем */
            gc.SetBackground(*wxBLACK_BRUSH);
            gc.Clear();

            /* На время прорисовки параметры не должны изменяться */
            my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

            /* Рисуем */
            ++painter_debug_counter_;
            paint_map(gc, width, height);

            //if (on_paint_)
            //	on_paint_(gc, width, height);

            #ifndef NDEBUG
            paint_debug_info(gc, width, height);
            #endif
		}

		dc.SelectObject(wxNullBitmap);
	}

	/* Перерисовываем окно */
	dc.DrawBitmap(buffer_, 0, 0);
#endif

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

	fix_kx_ = scr_x / widthd();
	fix_ky_ = scr_y / heightd();
}

void wxCartographer::set_fix_to_scr_xy(double scr_x, double scr_y)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	fix_lat_ = scr_y_to_lat(scr_y, z_, maps_[active_map_id_].projection,
		fix_lat_, heightd() * fix_ky_);
	fix_lon_ = scr_x_to_lon(scr_x, z_, fix_lon_, widthd() * fix_kx_);

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
	Repaint();
}

void wxCartographer::on_left_down(wxMouseEvent& event)
{
	SetFocus();

	set_fix_to_scr_xy( (double)event.GetX(), (double)event.GetY() );

	move_mode_ = true;

	#ifdef BOOST_WINDOWS
	CaptureMouse();
	#endif

	Refresh(false);
}

void wxCartographer::on_left_up(wxMouseEvent& event)
{
	if (move_mode_)
	{
		set_fix_to_scr_xy( widthd() / 2.0, heightd() / 2.0 );
		move_mode_ = false;

		#ifdef BOOST_WINDOWS
		ReleaseMouse();
		#endif

		Refresh(false);
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
		Repaint();
		Refresh(false);
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
		
		SetZ(z);
	}

	Repaint();
}

void wxCartographer::Repaint()
{
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
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return maps_[active_map_id_];
}

bool wxCartographer::SetActiveMap(const std::wstring &MapName)
{
	int map_num_id = maps_name_to_id_[MapName];

	if (map_num_id)
	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

		active_map_id_ = map_num_id;

		Repaint();

		return true;
	}

	return false;
}

wxCoord wxCartographer::LatToY(double Lat)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return (wxCoord)(lat_to_scr_y(Lat, z_, maps_[active_map_id_].projection,
		fix_lat_, heightd() * fix_ky_) + 0.5);
}

wxCoord wxCartographer::LonToX(double Lon)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return (wxCoord)(lon_to_scr_x(Lon, z_,
		fix_lon_, widthd() * fix_kx_) + 0.5);
}

int wxCartographer::GetZ(void)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return (int)(z_ + 0.5);
}

void wxCartographer::SetZ(int z)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	new_z_ = z;
	z_step_ = def_min_anim_steps_ ? 2 * def_min_anim_steps_ : 1;

	Repaint();
}

double wxCartographer::GetLat()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return fix_lat_;
}

double wxCartographer::GetLon()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return fix_lon_;
}

void wxCartographer::MoveTo(int z, double lat, double lon)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	z_ = z;
	fix_lat_ = lat;
	fix_lon_ = lon;
	Repaint();
}

void wxCartographer::on_idle(wxIdleEvent& event)
{
	++on_idle_debug_counter_;
}
