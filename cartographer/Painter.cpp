#include "Painter.h"

#include <wchar.h> /* swprintf */
//#include <sstream>
//#include <fstream>
//#include <vector>

#include <boost/bind.hpp>

namespace cartographer
{

Painter::Painter(wxWindow *parent, const std::wstring &server_addr,
	const std::wstring &init_map, std::size_t cache_size)
	: Base(parent, server_addr, init_map, cache_size)
	, sprites_index_(0)
	, fonts_index_(0)
	, system_font_id_(0)
{
	system_font_id_ = CreateFont(
		wxFont(8, wxFONTFAMILY_MODERN, /* fixed size */
			wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD) );

	update();
}

Painter::~Painter()
{
	if (!finish())
		stop();

	unique_lock<shared_mutex> lock(sprites_mutex_);
	sprites_.clear();
}

void Painter::SetPainter(on_paint_proc_t on_paint_proc)
{
	unique_lock<mutex> l(paint_mutex_);
	on_paint_handler_ = on_paint_proc;
}

void Painter::Update()
{
	update();
}

void Painter::Stop()
{
	stop();
}

int Painter::GetMapsCount()
{
	return (int)maps_.size();
}

map_info Painter::GetMapInfo(int index)
{
	map_info map;
	maps_list::iterator iter = maps_.begin();

	while (index-- && iter != maps_.end())
		++iter;

	if (iter != maps_.end())
		map = iter->second;

	return map;
}

map_info Painter::GetActiveMapInfo()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return maps_[map_id_];
}

bool Painter::SetActiveMapByIndex(int index)
{
	maps_list::iterator iter = maps_.begin();

	while (index-- && iter != maps_.end())
		++iter;

	if (iter == maps_.end())
		return false;

	unique_lock<recursive_mutex> lock(params_mutex_);
	map_id_ = iter->first;
	map_pr_ = iter->second.pr;

	update();

	return true;
}

bool Painter::SetActiveMapByName(const std::wstring &map_name)
{
	maps_name_to_id_list::iterator iter = maps_name_to_id_.find(map_name);

	if (iter == maps_name_to_id_.end())
		return false;

	unique_lock<recursive_mutex> lock(params_mutex_);
	map_id_ = iter->second;
	map_pr_ = maps_[map_id_].pr;

	update();

	return true;
}

point Painter::CoordToScreen(const coord &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	return coord_to_screen( pt, map_pr_, z_,
		screen_pos_.get_world_pos(map_pr_), center_pos_.get_pos() );
}

point Painter::CoordToScreen(fast_point &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	return pt.get_screen_pos( map_pr_, z_,
		screen_pos_.get_world_pos(map_pr_), center_pos_.get_pos() );
}

coord Painter::ScreenToCoord(const point &pos)
{
	unique_lock<recursive_mutex> lock(params_mutex_);

	return screen_to_coord(pos, map_pr_, z_,
		screen_pos_.get_world_pos(map_pr_),
		center_pos_.get_pos() );
}

double Painter::GetActiveZ(void)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return z_;
}

void Painter::SetActiveZ(int z)
{
	set_z(z);
}

void Painter::ZoomIn()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	SetActiveZ( new_z_ + 1.0 );
}

void Painter::ZoomOut()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	SetActiveZ( new_z_ - 1.0 );
}

fast_point Painter::GetScreenPos()
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	return screen_pos_;
}

void Painter::MoveTo(const coord &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
}

void Painter::MoveTo(int z, const coord &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
	SetActiveZ(z);
}

int Painter::LoadImageFromFile(const std::wstring &filename)
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

int Painter::LoadImageFromMem(const void *data, std::size_t size)
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
int Painter::LoadImageFromRaw(const unsigned char *data,
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

void Painter::DeleteImage(int image_id)
{
	unique_lock<shared_mutex> lock(sprites_mutex_);
	sprites_.erase(image_id);
}

ratio Painter::GetImageCenter(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? ratio() : iter->second->center();
}

void Painter::SetImageCenter(int image_id, const ratio &pos)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_center(pos);
}

point Painter::GetImageCentralPoint(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? point() : iter->second->central_point();
}

void Painter::SetImageCentralPoint(int image_id, const point &pos)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_central_point(pos);
}

size Painter::GetImageSize(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? size() : iter->second->get_size();
}

ratio Painter::GetImageScale(int image_id)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);
	return iter == sprites_.end() ? ratio() : iter->second->scale();
}

void Painter::SetImageScale(int image_id, const ratio &scale)
{
	shared_lock<shared_mutex> lock(sprites_mutex_);

	sprites_list::iterator iter = sprites_.find(image_id);

	if (iter != sprites_.end())
		iter->second->set_scale(scale);
}

void Painter::DrawImage(int image_id, const point &pos, const ratio &scale)
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

int Painter::CreateFont(const wxFont &wxfont)
{
	unique_lock<shared_mutex> lock(fonts_mutex_);

	font::ptr font_ptr( new font(wxfont, on_image_delete_) );
	fonts_[++fonts_index_] = font_ptr;

	/* Загружаем текстуры символов, если это возможно */
	if (boost::this_thread::get_id() == paint_thread_id_)
		font_ptr->prepare(L" -~°А-Яа-яЁё"); /* ASCII + русские буквы */

	return fonts_index_;
}

void Painter::DeleteFont(int font_id)
{
	unique_lock<shared_mutex> lock(fonts_mutex_);
	fonts_.erase(font_id);
}

size Painter::DrawText(int font_id, const std::wstring &str, const point &pos,
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

void Painter::after_repaint(const size &screen_size)
{
	/* Статус-строка */
	wchar_t buf[400];

	/* Позиции экрана и его центра */
	coord mouse_coord = screen_to_coord(
		mouse_pos_, map_pr_, z_,
		screen_pos_.get_world_pos(map_pr_), center_pos_.get_pos());

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

} /* namespace cartographer */
