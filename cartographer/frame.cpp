#include "frame.h"

#ifdef BOOST_WINDOWS
	#if defined(_MSC_VER)
		#define __swprintf swprintf_s
	#else
		#define __swprintf snwprintf
	#endif
#else
	#define __swprintf swprintf
#endif

#include <math.h> /* sin, sqrt */
#include <wchar.h> /* swprintf */
#include <sstream>
#include <fstream>
#include <vector>
#include <locale>

#include <boost/bind.hpp>

namespace cartographer
{

BEGIN_EVENT_TABLE(Frame, wxGLCanvas)
	EVT_PAINT(Frame::on_paint)
	EVT_ERASE_BACKGROUND(Frame::on_erase_background)
	EVT_SIZE(Frame::on_size)
	EVT_LEFT_DOWN(Frame::on_left_down)
	EVT_LEFT_UP(Frame::on_left_up)
	EVT_MOUSE_CAPTURE_LOST(Frame::on_capture_lost)
	EVT_MOTION(Frame::on_mouse_move)
	EVT_MOUSEWHEEL(Frame::on_mouse_wheel)
	//EVT_KEY_DOWN(Frame::OnKeyDown)
END_EVENT_TABLE()

Frame::Frame(wxWindow *parent,
	const std::wstring &server_addr, const std::wstring &server_port,
	std::size_t cache_size, bool only_cache,
	const std::wstring &init_map, on_paint_proc_t on_paint_proc,
	int anim_period, int def_min_anim_steps)
	: wxGLCanvas(parent, wxID_ANY, NULL /* attribs */,
		wxDefaultPosition, wxDefaultSize,
		wxFULL_REPAINT_ON_RESIZE)
	, gl_context_(this)
	, magic_id_(0)
	, load_texture_debug_counter_(0)
	, delete_texture_debug_counter_(0)
	, cache_path_( fs::system_complete(L"cache").string() )
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
	, draw_tile_debug_counter_(0)
	, active_map_id_(0)
	, z_(1.0)
	, new_z_(z_)
	, z_step_(0)
	, screen_pos_()
	, center_pos_( ratio(0.5, 0.5) )
	, central_cross_step_(0)
	, central_cross_alpha_(0.0)
	, painter_debug_counter_(0)
	, move_mode_(false)
	, force_repaint_(false)
	, mouse_pos_()
	, system_font_id_(0)
	, paint_thread_id_( boost::this_thread::get_id() )
	, on_paint_handler_(on_paint_proc)
	, sprites_index_(0)
	, on_image_delete_( boost::bind(&Frame::on_image_delete_proc, this, _1) )
	, fonts_index_(0)
{
	try
	{
		SetCurrent(gl_context_);

		magic_init();

		system_font_id_ = CreateFont( wxFont(8,
			wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD) );


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
			&Frame::file_loader_proc, this, file_loader_) );

		/* Запускаем серверный загрузчик тайлов */
		if (!only_cache_)
		{
			server_loader_ = new_worker(L"server_loader"); /* Название - только для отладки */
			boost::thread( boost::bind(
				&Frame::server_loader_proc, this, server_loader_) );
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
				&Frame::anim_thread_proc, this, animator_) );
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

Frame::~Frame()
{
	if (!finish())
		Stop();

	cache_.clear();
	delete_textures();
	magic_deinit();
	sprites_.clear();

	assert( load_texture_debug_counter_ == delete_texture_debug_counter_ );
}

void Frame::Stop()
{
	/* Оповещаем о завершении работы */
	lets_finish();

	/* Освобождаем ("увольняем") всех "работников" */
	dismiss(file_loader_);
	dismiss(server_loader_);
	dismiss(animator_);

	/* Ждём завершения */
	#ifndef NDEBUG
	debug_wait_for_finish(L"Frame", posix_time::seconds(5));
	#endif

	wait_for_finish();
}

void Frame::Update()
{
	//Refresh(false);
}

int Frame::GetMapsCount()
{
	return (int)maps_.size();
}

map_info Frame::GetMapInfo(int index)
{
	map_info map;
	maps_list::iterator iter = maps_.begin();

	while (index-- && iter != maps_.end())
		++iter;

	if (iter != maps_.end())
		map = iter->second;

	return map;
}

map_info Frame::GetActiveMapInfo()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return maps_[active_map_id_];
}

bool Frame::SetActiveMapByIndex(int index)
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

bool Frame::SetActiveMapByName(const std::wstring &map_name)
{
	maps_name_to_id_list::iterator iter = maps_name_to_id_.find(map_name);

	if (iter == maps_name_to_id_.end())
		return false;

	unique_lock<recursive_mutex> lock(params_mutex_);
	active_map_id_ = iter->second;
	Update();

	return true;
}

