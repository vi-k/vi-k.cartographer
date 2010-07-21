//#define WXCART_PAINT_IN_THREAD 0
#define TEST_FRUSTRUM

#include "wxCartographer.h"
#include "wxCartographer.h"
#include "handle_exception.h"

/* windows */
#ifdef BOOST_WINDOWS

/* Макросы нужны для gdiplus.h */
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

#include <windows.h>
#undef GDIPVER
#define GDIPVER 0x0110
#include <gdiplus.h>
#include <windowsx.h>
#undef max
#undef min

#endif /* BOOST_WINDOWS */


#if defined(_MSC_VER)
#define __swprintf swprintf_s
#else
#define __swprintf snwprintf
#endif


#include <my_exception.h>
#include <my_ptr.h>
#include <my_utf8.h>
#include <my_time.h>
#include <my_xml.h>
#include <my_fs.h> /* boost::filesystem */
#include <my_curline.h>

#include <math.h> /* sin, sqrt */
#include <wchar.h> /* swprintf */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

#include <wx/dcclient.h> /* wxPaintDC */
#include <wx/dcmemory.h> /* wxMemoryDC */
#include <wx/rawbmp.h> /* wxNativePixelData */

MYLOCKINSPECTOR_INIT()

#define EXCT 0.081819790992 /* эксцентриситет эллипса */

template<typename Real>
Real atanh(Real x)
{
	return 0.5 * log( (1.0 + x) / (1.0 - x) );
}

static void LoadImage(const char *filename, raw_image &image)
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

wxCartographer::wxCartographer(wxWindow *parent, const std::wstring &serverAddr,
	const std::wstring &serverPort, std::size_t cacheSize,
	std::wstring cachePath, bool onlyCache,
	const std::wstring &initMap, int initZ, wxDouble initLat, wxDouble initLon,
	OnPaintProc_t onPaintProc,
	int animPeriod, int defAnimSteps)
	: wxGLCanvas(parent, wxID_ANY, NULL /* attribs */,
                 wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE)
	, gl_context_(this)
	, cache_path_( fs::system_complete(cachePath).string() )
	, only_cache_(onlyCache)
	, cache_(cacheSize)
	, builder_debug_counter_(0)
	, file_queue_(100)
	, file_loader_debug_counter_(0)
	, server_queue_(100)
	, server_loader_debug_counter_(0)
	, anim_period_( posix_time::milliseconds(animPeriod) )
	, def_anim_steps_(defAnimSteps)
	, anim_speed_(0)
	, anim_freq_(0)
	, animator_debug_counter_(0)
	, background1_()
	, background2_()
	, buffer_(100,100)
	, draw_tile_debug_counter_(0)
	, active_map_id_(0)
	, z_(initZ)
	, fix_kx_(0.5)
	, fix_ky_(0.5)
	, fix_lat_(initLat)
	, fix_lon_(initLon)
	, painter_debug_counter_(0)
	, backgrounder_debug_counter_(0)
	, move_mode_(false)
	, force_repaint_(false)
	, on_paint_(onPaintProc)
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
    wxInitAllImageHandlers();

    SetCurrent(gl_context_);

	#ifdef TEST_FRUSTRUM
	glEnable(GL_DEPTH_TEST);
	#endif

    //glEnable(GL_LIGHTING);
    //glEnable(GL_LIGHT0);

	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.3f, 1.0f);				// Black Background
	glClearDepth(1.0);									// Depth Buffer Setup

    // add slightly more light, the default lighting is rather dark
    //GLfloat ambient[] = { 0.5, 0.5, 0.5, 0.5 };
    //glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    // set viewing projection
    glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//glOrtho(0.0, 2.0, 0.0, 2.0, 0.0, 1.0);

    //glMatrixMode(GL_MODELVIEW);
    //glLoadIdentity();

	//glOrtho(0.0, 2.0, 0.0, 1.0, 0.0, 1.0);
	//glFrustum(0.0f, 1.0f, 0.0f, 1.0f, 1.25f, 3.0f);

    // create the textures to use for cube sides: they will be reused by all
    // canvases (which is probably not critical in the case of simple textures
    // we use here but could be really important for a real application where
    // each texture could take many megabytes)
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

	glEnable(GL_BLEND);			// Turn Blending On

    CheckGLError();
}

