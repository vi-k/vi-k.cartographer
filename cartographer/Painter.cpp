#include "Painter.h"

#include <wchar.h> /* swprintf */

#include <boost/bind.hpp>

namespace cartographer
{

extern my::log log;

Painter::Painter(wxWindow *parent, const std::wstring &server_addr,
	const std::wstring &init_map, std::size_t cache_size)
	: Base(parent, server_addr, init_map, cache_size)
	, sprites_index_(0)
	, MY_MUTEX_DEF(sprites_mutex_,true)
	, fonts_index_(0)
	, MY_MUTEX_DEF(fonts_mutex_,true)
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

void Painter::Stop()
{
	stop();
}

void Painter::Repaint()
{
	if (boost::this_thread::get_id() == paint_thread_id_)
		repaint();
}

void Painter::SetStatusHandler(on_status_proc_t on_status_proc)
{
	on_status_ = on_status_proc;
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
	log << L"MoveTo()" << log;

	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
}

void Painter::MoveTo(int z, const coord &pt)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
	set_z(z);
}

void Painter::MoveTo(const coord &pt, const ratio &center)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
	if (!move_mode_)
		center_pos_.set_rel_pos(center);
}

void Painter::MoveTo(int z, const coord &pt, const ratio &center)
{
	unique_lock<recursive_mutex> lock(params_mutex_);
	screen_pos_ = pt;
	if (!move_mode_)
		center_pos_.set_rel_pos(center);
	set_z(z);
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

void Painter::DrawImage(int image_id, const point &pos, const ratio &scale,
	const color &blend_color, double angle)
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
		const raw_image &raw = sprite_ptr->raw();

		const size offset = sprite_ptr->get_size() * sprite_ptr->center();

		/* Координаты левого верхнего и правого нижнего угла выводимой текстуры */
		point lt = -point(offset) * total_scale;
		point rb = point(raw.width() - offset.width, raw.height() - offset.height)
			* total_scale;
		point rt(rb.x, lt.y);
		point lb(lt.x, rb.y);

		const double a = angle * M_PI / 180.0;

		const size screen_offset = pos.as_size();

		lt = lt.rotate(a) + screen_offset;
		rt = rt.rotate(a) + screen_offset;
		rb = rb.rotate(a) + screen_offset;
		lb = lb.rotate(a) + screen_offset;

		glColor4dv(&blend_color.r);

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0); glVertex3d(lt.x, lt.y, 0);
			glTexCoord2d(1.0, 0.0); glVertex3d(rt.x, rt.y, 0);
			glTexCoord2d(1.0, 1.0); glVertex3d(rb.x, rb.y, 0);
			glTexCoord2d(0.0, 1.0); glVertex3d(lb.x, lb.y, 0);
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

void Painter::DrawSimpleCircle(const point &center, double radius,
	double line_width, const color &line_color, const color &fill_color)
{
	/* При малых радиусах - нет необходимости в мелком шаге */
	double step = 180.0 / (M_PI * radius);

	if (step < 1.0)
		step = 1.0;

	/* Применяем такое хитрое сравнение на случай,
		если step получился NAN или INF */
	if ( !(step < 60.0) )
		step = 60.0;

	step *= M_PI / 180.0;

	/* Сначала круг, затем окружность */
	for (int n = 0; n < 2; ++n)
	{
		if (n == 0)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glBegin(GL_POLYGON);
			glColor4dv(&fill_color.r);
		}
		else
		{
			glLineWidth(line_width);
			glBegin(GL_LINE_LOOP);
			glColor4dv(&line_color.r);
		}

		for (double a = 2.0 * M_PI; a > 0.0; a -= step)
		{
			point pos( center.x + radius * cos(a),
				center.y + radius * sin(a) );
			glVertex3d(pos.x, pos.y, 0);
		}

		glEnd();
	}
}

void Painter::DrawSimpleCircle(const coord &center, double radius_in_m,
	double line_width, const color &line_color, const color &fill_color)
{
	point center_pos = CoordToScreen(center);
	coord east_pt = Direct(center, 90.0, radius_in_m);
	point east_pos = CoordToScreen(east_pt);

	double radius = east_pos.x - center_pos.x;

	/* Проверяем случай пересечения линию перемены дат */
	if (radius < 0.0)
		radius += world_px_size(z_);

	DrawSimpleCircle( center_pos, radius,
		line_width, line_color, fill_color );
}

void Painter::DrawCircle(const coord &center, double radius_in_m,
	double line_width, const color &line_color, const color &fill_color)
{
	double step = 1.0;

	point center_pos = CoordToScreen(center);

	/* Сначала круг, затем окружность */
	for (int n = 0; n < 2; ++n)
	{
		if (n == 0)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glBegin(GL_POLYGON);
			glColor4dv(&fill_color.r);
		}
		else
		{
			glLineWidth(line_width);
			glBegin(GL_LINE_LOOP);
			glColor4dv(&line_color.r);
		}

		for (double a = 0.0; a < 360.0; a += step)
		{
			coord ptN = Direct(center, a, radius_in_m);
			point ptN_pos = CoordToScreen(ptN);

			if (a == 0.0)
			{
				/* При малых радиусах - нет необходимости в мелком шаге */
				double radius = center_pos.y - ptN_pos.y;

				step = 180.0 / (M_PI * radius);

				if (step < 1.0)
					step = 1.0;

				/* Применяем такое хитрое сравнение на случай,
					если step получился NAN или INF */
				if ( !(step < 60.0) )
					step = 60.0;
			}

			glVertex3d(ptN_pos.x, ptN_pos.y, 0);
		}
		glEnd();
	}
}

