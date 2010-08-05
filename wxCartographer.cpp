#include "wxCartographer.h"
#include "handle_exception.h"

#if defined(_MSC_VER)
#define __swprintf swprintf_s
#else
#define __swprintf snwprintf
#endif

#include <math.h> /* sin, sqrt */
#include <wchar.h> /* swprintf */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

/* Эллипсоид Красовского */
//const double c_a  = 6378245.0; /* большая полуось */
//const double c_f = 1.0 / 298.3; /* flattening / сжатие */

/* Эллипсоид WGS84 */
static const double c_a  = 6378137.0; /* большая полуось */
static const double c_f = 1.0 / 298.257223563; /* сжатие / flattening */

static const double c_b  = c_a * (1.0 - c_f); /* малая полуось */
static const double c_e = sqrt(c_a * c_a - c_b * c_b) / c_a; /* эксцентриситет эллипса / eccentricity */
static const double c_e2 = c_e * c_e;
static const double c_e4 = c_e2 * c_e2;
static const double c_e6 = c_e2 * c_e2 * c_e2;
static const double c_k = 1.0 - c_f; /*
                        = c_b / c_a
                        = sqrt(1.0 - c_e2)
                        = 1 / sqrt(1.0 + c_eb2)
                        = c_e / c_eb */
static const double c_eb = sqrt(c_a * c_a - c_b * c_b) / c_b;
static const double c_eb2 = c_eb * c_eb;
static const double c_b_eb2 = c_b * c_eb2;
static const double c_b_eb4 = c_b_eb2 * c_eb2;
static const double c_b_eb6 = c_b_eb4 * c_eb2;

template<typename Real>
Real atanh(Real x)
{
	return 0.5 * log( (1.0 + x) / (1.0 - x) );
}

namespace cart
{

double DegreesToGeo(double deg, double min, double sec)
{
	return deg + min / 60.0 + sec / 3600.0;
}

void GeoToDegrees(double lat_or_lon, int *pdeg, int *pmin, double *psec)
{
	int d = (int)lat_or_lon;
	double m_d = (lat_or_lon - d) * 60.0;
	int m = (int)m_d;
	double s = (m_d - (double)m) * 60.0;

	*pdeg = d;
	*pmin = m;
	*psec = s;
}

BEGIN_EVENT_TABLE(Cartographer, wxGLCanvas)
	EVT_PAINT(Cartographer::on_paint)
	EVT_ERASE_BACKGROUND(Cartographer::on_erase_background)
	EVT_SIZE(Cartographer::on_size)
	EVT_LEFT_DOWN(Cartographer::on_left_down)
	EVT_LEFT_UP(Cartographer::on_left_up)
	EVT_MOUSE_CAPTURE_LOST(Cartographer::on_capture_lost)
	EVT_MOTION(Cartographer::on_mouse_move)
	EVT_MOUSEWHEEL(Cartographer::on_mouse_wheel)
	//EVT_KEY_DOWN(Cartographer::OnKeyDown)
END_EVENT_TABLE()

Cartographer::Cartographer(wxWindow *parent, const std::wstring &server_addr,
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
	, fix_step_(0)
	, fix_alpha_(0.0)
	, painter_debug_counter_(0)
	, move_mode_(false)
	, force_repaint_(false)
	, paint_thread_id_( boost::this_thread::get_id() )
	, on_paint_handler_(on_paint_proc)
	, sprites_index_(0)
	, on_image_delete_( boost::bind(&Cartographer::on_image_delete_proc, this, _1) )
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

				if (projection == L"spheroid") /* Google */
					map.projection = map_info::spheroid;
				else if (projection == L"ellipsoid") /* Yandex */
					map.projection = map_info::ellipsoid;
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
			&Cartographer::file_loader_proc, this, file_loader_) );

		/* Запускаем серверный загрузчик тайлов */
		if (!only_cache_)
		{
			server_loader_ = new_worker(L"server_loader"); /* Название - только для отладки */
			boost::thread( boost::bind(
				&Cartographer::server_loader_proc, this, server_loader_) );
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
				&Cartographer::anim_thread_proc, this, animator_) );
		}
	}
	catch(std::exception &e)
	{
		throw my::exception(L"Ошибка создания Картографа")
			<< my::param(L"serverAddr", server_addr)
			<< my::param(L"serverPort", server_port)
			<< my::exception(e);
	}

	Update();
}

Cartographer::~Cartographer()
{
	if (!finish())
		Stop();

	cache_.clear();
	delete_textures();
	magic_deinit();
	sprites_.clear();

	assert( load_texture_debug_counter_ == delete_texture_debug_counter_ );
}

void Cartographer::Stop()
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
	debug_wait_for_finish(L"Cartographer", posix_time::seconds(5));
	#endif

	wait_for_finish();
}

void Cartographer::Update()
{
	//Refresh(false);
}

int Cartographer::GetMapsCount()
{
	return (int)maps_.size();
}

map_info Cartographer::GetMapInfo(int index)
{
	map_info map;
	maps_list::iterator iter = maps_.begin();

	while (index-- && iter != maps_.end())
		++iter;

	if (iter != maps_.end())
		map = iter->second;

	return map;
}

map_info Cartographer::GetActiveMapInfo()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return maps_[active_map_id_];
}

bool Cartographer::SetActiveMapByIndex(int index)
{
	maps_list::iterator iter = maps_.begin();

	while (index-- && iter != maps_.end())
		++iter;

	if (iter == maps_.end())
		return false;

	unique_lock<recursive_mutex> lock(params_mutex_);
	active_map_id_ = iter->first;
	Update();

	return true;
}

bool Cartographer::SetActiveMapByName(const std::wstring &map_name)
{
	maps_name_to_id_list::iterator iter = maps_name_to_id_.find(map_name);

	if (iter == maps_name_to_id_.end())
		return false;

	unique_lock<recursive_mutex> lock(params_mutex_);
	active_map_id_ = iter->second;
	Update();

	return true;
}