point Frame::CoordToScreen(const coord &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	const projection pr = maps_[active_map_id_].pr;

	return coord_to_screen( pt, pr, z_,
		screen_pos_.get_world_pos(pr), center_pos_.get_pos() );
}

point Frame::CoordToScreen(fast_point &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	const projection pr = maps_[active_map_id_].pr;

	return pt.get_screen_pos( pr, z_,
		screen_pos_.get_world_pos(pr), center_pos_.get_pos() );
}

coord Frame::ScreenToCoord(const point &pos)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	const projection pr = maps_[active_map_id_].pr;

	return screen_to_coord(pos, pr, z_,
		screen_pos_.get_world_pos(pr),
		center_pos_.get_pos() );
}

double Frame::GetActiveZ(void)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return z_;
}

void Frame::SetActiveZ(int z)
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

void Frame::ZoomIn()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	SetActiveZ( new_z_ + 1.0 );
}

void Frame::ZoomOut()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	SetActiveZ( new_z_ - 1.0 );
}

fast_point Frame::GetScreenPos()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return screen_pos_;
}

void Frame::MoveTo(int z, const coord &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
	SetActiveZ(z);
}

int Frame::LoadImageFromFile(const std::wstring &filename)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);

	sprite::ptr sprite_ptr( new sprite(on_image_delete_) );
	sprites_[++sprites_index_] = sprite_ptr;

	if (!sprite_ptr->load_from_file(filename))
	{
		sprites_.erase(sprites_index_--);
		return 0;
	}

	/* Загружаем текстуру, если это возможно */
	if (boost::this_thread::get_id() == paint_thread_id_)
	{
		sprite_ptr->convert_to_gl_texture();
		check_gl_error();
		++load_texture_debug_counter_;
	}

	return sprites_index_;
}

int Frame::LoadImageFromMem(const void *data, std::size_t size)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);

	sprite::ptr sprite_ptr( new sprite(on_image_delete_) );
	sprites_[++sprites_index_] = sprite_ptr;

	if (!sprite_ptr->load_from_mem(data, size))
	{
		sprites_.erase(sprites_index_--);
		return 0;
	}

	/* Загружаем текстуру, если это возможно */
	if (boost::this_thread::get_id() == paint_thread_id_)
	{
		sprite_ptr->convert_to_gl_texture();
		check_gl_error();
		++load_texture_debug_counter_;
	}

	return sprites_index_;
}
int Frame::LoadImageFromRaw(const unsigned char *data,
	int width, int height, bool with_alpha)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);

	sprite::ptr sprite_ptr( new sprite(on_image_delete_) );
	sprites_[++sprites_index_] = sprite_ptr;

	sprite_ptr->load_from_raw(data, width, height, with_alpha);

	/* Загружаем текстуру, если это возможно */
	if (boost::this_thread::get_id() == paint_thread_id_)
	{
		sprite_ptr->convert_to_gl_texture();
		check_gl_error();
		++load_texture_debug_counter_;
	}

	return sprites_index_;
}

void Frame::DeleteImage(int image_id)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);
	sprites_.erase(image_id);
}

ratio Frame::GetImageCenter(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? ratio() : iter->second->center();
}

void Frame::SetImageCenter(int image_id, const ratio &pos)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_center(pos);
}

point Frame::GetImageCentralPoint(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? point() : iter->second->central_point();
}

void Frame::SetImageCentralPoint(int image_id, const point &pos)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_central_point(pos);
}

size Frame::GetImageSize(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? size() : iter->second->get_size();
}

ratio Frame::GetImageScale(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? ratio() : iter->second->scale();
}

void Frame::SetImageScale(int image_id, const ratio &scale)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_scale(scale);
}