void wxCartographer::draw_gl()
{
	GLdouble alpha = 0.5;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glScaled(2.0 - alpha, 2.0 - alpha, 1.0);
	glScaled(0.5, 0.5, 1.0);
	glTranslated(0.0, 0.0, -1.0);

	double x = 7169.0;
	double y = 2837.0;
	double x2 = 14338.0;
	double y2 = 5674.0;

	/* * * * * * */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
	glTranslated(-2 * x - 1.0, -2 * y - 1.0, 0.0);

	for (int i = -2; i <= 2; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			draw_tile(2*i + x2, 2*j + y2, 1.0, -0.1, m_textures[2]);
			draw_tile(2*i + x2, 2*j + y2 + 1.0, 1.0, -0.1, m_textures[3]);
			draw_tile(2*i + x2 + 1.0, 2*j + y2, 1.0, -0.1, m_textures[4]);
			draw_tile(2*i + x2 + 1.0, 2*j + y2 + 1.0, 1.0, -0.1, m_textures[5]);
		}
	}


	/* * * * * * */
	glMatrixMode(GL_PROJECTION);
	glScaled(2.0, 2.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
	glTranslated(-x - 0.5, -y - 0.5, 0.0);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);					// Full Brightness.  50% Alpha
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);	// Set The Blending Function For Translucency

	for (int i = -2; i <= 2; ++i)
		for (int j = -1; j <= 1; ++j)
			draw_tile(x + i, y + j, alpha, 0.0, m_textures[6]);

	glFlush();

    CheckGLError();
}

void wxCartographer::draw_tile(double x, double y, double alpha, double z, GLuint texture)
{
	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
        glNormal3f( 0.0f, 0.0f, 1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3d(x,       y,       z);
		glTexCoord2f(1.0f, 0.0f); glVertex3d(x + 1.0, y,       z);
		glTexCoord2f(1.0f, 1.0f); glVertex3d(x + 1.0, y + 1.0, z);
		glTexCoord2f(0.0f, 1.0f); glVertex3d(x,       y + 1.0, z);
    glEnd();

	#ifdef TEST_FRUSTRUM
	glColor4d(0.0, 0.0, 0.0, alpha);
	glLineWidth(3);

	glBegin(GL_LINES);
		glVertex3d(x, y, z);
		glVertex3d(x, y, z + 0.2);

		glVertex3d(x, y + 1.0, z);
		glVertex3d(x, y + 1.0, z + 0.2);

		glVertex3d(x + 1.0, y, z);
		glVertex3d(x + 1.0, y, z + 0.2);

		glVertex3d(x + 1.0, y + 1.0, z);
		glVertex3d(x + 1.0, y + 1.0, z + 0.2);
	glEnd();
	#endif
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
	{
		my::not_shared_locker locker( MYLOCKERPARAMS(cache_mutex_, 5, MYCURLINE) );

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

	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );
		force_repaint_ = true;
	}

	Repaint();
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

	wxImage im1 = parent_ptr->bitmap().ConvertToImage();
	wxImage im2 = im1.GetSubImage(
		wxRect((tile_id.x & 1) * 128, (tile_id.y & 1) * 128, 128, 128) );
	tile_ptr->bitmap() = wxBitmap(im2.Scale(256, 256));

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

		++file_loader_debug_counter_;

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

		++server_loader_debug_counter_;

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

		/* Если доступна отрисовка в потоке, то рисуем */
		#if WXCART_PAINT_IN_THREAD

		repaint();

		/* Если нет, то только уведомляем главный поток
			о необходимости отрисовки */
		#else

		Repaint();

		#endif

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
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

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

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"painter: %d", painter_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"draw_tile: %d", draw_tile_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"builder: %d", builder_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"file_loader: %d", file_loader_debug_counter_);
	gc.DrawText(buf, x, y), y += 12;

	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"server_loader: %d", server_loader_debug_counter_);
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
	wxDouble fix_tile_x, wxDouble fix_tile_y,
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
}

void wxCartographer::paint_map(wxGCDC &dc, wxCoord width, wxCoord height)
{
	/*! Блокировка должна быть обеспечена извне !*/

	wxCartographer::map map = maps_[active_map_id_];

	int zi = (int)z_;

	/* "Тайловые" координаты центра экрана */
	wxDouble fix_tile_x = lon_to_tile_x(fix_lon_, zi);
	wxDouble fix_tile_y = lat_to_tile_y(fix_lat_, zi, map.projection);

	/* Экранные координаты центра экрана */
	wxDouble fix_scr_x = width * fix_kx_;
	wxDouble fix_scr_y = height * fix_ky_;

	prepare_background(background1_, width, height, force_repaint_,
		active_map_id_, zi, fix_tile_x, fix_tile_y, fix_scr_x, fix_scr_y);

	force_repaint_ = false;

	wxDouble k = 1.0 + z_ - zi;
	wxDouble real_tile_sz = 256.0 * k;
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

#if WXCART_PAINT_IN_THREAD
void wxCartographer::repaint()
#else
void wxCartographer::repaint(wxDC &dc)
#endif
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
	#if WXCART_PAINT_IN_THREAD
	scoped_ptr<wxGraphicsContext> gc( wxGraphicsContext::Create(this) );
	gc->DrawBitmap(buffer_, 0.0, 0.0, width, height);
	#else
	dc.DrawBitmap(buffer_, 0, 0);
	#endif


	#if 0
	Gdiplus::Graphics *gr_win = (Gdiplus::Graphics*)gc_win->GetNativeContext();
	HDC hdc_win = gr_win->GetHDC();
	HDC hdc_buf = gr_buf->GetHDC();

	BLENDFUNCTION bf = {AC_SRC_OVER, 0, 128, AC_SRC_ALPHA};
	AlphaBlend(hdc_win, 0, 0, width, height, hdc_buf, 0, 0, width, height, bf);

	gr_win->ReleaseHDC(hdc_win);
	gr_buf->ReleaseHDC(hdc_buf);

	paint_debug_info(*gc_win.get(), width, height);
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
#endif
}