point Cartographer::GeoToScr(double lat, double lon)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	double w, h;
	get_viewport_size(&w, &h);

	point pt;
	pt.x = lon_to_scr_x(lon, z_, fix_lon_, w * fix_kx_);
	pt.y = lat_to_scr_y(lat, z_, maps_[active_map_id_].projection, fix_lat_, h * fix_ky_);

	return pt;
}

coord Cartographer::ScrToGeo(double x, double y)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	double w, h;
	get_viewport_size(&w, &h);

	coord pt;
	pt.lat = scr_y_to_lat(y, z_, maps_[active_map_id_].projection, fix_lat_, h * fix_ky_);
	pt.lon = scr_x_to_lon(x, z_, fix_lon_, w * fix_kx_);

	return pt;
}

double Cartographer::GetActiveZ(void)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return z_;
}

void Cartographer::SetActiveZ(int z)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	if (z < 1)
		z = 1;
	
	if (z > 30)
		z = 30;

	new_z_ = z;
	z_step_ = def_min_anim_steps_ ? 2 * def_min_anim_steps_ : 1;

	Update();
}

void Cartographer::ZoomIn()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	SetActiveZ( new_z_ + 1.0 );
}

void Cartographer::ZoomOut()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	SetActiveZ( new_z_ - 1.0 );
}

coord Cartographer::GetActiveGeoPos()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return coord(fix_lat_, fix_lon_);
}

point Cartographer::GetActiveScrPos()
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	double w, h;
	get_viewport_size(&w, &h);

	return point(w * fix_kx_, h * fix_ky_);
}

void Cartographer::MoveTo(int z, double lat, double lon)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	fix_lat_ = lat;
	fix_lon_ = lon;
	SetActiveZ(z);
}

int Cartographer::LoadImageFromFile(const std::wstring &filename)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);

	sprite::ptr sprite_ptr( new sprite(on_image_delete_) );
	sprites_[++sprites_index_] = sprite_ptr;

	unique_lock<recursive_mutex> lock2( sprite_ptr->get_mutex() );

	if (!sprite_ptr->load_from_file(filename))
	{
		sprites_.erase(sprites_index_--);
		return 0;
	}

	/* Загружаем текстуру, если это возможно */
	load_image_to_texture(*sprite_ptr);

	return sprites_index_;
}

int Cartographer::LoadImageFromMem(const void *data, std::size_t size)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);

	sprite::ptr sprite_ptr( new sprite(on_image_delete_) );
	sprites_[++sprites_index_] = sprite_ptr;

	unique_lock<recursive_mutex> lock2( sprite_ptr->get_mutex() );

	if (!sprite_ptr->load_from_mem(data, size))
	{
		sprites_.erase(sprites_index_--);
		return 0;
	}

	/* Загружаем текстуру, если это возможно */
	load_image_to_texture(*sprite_ptr);

	return sprites_index_;
}
int Cartographer::LoadImageFromRaw(const unsigned char *data,
	int width, int height, bool with_alpha)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);

	sprite::ptr sprite_ptr( new sprite(on_image_delete_) );
	sprites_[++sprites_index_] = sprite_ptr;

	unique_lock<recursive_mutex> lock2( sprite_ptr->get_mutex() );

	sprite_ptr->load_from_raw(data, width, height, with_alpha);

	/* Загружаем текстуру, если это возможно */
	load_image_to_texture(*sprite_ptr);

	return sprites_index_;
}

void Cartographer::DeleteImage(int image_id)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);
	sprites_.erase(image_id);
}

size Cartographer::GetImageOffset(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	size offset;

	if (iter != sprites_.end())
		offset = iter->second->offset();

	return offset;
}

void Cartographer::SetImageOffset(int image_id, double dx, double dy)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_offset(dx, dy);
}

point Cartographer::GetImageCentralPoint(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	point pos;

	if (iter != sprites_.end())
		pos = iter->second->central_point();

	return pos;
}

void Cartographer::SetImageCentralPoint(int image_id, double x, double y)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_central_point(x, y);
}

size Cartographer::GetImageSize(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	size sz;

	if (iter != sprites_.end())
		sz = iter->second->get_size();

	return sz;
}

size Cartographer::GetImageScale(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	size scale;

	if (iter != sprites_.end())
		scale = iter->second->scale();

	return scale;
}

void Cartographer::SetImageScale(int image_id, const size &scale)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	size sz;

	if (iter != sprites_.end())
		iter->second->set_scale(scale);
}

void Cartographer::DrawImage(int image_id, double x, double y,
	double kx = 1.0, double ky = 1.0)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
	{
		sprite::ptr sprite_ptr = iter->second;
		unique_lock<recursive_mutex> lock2( sprite_ptr->get_mutex() );

		/* Загружаем текстуру, если она не была загружена */
		GLuint texture_id = load_image_to_texture(*sprite_ptr);

		double w = sprite_ptr->raw().width();
		double h = sprite_ptr->raw().height();
		kx *= sprite_ptr->scale().width;
		ky *= sprite_ptr->scale().height;

		w *= kx;
		h *= ky;

		size offset = sprite_ptr->offset();
		x += offset.width * kx;
		y += offset.height * ky;

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0); glVertex3d(x,     y,     0);
			glTexCoord2d(1.0, 0.0); glVertex3d(x + w, y,     0);
			glTexCoord2d(1.0, 1.0); glVertex3d(x + w, y + h, 0);
			glTexCoord2d(0.0, 1.0); glVertex3d(x,     y + h, 0);
		glEnd();

		magic_exec();
		
		check_gl_error();
	}
}

void Cartographer::magic_init()
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

void Cartographer::magic_deinit()
{
	glDeleteTextures(1, &magic_id_);
	check_gl_error();
}

void Cartographer::magic_exec()
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

void Cartographer::check_gl_error()
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