void Frame::DrawImage(int image_id, const point &pos, const ratio &scale)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
	{
		sprite::ptr sprite_ptr = iter->second;

		/* Загружаем текстуру, если она не была загружена */
		GLuint texture_id = sprite_ptr->texture_id();
		if (texture_id == 0)
		{
			texture_id = sprite_ptr->convert_to_gl_texture();
			check_gl_error();
			++load_texture_debug_counter_;
		}

		const ratio total_scale = scale * sprite_ptr->scale();
		double w = sprite_ptr->raw().width() * total_scale.kx;
		double h = sprite_ptr->raw().height() * total_scale.ky;

		const ratio center = sprite_ptr->center();
		double x = pos.x - w * center.kx;
		double y = pos.y - h * center.ky;

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

int Frame::CreateFont(const wxFont &wxfont)
{
	unique_lock<shared_mutex> lock(fonts_mutex_);

	font::ptr font_ptr( new font(wxfont, on_image_delete_) );
	fonts_[++fonts_index_] = font_ptr;

	/* Загружаем текстуры символов, если это возможно */
	if (boost::this_thread::get_id() == paint_thread_id_)
		font_ptr->prepare(L" -~°А-Яа-яЁё"); /* ASCII + русские буквы */

	return fonts_index_;
}

void Frame::DeleteFont(int font_id)
{
	unique_lock<shared_mutex> lock(fonts_mutex_);
	fonts_.erase(font_id);
}

size Frame::DrawText(int font_id, const std::wstring &str, const point &pos,
	const color &text_color, const ratio &center, const ratio &scale)
{
	shared_lock<shared_mutex> lock(fonts_mutex_);

	fonts_list::iterator iter = fonts_.find(font_id);
	size sz;

	if (iter != fonts_.end())
	{
		font::ptr font_ptr = iter->second;

		glColor4dv(&text_color.r);
		sz = font_ptr->draw(str, pos, center, scale);
	}

	magic_exec();

	check_gl_error();

	return sz;
}

void Frame::magic_init()
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

void Frame::magic_deinit()
{
	glDeleteTextures(1, &magic_id_);
	check_gl_error();
}

void Frame::magic_exec()
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

void Frame::check_gl_error()
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

void Frame::load_textures()
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

void Frame::delete_texture_later(GLuint texture_id)
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

void Frame::delete_texture(GLuint texture_id)
{
	glDeleteTextures(1, &texture_id);
	check_gl_error();
	++delete_texture_debug_counter_;
}

void Frame::delete_textures()
{
	unique_lock<mutex> lock(delete_texture_mutex_);

	while (delete_texture_queue_.size())
	{
		GLuint texture_id = delete_texture_queue_.front();
		delete_texture_queue_.pop_front();
		delete_texture(texture_id);
	}
}

bool Frame::check_tile_id(const tile::id &tile_id)
{
	int sz = tiles_count(tile_id.z);

	return tile_id.z >= 1
		&& tile_id.x >= 0 && tile_id.x < sz
		&& tile_id.y >= 0 && tile_id.y < sz;
}

tile::ptr Frame::find_tile(const tile::id &tile_id)
{
	/* Блокируем кэш для чтения */
	shared_lock<shared_mutex> lock(cache_mutex_);

	tiles_cache::iterator iter = cache_.find(tile_id);

	return iter == cache_.end() ? tile::ptr() : iter->value();
}

void Frame::paint_tile(const tile::id &tile_id, int level)
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

tile::ptr Frame::get_tile(const tile::id &tile_id)
{
	//if ( !check_tile_id(tile_id))
	//	return tile::ptr();

	tile::ptr tile_ptr = find_tile(tile_id);

	return tile_ptr;
}

/* Загрузчик тайлов с диска. При пустой очереди - засыпает */
void Frame::file_loader_proc(my::worker::ptr this_worker)
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
			if (!tile_ptr->load_from_file(filename))
			{
				tile_ptr->set_state(tile::ready);
				main_log << L"Ошибка загрузки wxImage: " << filename << main_log;
			}
		}

	} /* while (!finish()) */
}

/* Загрузчик тайлов с сервера. При пустой очереди - засыпает */
void Frame::server_loader_proc(my::worker::ptr this_worker)
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

void Frame::anim_thread_proc(my::worker::ptr this_worker)
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