void wxCartographer::move_fix_to_scr_xy(wxDouble scr_x, wxDouble scr_y)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	fix_kx_ = scr_x / widthd();
	fix_ky_ = scr_y / heightd();
}

void wxCartographer::set_fix_to_scr_xy(wxDouble scr_x, wxDouble scr_y)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	fix_lat_ = scr_y_to_lat(scr_y, z_, maps_[active_map_id_].projection,
		fix_lat_, heightd() * fix_ky_);
	fix_lon_ = scr_x_to_lon(scr_x, z_, fix_lon_, widthd() * fix_kx_);

	move_fix_to_scr_xy(scr_x, scr_y);
}

void wxCartographer::on_paint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

	repaint();

	int width;
	int height;
    GetClientSize(&width, &height);

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

	//double fix_kx_ = 0.5;
	//double fix_ky_ = 0.5;

	GLdouble w = width / 256.0;
	GLdouble h = height / 256.0;
	GLdouble x = -w * fix_kx_;
	GLdouble y = -h * fix_ky_;
	#ifdef TEST_FRUSTRUM
	glFrustum(x, x + w, -y - h, -y, 0.8, 3.0);
	#else
	glOrtho(x, x + w, -y - h, -y, -1.0, 2.0);
	#endif
	glScaled(1.0, -1.0, 1.0);

	draw_gl();
    SwapBuffers();

#if 0
	/* Если отрисовка ведётся в потоке,
		то выводим уже сформированную картинку */
	#if WXCART_PAINT_IN_THREAD

	mutex::scoped_lock l(paint_mutex_);
	wxPaintDC dc(this);
	dc.DrawBitmap(buffer_, 0, 0);

	/* Если отрисовка доступна только в главном потоке,
		то здесь и будет производиться вся работа по отрисовке */
	#else

	wxPaintDC dc(this);
	repaint(dc);

	#endif

	event.Skip(false);
#endif
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

	set_fix_to_scr_xy( (wxDouble)event.GetX(), (wxDouble)event.GetY() );

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
		move_fix_to_scr_xy( (wxDouble)event.GetX(), (wxDouble)event.GetY() );
		Repaint();
	}
}

void wxCartographer::on_mouse_wheel(wxMouseEvent& event)
{
	{
		my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

		z_ += (event.GetWheelRotation() / event.GetWheelDelta()) / 3.0;

		if (z_ < 1.0)
			z_ = 1.0;

		if (z_ > 30.0)
			z_ = 30.0;

		wxDouble z = std::floor(z_ + 0.5);
		if ( std::abs(z_ - z) < 0.01)
			z_ = z;
	}

	Repaint();
}

void wxCartographer::Repaint()
{
	/* Если отрисовка ведётся в потоке, будим поток,
		занимающийся анимацией, если он спит */
	#if WXCART_PAINT_IN_THREAD

	/* Копированием указателя на "работника" гарантируем,
		что он не будет удалён, пока выполняется функция */
	my::worker::ptr worker = animator_;
	if (worker)
		wake_up(worker);

	/* Если отрисовка доступна только в главном потоке,
		то отсылаем сообщение о необходимости обновления */
	#else

	Refresh(false);

	#endif
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

wxCoord wxCartographer::LatToY(wxDouble Lat)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return (wxCoord)(lat_to_scr_y(Lat, z_, maps_[active_map_id_].projection,
		fix_lat_, heightd() * fix_ky_) + 0.5);
}

wxCoord wxCartographer::LonToX(wxDouble Lon)
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

	z_ = z;
	Repaint();
}

wxDouble wxCartographer::GetLat()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return fix_lat_;
}

wxDouble wxCartographer::GetLon()
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	return fix_lon_;
}

void wxCartographer::MoveTo(int z, wxDouble lat, wxDouble lon)
{
	my::recursive_locker locker( MYLOCKERPARAMS(params_mutex_, 5, MYCURLINE) );

	z_ = z;
	fix_lat_ = lat;
	fix_lon_ = lon;
	Repaint();
}