void Cartographer::load_textures()
{
	shared_lock<shared_mutex> lock(cache_mutex_);
	
	tiles_cache::iterator iter = cache_.begin();

	int count = 0;

	while (iter != cache_.end() && ++count <= cache_active_tiles_)
	{
		tile::ptr tile_ptr = iter->value();

		unique_lock<recursive_mutex> lock2( tile_ptr->get_mutex() );

		if (tile_ptr->ok())
			load_image_to_texture(*tile_ptr);

		++iter;
	}
}

void Cartographer::delete_texture_later(GLuint texture_id)
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

void Cartographer::delete_texture(GLuint texture_id)
{
	glDeleteTextures(1, &texture_id);
	check_gl_error();
	++delete_texture_debug_counter_;
}

void Cartographer::delete_textures()
{
	unique_lock<mutex> lock(delete_texture_mutex_);
		
	while (delete_texture_queue_.size())
	{
		GLuint texture_id = delete_texture_queue_.front();
		delete_texture_queue_.pop_front();
		delete_texture(texture_id);
	}
}

bool Cartographer::check_tile_id(const tile::id &tile_id)
{
	int sz = size_for_z_i(tile_id.z);

	return tile_id.z >= 1
		&& tile_id.x >= 0 && tile_id.x < sz
		&& tile_id.y >= 0 && tile_id.y < sz;
}

tile::ptr Cartographer::find_tile(const tile::id &tile_id)
{
	/* Блокируем кэш для чтения */
	shared_lock<shared_mutex> lock(cache_mutex_);

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

void Cartographer::paint_tile(const tile::id &tile_id, int level)
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

tile::ptr Cartographer::get_tile(const tile::id &tile_id)
{
	//if ( !check_tile_id(tile_id))
	//	return tile::ptr();

	tile::ptr tile_ptr = find_tile(tile_id);

	return tile_ptr;
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void Cartographer::file_loader_proc(my::worker::ptr this_worker)
{
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
			unique_lock<recursive_mutex> lock( tile_ptr->get_mutex() );

			if (!tile_ptr->load_from_file(filename))
			{
				tile_ptr->set_state(tile::ready);
				main_log << L"Ошибка загрузки wxImage: " << filename << main_log;
			}
		}

	} /* while (!finish()) */
}