void Frame::get(my::http::reply &reply,
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

unsigned int Frame::load_and_save(const std::wstring &request,
	const std::wstring &local_filename)
{
	my::http::reply reply;

	get(reply, request);

	if (reply.status_code == 200)
		reply.save(local_filename);

	return reply.status_code;
}

unsigned int Frame::load_and_save_xml(const std::wstring &request,
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
void Frame::paint_debug_info_int(DC &gc, int width, int height)
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

void Frame::repaint(wxPaintDC &dc)
{
	unique_lock<mutex> lock1(paint_mutex_);
	unique_lock<recursive_mutex> lock2(params_mutex_);

	++painter_debug_counter_;

	/* Измеряем скорость выполнения функции */
	anim_speed_sw_.start();

	/* Размеры окна + обновляем размеры экрана для центральной точки */
	const size screen_size = get_screen_size();
	center_pos_.set_size(screen_size);

	/* Активная карта */
	const int map_id = active_map_id_;
	const map_info map = maps_[map_id];

	/* Текущий масштаб. При перемещениях
		между масштабами - масштаб верхнего слоя */
	const int z_i = (int)z_;
	const double dz = z_ - (double)z_i; /* "Расстояние" от верхнего слоя */
	const double alpha = 1.0 - dz; /* Прозрачность верхнего слоя */
	int basis_z = z_i;

	/* Центральный тайл */
	const point central_tile = screen_pos_.get_tiles_pos(map.pr, z_i);

	/* Позиции экрана и его центра */
	const point screen_pos = screen_pos_.get_world_pos(map.pr);
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
		//point tile_corner(
		//	center_pos.x - (central_tile.x - central_tile_x_i) * 256.0,
		//	center_pos.y - (central_tile.y - central_tile_y_i) * 256.0);

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
				paint_tile( tile::id(map_id, basis_z_, x, y) );
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslated(-central_tile.x, -central_tile.y, 0.0);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	for (int x = z_i_tile_x1; x < z_i_tile_x2; ++x)
		for (int y = z_i_tile_y1; y < z_i_tile_y2; ++y)
			paint_tile( tile::id(map_id, z_i, x, y) );

	/* Меняем проекцию на проекцию экрана */
	{
		magic_exec();

		glColor4d(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(0.0, screen_size.width, -screen_size.height, 0.0, -1.0, 2.0);
		glScaled(1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	/* Картинка пользователя */
	if (on_paint_handler_)
		on_paint_handler_(z_, screen_size.width, screen_size.height);

	magic_exec();

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
	}

	{
		wchar_t buf[400];

		coord mouse_coord = screen_to_coord(
			mouse_pos_, map.pr, z_, screen_pos, center_pos);

		int lat_sign, lon_sign;
		int lat_d, lon_d;
		int lat_m, lon_m;
		double lat_s, lon_s;

		DDToDMS( mouse_coord,
			&lat_sign, &lat_d, &lat_m, &lat_s,
			&lon_sign, &lon_d, &lon_m, &lon_s);

		__swprintf(buf, sizeof(buf)/sizeof(*buf),
			L"z: %.1f | lat: %s%d°%02d\'%05.2f\" | lon: %s%d°%02d\'%05.2f\"",
			z_,
			lat_sign < 0 ? L"-" : L"", lat_d, lat_m, lat_s,
			lon_sign < 0 ? L"-" : L"", lon_d, lon_m, lon_s);

		DrawText(system_font_id_, buf,
			point(4.0, screen_size.height), cartographer::color(1.0, 1.0, 1.0),
			cartographer::ratio(0.0, 1.0));
	}

	glFlush();
	SwapBuffers();
	check_gl_error();

	//paint_debug_info(dc, width_i, height_i);

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

size Frame::get_screen_size()
{
	wxCoord w, h;
	GetClientSize(&w, &h);
	return size( (double)w, (double)h);
}

point Frame::get_screen_max_point()
{
	wxCoord w, h;
	GetClientSize(&w, &h);
	return point( (double)w, (double)h);
}

void Frame::move_screen_to(const point &pos)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	center_pos_.set_pos(pos);
}

void Frame::set_screen_pos(const point &pos)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	const projection pr = maps_[active_map_id_].pr;

	screen_pos_ = screen_to_coord(
		pos, pr, z_, screen_pos_.get_world_pos(pr),
		center_pos_.get_pos() );
	
	/* move_screen_to() */
	center_pos_.set_pos(pos);
}

void Frame::on_paint(wxPaintEvent &event)
{
	wxPaintDC dc(this);

	repaint(dc);

	event.Skip(false);
}

void Frame::on_erase_background(wxEraseEvent& event)
{
	event.Skip(false);
}

void Frame::on_size(wxSizeEvent& event)
{
	Update();
}

void Frame::on_left_down(wxMouseEvent& event)
{
	SetFocus();

	set_screen_pos( point(event.GetX(), event.GetY()) );

	move_mode_ = true;

	#ifdef BOOST_WINDOWS
	CaptureMouse();
	#endif

	Update();
}

void Frame::on_left_up(wxMouseEvent& event)
{
	if (move_mode_)
	{
		//set_screen_pos( point( get_screen_size() / 2.0 ) );
		move_mode_ = false;

		#ifdef BOOST_WINDOWS
		ReleaseMouse();
		#endif

		Update();
	}
}

void Frame::on_capture_lost(wxMouseCaptureLostEvent& event)
{
	move_mode_ = false;
}

void Frame::on_mouse_move(wxMouseEvent& event)
{
	mouse_pos_.x = (double)event.GetX();
	mouse_pos_.y = (double)event.GetY();

	if (move_mode_)
	{
		move_screen_to(mouse_pos_);
		Update();
	}
}

void Frame::on_mouse_wheel(wxMouseEvent& event)
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

void Frame::on_image_delete_proc(image &img)
{
	GLuint texture_id = img.texture_id();
	if (texture_id)
		delete_texture_later(texture_id);
}

} /* namespace cartographer */