coord Painter::DrawPath(const coord &pt, double azimuth, double distance,
	double line_width, const color &line_color, double *p_rev_azimuth)
{
	glLineWidth(line_width);
	glBegin(GL_LINE_STRIP);
	glColor4dv(&line_color.r);

	coord ptN = pt;
	point ptN_pos = CoordToScreen(ptN);

	glVertex3d(ptN_pos.x, ptN_pos.y, 0);

	/* Делим путь на равные промежутки и вычисляем координаты узлов */
	double step = distance / 10.0;
	double d = 0.0;

	while (d < distance)
	{
		/* Сохраняем старое значение */
		coord ptP = ptN;
		point ptP_pos = ptN_pos;

		/* Получаем новое значение */
		while (1)
		{
			double new_d = d + step;

			if (new_d > distance)
				new_d = distance;

			ptN = Direct(pt, azimuth, new_d, p_rev_azimuth);
			ptN_pos = CoordToScreen(ptN);

			double dist_px = ptP_pos.distance(ptN_pos);
			if (dist_px < 10.0 || step < 50000.0)
			{
				d = new_d;
				break;
			}

			step /= 2.0;
		}

		/* Проверяем переход с одной стороны карты на другую */
		if (ptP.lon * ptN.lon < 0.0 && abs(ptN.lon) > 170.0)
		{
			/* Ищем среднюю точку, на которой произошёл переход */
			const double k = (180.0 - abs(ptP.lon)) / (360.0 - abs(ptN.lon - ptP.lon));
			const double mid_lat = ptP.lat + (ptN.lat - ptP.lat) * k;
			coord ptM_N(mid_lat, ptN.lon < 0.0 ? -180.0 : 180.0);
			coord ptM_P(mid_lat, ptP.lon < 0.0 ? -180.0 : 180.0);

			point ptM_N_pos = CoordToScreen(ptM_N);
			point ptM_P_pos = CoordToScreen(ptM_P);

			/* Дочерчиваем линию на предыдущей стороне */
			glVertex3d(ptM_P_pos.x, ptM_P_pos.y, 0);
			glEnd();

			/* ... и переходим на новую сторону */
			glBegin(GL_LINE_STRIP);
			glColor4dv(&line_color.r);
			glVertex3d(ptM_N_pos.x, ptM_N_pos.y, 0);
		}

		glVertex3d(ptN_pos.x, ptN_pos.y, 0);
	}
	glEnd();

	return ptN;
}

double Painter::DrawPath(const coord &pt1, const coord &pt2,
	double line_width, const color &line_color,
	double *p_azimuth, double *p_rev_azimuth)
{
	/* Находим расстояние и начальный азимут */
	double azimuth;
	double distance = Inverse(pt1, pt2, &azimuth, NULL);

	if (p_azimuth)
		*p_azimuth = azimuth;

	DrawPath(pt1, azimuth, distance, line_width, line_color, p_rev_azimuth);

	return distance;
}

void Painter::after_repaint(const size &screen_size)
{
	log << L"cartographer::after_repaint()" << log;

	/* Статус-строка */
	std::wstring status_str;

	{
		wchar_t buf[200];

		/*
		point sc_pt = screen_pos_.get_world_pos(map_pr_);
		point ce_pt = center_pos_.get_pos();

		log
			<< L"mouse_pos: " << mouse_pos_.x << L',' << mouse_pos_.y
			<< L" z_: " << z_
			<< L" screen_pos: " << sc_pt.x << L',' << sc_pt.y
			<< L" center_pos: " << ce_pt.x << L',' << ce_pt.y
			<< log;
		-*/

		coord mouse_coord = screen_to_coord(
			mouse_pos_, map_pr_, z_,
			screen_pos_.get_world_pos(map_pr_),
			center_pos_.get_pos());

		int lat_sign, lon_sign;
		int lat_d, lon_d;
		int lat_m, lon_m;
		double lat_s, lon_s;

		DDToDMS( mouse_coord,
			&lat_sign, &lat_d, &lat_m, &lat_s,
			&lon_sign, &lon_d, &lon_m, &lon_s);

		__swprintf(buf, sizeof(buf)/sizeof(*buf),
			L"z: %.1f | lat: %s%d°%02d\'%05.2f\" lon: %s%d°%02d\'%05.2f\"",
			z_,
			lat_sign < 0 ? L"-" : L"", lat_d, lat_m, lat_s,
			lon_sign < 0 ? L"-" : L"", lon_d, lon_m, lon_s);

		status_str = buf;
	}

	if (on_status_)
		on_status_(status_str);

	DrawText(system_font_id_, status_str,
		point(4.0, screen_size.height),
		color(1.0, 1.0, 1.0), ratio(0.0, 1.0));

	log << L"~ cartographer::after_repaint()" << log;
}

} /* namespace cartographer */