/* Загрузчик тайлов с сервера. При пустой очереди - засыпает */
void Cartographer::server_loader_proc(my::worker::ptr this_worker)
{
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
				unique_lock<recursive_mutex> lock2( tile_ptr->get_mutex() );

				/* При успешной загрузке с сервера, создаём тайл из буфера
					и сохраняем файл на диске */
				if ( tile_ptr->load_from_mem(reply.body.c_str(), reply.body.size()) )
					reply.save(path.str() + map.ext);
				else
				{
					tile_ptr->set_state(tile::ready);
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

void Cartographer::anim_thread_proc(my::worker::ptr this_worker)
{
	asio::io_service io_service;
	asio::deadline_timer timer(io_service, my::time::utc_now());

	while (!finish())
	{
		++animator_debug_counter_;

		{
			unique_lock<recursive_mutex> lock(params_mutex_);

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
			unique_lock<mutex> lock(paint_mutex_);
		}

		//Update();
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

void Cartographer::get(my::http::reply &reply,
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

unsigned int Cartographer::load_and_save(const std::wstring &request,
	const std::wstring &local_filename)
{
	my::http::reply reply;

	get(reply, request);

	if (reply.status_code == 200)
		reply.save(local_filename);

	return reply.status_code;
}

unsigned int Cartographer::load_and_save_xml(const std::wstring &request,
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

double Cartographer::lon_to_tile_x(double lon, double z)
{
	return (lon + 180.0) * size_for_z_d(z) / 360.0;
}

double Cartographer::lat_to_tile_y(double lat, double z,
	map_info::projection_t projection)
{
	double s = sin( lat / 180.0 * M_PI );
	double y;

	switch (projection)
	{
		case map_info::spheroid:
			y = (0.5 - atanh(s) / (2*M_PI)) * size_for_z_d(z);
			break;

		case map_info::ellipsoid:
			// q - изометрическая широта
			// q = 1/2 * ln( (1 + sin B) / (1 - sin B) )
			//	- e / 2 * ln( (1 + e * sin B) / (1 - e * sin B) )
			y = (0.5 - (atanh(s) - c_e * atanh(c_e*s)) / (2*M_PI)) * size_for_z_d(z);
			break;

		default:
			assert(projection == map_info::spheroid
				|| projection == map_info::ellipsoid);
	}

	return y;
}

double Cartographer::lon_to_scr_x(double lon, double z,
	double fix_lon, double fix_scr_x)
{
	double fix_tile_x = lon_to_tile_x(fix_lon, z);
	double tile_x = lon_to_tile_x(lon, z);
	return (tile_x - fix_tile_x) * 256.0 + fix_scr_x;
}

double Cartographer::lat_to_scr_y(double lat, double z,
	map_info::projection_t projection, double fix_lat, double fix_scr_y)
{
	double fix_tile_y = lat_to_tile_y(fix_lat, z, projection);
	double tile_y = lat_to_tile_y(lat, z, projection);
	return (tile_y - fix_tile_y) * 256.0 + fix_scr_y;
}

double Cartographer::tile_x_to_lon(double x, double z)
{
	return x / size_for_z_d(z) * 360.0 - 180.0;
}

double Cartographer::tile_y_to_lat(double y, double z,
	map_info::projection_t projection)
{
	double lat;
	double sz = size_for_z_d(z);
	double tmp = atan( exp( (0.5 - y / sz) * (2 * M_PI) ) );

	switch (projection)
	{
		case map_info::spheroid:
			lat = tmp * 360.0 / M_PI - 90.0;
			break;

		case map_info::ellipsoid:
		{
			tmp = tmp * 2.0 - M_PI / 2.0;
			double yy = y - sz / 2.0;
			double tmp2;
			do
			{
				tmp2 = tmp;
				tmp = asin(1.0 - ((1.0 + sin(tmp))*pow(1.0-c_e*sin(tmp),c_e)) / (exp((2.0*yy)/-(sz/(2.0*M_PI)))*pow(1.0+c_e*sin(tmp),c_e)) );

			} while( abs(tmp - tmp2) > 0.00000001 );

			lat = tmp * 180.0 / M_PI;
		}
		break;

		default:
			assert(projection == map_info::spheroid
				|| projection == map_info::ellipsoid);
	}

	return lat;
}

double Cartographer::scr_x_to_lon(double x, double z,
	double fix_lon, double fix_scr_x)
{
	double fix_tile_x = lon_to_tile_x(fix_lon, z);
	return tile_x_to_lon( fix_tile_x + (x - fix_scr_x) / 256.0, z );
}

double Cartographer::scr_y_to_lat(double y, double z,
	map_info::projection_t projection, double fix_lat, double fix_scr_y)
{
	double fix_tile_y = lat_to_tile_y(fix_lat, z, projection);
	return tile_y_to_lat( fix_tile_y + (y - fix_scr_y) / 256.0, z, projection );
}

coord Cartographer::Point(const coord &pt, double a, double s)
{
	// brng -> a
	// dist -> s
	// a -> c_a
	// b -> c_b
	// f -> c_f

	const double A1 = a * M_PI / 180.0;
	const double sin_A1 = sin(A1);
	const double cos_A1 = cos(A1);

	const double tan_u1 = c_k * tan(pt.lat * M_PI / 180.0);
	const double cos_u1 = 1.0 / sqrt(1.0 + tan_u1 * tan_u1);
	const double sin_u1 = tan_u1 * cos_u1;
	
	const double sigma1 = atan2(tan_u1, cos_A1);
	const double sin_A = cos_u1 * sin_A1;
	const double cos2_A = 1.0 - sin_A * sin_A;
	
	const double u2 = c_eb2 * cos2_A;
	const double A = 1.0 + u2 / 16384.0 * (4096 + u2 * (-768 + u2 * (320 - 175 * u2)));
	const double B = u2 / 1024 * (256 + u2 * (-128 + u2 * (74 - 47 * u2)));

	double sigma = s / (c_b * A);
	double sigmaP = 2.0 * M_PI;
	double sin_sigma;
	double cos_sigma;
	double cos2sigmaM;

	while (abs(sigma - sigmaP) > 1e-12)
	{
		cos2sigmaM = cos(2.0 * sigma1 + sigma);
		sin_sigma = sin(sigma);
		cos_sigma = cos(sigma);

		const double delta_sigma = B * sin_sigma
			* (cos2sigmaM + B / 4.0 * (cos_sigma * (-1.0 + 2.0 * cos2sigmaM * cos2sigmaM)
			- B / 6 * cos2sigmaM * (-3.0 + 4.0 * sin_sigma * sin_sigma)
			* (-3.0 + 4.0 * cos2sigmaM * cos2sigmaM)));

		sigmaP = sigma;
		sigma = s / (c_b * A) + delta_sigma;
	}

	const double tmp = sin_u1 * sin_sigma - cos_u1 * cos_sigma * cos_A1;
	const double lat2 = atan2(
		sin_u1 * cos_sigma + cos_u1 * sin_sigma * cos_A1,
		c_k * sqrt(sin_A * sin_A + tmp * tmp));
	const double lambda = atan2(sin_sigma * sin_A1,
		cos_u1 * cos_sigma - sin_u1 * sin_sigma * cos_A1);
	const double C = c_f / 16.0 * cos2_A * (4.0 + c_f * (4.0 - 3.0 * cos2_A));
	const double L = lambda - (1.0 - C) * c_f * sin_A
		* (sigma + C * sin_sigma * (cos2sigmaM + C * cos_sigma
		* (-1.0 + 2.0 * cos2sigmaM * cos2sigmaM)));
	
	const double revAz = atan2(sin_A, tmp);

	return coord(lat2 * 180.0 / M_PI, pt.lon + L * 180.0 / M_PI);
}

double Cartographer::DistancePrec(const coord &pt1, const coord &pt2,
	double *p_azi1, double *p_azi2, double accuracy_in_m)
{
	/***
		Функция служит для решения обратной геодезической задачи:
		расчёт кратчайшего расстояния между двумя точками,
		начального и обратного азимутов.

		Решается задача по способу Бесселя согласно его описания
		в "Курсе сфероидической геодезии" В.П.Морозова сс.133-135.
		
			(Для желающих найти что-либо получше: ищите алгоритм по методу
			Vincenty (http://www.ga.gov.au/geodesy/datums/vincenty_inverse.jsp).
			На сегодня это наиболее применяемый метод, но я слишком поздно
			об этом узнал. Точность у обоих методов одинаковая, но, быть может,
			он окажется быстрее)

		В данном учебнике многие формулы используют константы для эллипсоида
		Красовского, принятого в СССР в 1940 г. Сегодня же более актуально
		использование эллипсоида WGS84. Это заставило автора вспомнить годы,
		проведённые в университете, и "покурить" учебник с целью приведения
		формул к универсальному виду. Что и было сделано.
		
		Теперь данный алгоритм не привязан к конкретному эллипсоиду (лишь бы
		он был двухосный и не повёрнутый - это, конечно, тоже ограничение,
		поэтому для расчёта траектории баллистических ракет его использовать
		не рекомендуется :)).

		Параметры для эллипсоида задаются константами:
		c_a - большая полуось
			и
		c_f - степень сжатия (flattening)
		
		Остальные константы вычисляются на основании этих двух.
	***/

	double s; /* Вычисленное расстояние */
	double A1; /* Начальный азимут (в радианах) */
	double A2; /* Обратный азимут (в радианах) */

	/* Переводим точность из мм (по экватору) в радианы */
	const double accuracy = accuracy_in_m / c_a;

	/*
		B1,L1,B2,L2 - геодезические широта и долгота точек, выраженные в радианах
		c_e2 - квадрат эксцентриситета эллипса Земли (c_e - эксцентриситет эллипса)
		V,W - основные сфероидические функции (V = W / c_k)
		u - приведённая широта точки (с.11)
		A - азимут точки
		А0 - азимут геодезической линии в точке пересения с экватором
		lambda - разница долгот (с.97)
		sigma - сферическое расстояние (длина дуги большого круга,
			выраженная в частях радиуса шара) (с.97)
	*/

	/* 1. Подготовительные вычисления */
	const double sin_B1 = sin(pt1.lat * M_PI / 180.0);
	const double sin_B2 = sin(pt2.lat * M_PI / 180.0);
	const double cos_B1 = cos(pt1.lat * M_PI / 180.0);
	const double cos_B2 = cos(pt2.lat * M_PI / 180.0);

	const double l = (pt2.lon - pt1.lon) * M_PI / 180.0;

	const double W1 = sqrt(1.0 - c_e2 * sin_B1 * sin_B1);
	const double W2 = sqrt(1.0 - c_e2 * sin_B2 * sin_B2);
	const double sin_u1 = sin_B1 / W1 * c_k; /* = sin_B1 / V1 */
	const double sin_u2 = sin_B2 / W2 * c_k; /* = sin_B2 / V2 */
	const double cos_u1 = cos_B1 / W1;
	const double cos_u2 = cos_B2 / W2;
	const double a1 = sin_u1 * sin_u2;
	const double a2 = cos_u1 * cos_u2;
	const double b1 = cos_u1 * sin_u2;
	const double b2 = sin_u1 * cos_u2;

	/* 2. Последовательные приближения */
	double delta = 0.0;

	int count = 0;

	while (1)
	{
		++count;

		/* Разница долгот */
		const double lambda = l + delta;
		const double sin_lambda = sin(lambda);
		const double cos_lambda = cos(lambda);
		
		/* Начальный азимут */
		const double p = cos_u2 * sin_lambda;
		const double q = b1 - b2 * cos_lambda;
		A1 = atan2(p, q);
		if (A1 < 0.0)
			A1 += 2.0 * M_PI;

		/* Сферическое расстояние */
		const double sin_sigma = p * sin(A1) + q * cos(A1);
		const double cos_sigma = a1 + a2 * cos_lambda;
		double sigma = atan2(sin_sigma, cos_sigma);

		/* Азимут линии на экваторе */
		const double sin_A0 = cos_u1 * sin(A1);
		const double cos2_A0 = 1.0 - sin_A0 * sin_A0;

		const double x = 2.0 * a1 - cos2_A0 * cos_sigma;
		
		/*
			alpha = (1/2 * e^2 + 1/8 * e^4 + 1/16 * e^6 + ...)
				- (1/16 * e^4 + 1/16 * e^6 + ...) * cos^2(A0)
				+ (3/128 * e^6 + ...) * cos^4(A0)
				- ...
			
			Для эллипсоида Красовского (из учебника):
			const double alpha =
				(33523299 - (28189 - 70 * cos2_A0) * cos2_A0) * 0.0000000001;
		*/
		const double alpha =
			(0.5 * c_e2 + 0.125 * c_e4 + 0.0625 * c_e6)
			- (0.0625 * (c_e4 + c_e6) - 0.0234375 * c_e6 * cos2_A0) * cos2_A0;

		/*
			beta' = 2beta / Cos^2(A0)
			beta = (1/32 * e^4 + 1/32 * e^6 + ...) * cos^2(A0)
				- (1/64 * e^6 + ...) * cos^4(A0)
				+ ...
			=> beta' = (1/16 * e^4 + 1/16 * e^6 + ...)
				- (1/32 * e^6 + ...) * cos^2(A0)
				+ ...
			
			Для эллипсоида Красовского (из учебника):
			const double beta_ = (28189 - 94 * cos2_A0) * 0.0000000001;
		*/
		const double beta_ =
			0.0625 * (c_e4 + c_e6) - (0.03125 * c_e6) * cos2_A0;

		const double prev_delta = delta;
		delta = (alpha * sigma - beta_ * x * sin_sigma) * sin_A0;
			
		if (delta < 0.0 && prev_delta > 0.0 || delta > 0.0 && prev_delta < 0.0)
		{
			/*
				Если дельта начинает метаться, значит, мы попали
				в точку-антипод и область вокруг неё!

				В теории (по учебнику) точки-антиподы легко обнаружить
				на ранней стадии: lambda = PI, u1 = -u2. А потом,
				соответственно, их решить математическим способом.
				В реальности дело обстоит сложнее. Существует большая
				область вокруг точки-антипода, для которой данный метод
				не подходит. По долготе эта область начинается на расстоянии
				~179.3965 градусов от первой точки. Если вторая точка лежит
				до этой точки - кратчайший путь будет пролегать вдоль экватора,
				если после неё - начинается плавное смещение к пути через полюс.
				К самим точкам-антиподам кратчайшее расстояние есть всегда
				константа и проходит через любой из полюсов.
				
				179.3965 - это 67 км до точки-антипода. И каким-либо образом
				пренебречь рассчётами для метода с погрешностью в сантиметрах
				не простительно.
				
				Нашёл простое, но медленное решение: разделить путь на два
				отрезка. Измерить каждый, вычислить сумму. Вопрос лишь в том,
				в какой точке разделить отрезок?

				Методом приближений: нахожу расстояния через полюс и через
				экватор. Сравниваю, точку, в которой расстояние получилось
				большим, приближаю к другой. Так продолжаю до требуемой
				точности.

				Приближаю на половину разницы широт - не оптимально, но работает!
			*/
			
			const double lonM = (pt1.lon + pt2.lon) / 2.0;
			
			coord ptM1(0.0, lonM);
			coord ptM2(90.0, lonM);
			double aziM1_1, aziM1_2;
			double aziM2_1, aziM2_2;

			double s1 = DistancePrec(pt1, ptM1, &aziM1_1, NULL, accuracy_in_m)
				+ DistancePrec(ptM1, pt2, NULL, &aziM1_2, accuracy_in_m);
			double s2 = DistancePrec(pt1, ptM2, &aziM2_1, NULL, accuracy_in_m)
				+ DistancePrec(ptM2, pt2, NULL, &aziM2_2, accuracy_in_m);

			while ( abs(s2 - s1) > accuracy_in_m )
			{
				if (s1 > s2)
				{
					ptM1.lat = (ptM1.lat + ptM2.lat) / 2.0;
					s1 = DistancePrec(pt1, ptM1, &aziM1_1, NULL, accuracy_in_m)
						+ DistancePrec(ptM1, pt2, NULL, &aziM1_2, accuracy_in_m);
				}
				else
				{
					ptM2.lat = (ptM1.lat + ptM2.lat) / 2.0;
					s2 = DistancePrec(pt1, ptM2, &aziM2_1, NULL, accuracy_in_m)
						+ DistancePrec(ptM2, pt2, NULL, &aziM2_2, accuracy_in_m);
				}
			}

			if (s1 < s2)
			{
				if (p_azi1) *p_azi1 = aziM1_1;
				if (p_azi2) *p_azi2 = aziM1_2;
				s = s1;
			}
			else
			{
				if (p_azi1) *p_azi1 = aziM2_1;
				if (p_azi2) *p_azi2 = aziM2_2;
				s = s2;
			}

			return s;
		}

		/* Достигли требуемой точности */
		if (abs(delta - prev_delta) < accuracy || count >= 10)
		{
			/***
				Расстояние рассчитываем по формуле:
				s = A * sigma + (B_ * x + C_ * y) * sin_sigma;
				
				sigma, x - уже рассчитали
				A, B_, C_, y - рассчитаем ниже
			***/

			/*
				A = b * (1 + 1/4 * k^2 - 3/64 * k^4 + 5/256 * k^6 - ...)
					= b * (1 + k^2 * (1/4 - 3/64 * k^2 + 5/256 * k^4 - ...))
				
				где k^2 = e'^2 * Cos^2(A0)
					(const double k2 = c_eb2 * cos2_A0;)
				
				Примеры расчётов (выносим константы вперёд):
				
				Если переменные c_xxx не определены на стадии компиляции:
				const double A_HI = c_b * (1.0 + k2 * (0.25 + k2 * (-0.046875 + 0.01953125 * k2)));
				const double A_LO = c_b * (1.0 + k2 * (0.25 - 0.046875 * k2));
				
				Если c_xxx - статические константы:
				const double A_HI = c_b + (0.25 * c_b_eb2 + (-0.046875 * c_b_eb4 + 0.01953125 * c_b_eb6 * cos2_A0) * cos2_A0) * cos2_A0;
				const double A_LO = c_b + (0.25 * c_b_eb2 - 0.046875 * c_b_eb4 * cos2_A0) * cos2_A0;

				Для эллипсоида Красовского (из учебника):
				const double A = 6356863.020 + (10708.949 - 13.474 * cos2_A0) * cos2_A0;
			*/
			const double A = c_b + (0.25 * c_b_eb2 + (-0.046875 * c_b_eb4 + 0.01953125 * c_b_eb6 * cos2_A0) * cos2_A0) * cos2_A0;

			/*
				B' = 2B / Cos^2(A0)
				B = b * (1/8 * k^2 - 1/32 * k^4 + 15/1024 * k^6 - ...)
					= b * k^2 * (1/8 - 1/32 * k^2 + 15/1024 * k^4 - ...)
				=> B' = b * e'^2 * (1/4 - 1/16 * k^2 + 15/512 * k^4 - ...)

				Примеры расчётов:

				Если переменные c_xxx не определены на стадии компиляции:
				const double B_HI = c_b * c_eb2 * (1.0 / 4.0 - k2 / 16.0 + 15.0 / 512.0 * k2 * k2);
				const double B_LO = c_b * c_eb2 * (1.0 / 4.0 - k2 / 16.0);

				Если c_xxx - статические константы:
				const double B_HI = 0.25 * c_b_eb2 + (-0.0625 * c_b_eb4 + 0.029296875 * c_b_eb6 * cos2_A0) * cos2_A0;
				const double B_LO = 0.25 * c_b_eb2 - 0.0625 * c_b_eb4 * cos2_A0;

				Для эллипсоида Красовского (из учебника):
				const double B_ = 10708.938 - 17.956 * cos2_A0;
			*/
			const double B_ = 0.25 * c_b_eb2 + (-0.0625 * c_b_eb4 + 0.029296875 * c_b_eb6 * cos2_A0) * cos2_A0;

			/*
				C' = 2C / Cos^4(A0)
				C = b * (1/128 * k^4 - 3/512 * k^6 + ...)
					= b * k^4 * (1/128 - 3/512 * k^2 + ...)
				=> C' = b * e'^4 * (1/64 - 3/256 * k^2 + ...)

				Примеры расчётов:

				Если переменные c_xxx не определены на стадии компиляции:
				const double C_HI = c_b_eb4 * (0.015625 - 0.01171875 * k2);
				const double C_LO = 0.015625 * c_b_eb4;

				Если c_xxx - статические константы:
				const double C_HI = 0.015625 * c_b_eb4 - 0.01171875 * c_b_eb6 * cos2_A0;
				const double C_LO = 0.015625 * c_b_eb4;

				Для эллипсоида Красовского (из учебника):
				const double C_ = 4.487;
			*/
			const double C_ = 0.015625 * c_b_eb4 - 0.01171875 * c_b_eb6 * cos2_A0;

			const double y = (cos2_A0 * cos2_A0 - 2 * x * x) * cos_sigma;
			s = A * sigma + (B_ * x + C_ * y) * sin_sigma;
			
			/* Обратный азимут.
				По учебнику обратный азимут - это азимут, с которым мы приходим
				в конечную точку. Мне показалось более правильным считать такой
				азимут азимутом, с которым мы будем возвращаться назад - это дело
				лишь смены знака в выражениях p2 и q2 */
			const double p2 = - cos_u1 * sin_lambda;
			const double q2 = b2 - b1 * cos_lambda;

			A2 = atan2(p2, q2);
			if (A2 < 0.0)
				A2 += 2.0 * M_PI;


			break;
		}
	} /* while (1) */

	if (p_azi1)
		*p_azi1 = A1 * 180.0 / M_PI;

	if (p_azi2)
		*p_azi2 = A2 * 180.0 / M_PI;

	return s;
}

double Cartographer::DistanceFast(const cart::coord &pt1, const cart::coord &pt2)
{
	/***
		Рассчёт расстояния через нахождение координат x,y,z точек,
		вычисление расстояния между ними по теореме Пифагора,
		и нахождениия радиуса по полученной хорде
	***/

	/*
		v - радиус кривизны первого вертикала на данной широте
	*/

	/* Координаты первой точки */
	const double sin_B1 = sin(pt1.lat * M_PI / 180);
	const double cos_B1 = cos(pt1.lat * M_PI / 180);
	const double sin_L1 = sin(pt1.lon * M_PI / 180);
	const double cos_L1 = cos(pt1.lon * M_PI / 180);
	
	const double v1 = c_a / sqrt(1.0 - c_e2 * sin_B1 * sin_B1);

	const double x1 = v1 * cos_B1 * cos_L1;
	const double y1 = v1 * cos_B1 * sin_L1;
	const double z1 = (1.0 - c_e2) * v1 * sin_B1;

	/* Координаты второй точки */
	const double sin_B2 = sin(pt2.lat * M_PI / 180);
	const double cos_B2 = cos(pt2.lat * M_PI / 180);
	const double sin_L2 = sin(pt2.lon * M_PI / 180);
	const double cos_L2 = cos(pt2.lon * M_PI / 180);

	const double v2 = c_a / sqrt(1.0 - c_e2 * sin_B2 * sin_B2);

	const double x2 = v2 * cos_B2 * cos_L2;
	const double y2 = v2 * cos_B2 * sin_L2;
	const double z2 = (1.0 - c_e2) * v2 * sin_B2;

	/* Расстояние между точками */
	const double d = sqrt( (x2 - x1) * (x2 - x1)
		+ (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1) );

	/* Длина дуги */
	const double r = v1 < v2 ? v1 : v2; //(v1 + v2) / 2.0;
	double s = 2.0 * r * asin(0.5 * d / r);

	return s;
}

#if 0
void Cartographer::sort_queue(tiles_queue &queue, my::worker::ptr worker)
{
	tile::id fix_tile; /* Тайл в центре экрана */

	/* Копируем все нужные параметры, обеспечив блокировку */
	{
		unique_lock<recursive_mutex> lock(params_mutex_);

		fix_tile.map_id = active_map_id_;
		fix_tile.z = (int)(z_ + 0.5);
		fix_tile.x = (wxCoord)lon_to_tile_x(fix_lon_, (double)fix_tile.z);
		fix_tile.y = (wxCoord)lat_to_tile_y(fix_lat_, (double)fix_tile.z,
			maps_[fix_tile.map_id].projection);
	}

	sort_queue(queue, fix_tile, worker);
}

void Cartographer::sort_queue(tiles_queue &queue,
	const tile::id &fix_tile, my::worker::ptr worker)
{
	if (worker)
	{
		unique_lock<mutex> lock(worker->get_mutex());

		queue.sort( boost::bind(
			&Cartographer::sort_by_dist, fix_tile, _1, _2) );
	}
}

bool Cartographer::sort_by_dist( tile::id fix_tile,
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
		return abs(first_id.z - fix_tile.z)
			< abs(second_id.z - fix_tile.z);

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
	return sqrt( (double)(dx1*dx1 + dy1*dy1) )
		< sqrt( (double)(dx2*dx2 + dy2*dy2) );
}
#endif

void Cartographer::paint_debug_info(wxDC &gc, int width, int height)
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

void Cartographer::paint_debug_info(wxGraphicsContext &gc, int width, int height)
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
void Cartographer::paint_debug_info_int(DC &gc, int width, int height)
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
	GeoToDegrees(fix_lat_, &d, &m, &s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lat: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;

	GeoToDegrees(fix_lon_, &d, &m, &s);
	__swprintf(buf, sizeof(buf)/sizeof(*buf), L"lon: %dº %d\' %0.2f\"", d, m, s);
	gc.DrawText(buf, x, y), y += 12;
}

void Cartographer::repaint(wxPaintDC &dc)
{
	unique_lock<mutex> lock1(paint_mutex_);
	unique_lock<recursive_mutex> lock2(params_mutex_);

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
	map_info map = maps_[map_id];

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
		unique_lock<shared_mutex> lock(cache_mutex_);

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

		/* Сортируем */
		////

		/* Устанавливаем итераторы загрузчиков на начало. Будим их */
		file_iterator_ = server_iterator_ = cache_.begin();
		wake_up(file_loader_);
		wake_up(server_loader_);

		cache_active_tiles_ = tiles_count;

		/*-
		wchar_t buf[200];
		std::wstring str;
		int z = 0;
		int count1 = 0;
		int count2 = 0;

		for (tiles_cache::iterator iter = cache_.begin();
			iter != cache_.end(); ++iter)
		{
			tile::id tile_id = iter->key();
			tile &ti = *(iter->value().get());
			tile_id = tile_id;
			if (z != tile_id.z && z != 0)
			{
				__swprintf(buf, sizeof(buf)/sizeof(*buf), L" %d:(%d,%d)",
					z, count1, count2);
				str += buf;
				count2 = 0;
			}
			z = tile_id.z;
			++count1;
			++count2;
		}
		__swprintf(buf, sizeof(buf)/sizeof(*buf), L" %d:(%d,%d)",
			z, count1, count2);
		str += buf;
		str = str;
		-*/
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

	/* Меняем проекцию на проекцию экрана: 0..width, 0..height */
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
	}

	/* Показываем fix-точку при изменении масштаба */
	if (fix_step_ || dz > 0.1)
	{
		if (fix_step_ == 0)
		{
			fix_step_ = def_min_anim_steps_ ? 2 * def_min_anim_steps_ : 1;
			fix_alpha_ = 1.0;
		}
		else
		{
			fix_alpha_ -= fix_alpha_ / fix_step_;
			--fix_step_;
		}

		glLineWidth(3);
		//glEnable(GL_LINE_SMOOTH);
		glColor4d( 1.0, 0.0, 0.0, fix_alpha_ );

		glBegin(GL_LINES);
			glVertex3d( fix_scr_x - 8, fix_scr_y - 8, 0.0 );
			glVertex3d( fix_scr_x + 8, fix_scr_y + 8, 0.0 );
			glVertex3d( fix_scr_x - 8, fix_scr_y + 8, 0.0 );
			glVertex3d( fix_scr_x + 8, fix_scr_y - 8, 0.0 );
		glEnd();

		/*-
		glBegin(GL_LINES);
			glVertex3d( fix_scr_x, fix_scr_y - 8, 0.0 );
			glVertex3d( fix_scr_x, fix_scr_y + 8, 0.0 );
			glVertex3d( fix_scr_x - 8, fix_scr_y, 0.0 );
			glVertex3d( fix_scr_x + 8, fix_scr_y, 0.0 );
		glEnd();
		-*/

		/*-
		glBegin(GL_LINE_LOOP);
			glVertex3d( fix_scr_x - 10, fix_scr_y - 10, 0.0 );
			glVertex3d( fix_scr_x + 10, fix_scr_y - 10, 0.0 );
			glVertex3d( fix_scr_x + 10, fix_scr_y + 10, 0.0 );
			glVertex3d( fix_scr_x - 10, fix_scr_y + 10, 0.0 );
		glEnd();
		-*/
	}

	/* Картинка пользователя */
	{
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

void Cartographer::move_fix_to_scr_xy(double scr_x, double scr_y)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	double w, h;
	get_viewport_size(&w, &h);

	fix_kx_ = scr_x / w;
	fix_ky_ = scr_y / h;
}

void Cartographer::set_fix_to_scr_xy(double scr_x, double scr_y)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	double w, h;
	get_viewport_size(&w, &h);

	fix_lat_ = scr_y_to_lat(scr_y, z_, maps_[active_map_id_].projection,
		fix_lat_, h * fix_ky_);
	fix_lon_ = scr_x_to_lon(scr_x, z_, fix_lon_, w * fix_kx_);

	move_fix_to_scr_xy(scr_x, scr_y);
}

void Cartographer::on_paint(wxPaintEvent &event)
{
	wxPaintDC dc(this);
	
	repaint(dc);

	event.Skip(false);
}

void Cartographer::on_erase_background(wxEraseEvent& event)
{
	event.Skip(false);
}

void Cartographer::on_size(wxSizeEvent& event)
{
	Update();
}

void Cartographer::on_left_down(wxMouseEvent& event)
{
	SetFocus();

	set_fix_to_scr_xy( (double)event.GetX(), (double)event.GetY() );

	move_mode_ = true;

	#ifdef BOOST_WINDOWS
	CaptureMouse();
	#endif

	Update();
}

void Cartographer::on_left_up(wxMouseEvent& event)
{
	if (move_mode_)
	{
		double w, h;
		get_viewport_size(&w, &h);

		//set_fix_to_scr_xy( w/2.0, h/2.0 );
		move_mode_ = false;

		#ifdef BOOST_WINDOWS
		ReleaseMouse();
		#endif

		Update();
	}
}

void Cartographer::on_capture_lost(wxMouseCaptureLostEvent& event)
{
	move_mode_ = false;
}

void Cartographer::on_mouse_move(wxMouseEvent& event)
{
	if (move_mode_)
	{
		move_fix_to_scr_xy( (double)event.GetX(), (double)event.GetY() );
		Update();
	}
}

void Cartographer::on_mouse_wheel(wxMouseEvent& event)
{
	{
		unique_lock<recursive_mutex> lock(params_mutex_);

		int z = (int)new_z_ + event.GetWheelRotation() / event.GetWheelDelta();

		if (z < 1)
			z = 1;

		if (z > 30)
			z = 30.0;
		
		SetActiveZ(z);
	}

	Update();
}

GLuint Cartographer::load_texture(raw_image &image)
{
	GLuint id;

	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); /* GL_CLAMP_TO_EDGE */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); /* GL_CLAMP_TO_EDGE */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(),
		0, (GLenum)image.tag(), GL_UNSIGNED_BYTE, image.data());

	check_gl_error();

	++load_texture_debug_counter_;

	return id;
}

GLuint Cartographer::load_image_to_texture(image &img)
{
	GLuint texture_id = img.texture_id();

	if (texture_id == 0 && boost::this_thread::get_id() == paint_thread_id_)
	{
		texture_id = load_texture( img.raw() );
		img.raw().clear(false);
		img.set_texture_id(texture_id);
	}

	return texture_id;
}

void Cartographer::on_image_delete_proc(image &img)
{
	GLuint texture_id = img.texture_id();
	if (texture_id)
		delete_texture_later(texture_id);
}

} /* namespace cart */
