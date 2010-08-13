#ifndef CARTOGRAPHER_IMAGE_H
#define CARTOGRAPHER_IMAGE_H

#include "config.h" /* Обязательно первым */
#include "defs.h"
#include "raw_image.h"

#include <my_ptr.h> /* shared_ptr */

#include <boost/function.hpp>

#include <wx/image.h> /* wxImage */
#include <GL/gl.h> /* OpenGL */

namespace cartographer
{

/*
	Изображение
*/
class image
{
public:
	typedef shared_ptr<image> ptr;
	typedef boost::function<void (image&)> on_delete_t;
	enum {unknown = -1, ready = 0};

	image(on_delete_t on_delete = on_delete_t())
		: state_(unknown)
		, width_(0)
		, height_(0)
		, scale_(1.0, 1.0)
		, texture_id_(0)
		, on_delete_(on_delete) {}

	~image()
	{
		if (on_delete_)
			on_delete_(*this);
	}

	void create(int width, int height);

	bool convert_from(const wxImage &src);
	bool load_from_file(const std::wstring &filename);
	bool load_from_mem(const void *data, std::size_t size);
	void load_from_raw(const unsigned char *data,
		int width, int height, bool with_alpha);

	GLuint load_as_gl_texture();
	GLuint convert_to_gl_texture();


	inline raw_image& raw()
		{ return raw_; }


	inline int state() const
		{ return state_; }

	inline void set_state(int state)
		{ state_ = state; }

	inline bool ok() const
		{ return state_ == ready && raw_.data() != 0; }

	inline size get_size() const
		{ return size(width_, height_); }

	inline int width() const
		{ return width_; }

	inline int height() const
		{ return height_; }

	inline ratio scale() const
		{ return scale_; }

	inline void set_scale(const ratio &scale)
		{ scale_ = scale; }

	inline GLuint texture_id() const
		{ return texture_id_; }

	inline void set_texture_id(GLuint texture_id)
		{ texture_id_ = texture_id; }

protected:
	raw_image raw_;
	int state_;
	int width_;
	int height_;
	ratio scale_;
	GLuint texture_id_;
	on_delete_t on_delete_;
};


/*
	Спрайт - изображение со смещённым центром
*/
class sprite : public image
{
public:
	typedef shared_ptr<sprite> ptr;

	sprite(on_delete_t on_delete = on_delete_t())
		: image(on_delete)
		, center_(0.5, 0.5) {}


    /* Центр изображения задаётся относительно размеров изображения */
	ratio center() const
		{ return center_; }

	void set_center(const ratio &pos)
		{ center_ = pos; }

	/* Центральная точка - центр конкретного пикселя: от 0 до sz-1.
		Координаты верхнего угла изображения в этом случае - это координаты
		верхнего левого угла верхнего левого пикселя, т.е. равны -0.5,-0.5 */
	point central_point() const
	{
		return point(width_ * center_.kx - 0.5,
			height_ * center_.ky - 0.5);
	}

	void set_central_point(const point &pos)
	{
		center_.kx = (pos.x + 0.5) / width_;
		center_.ky = (pos.y + 0.5) / height_;
	}


private:
	ratio center_;
};


/*
	Тайл
*/
class tile : public image
{
public:
	typedef shared_ptr<tile> ptr;
	enum {file_loading = 1, server_loading = 2};

	/* Идентификатор тайла */
	struct id
	{
		int map_id;
		int z;
		int x;
		int y;

		id()
			: map_id(0), z(0), x(0), y(0) {}

		id(int map_id, int z, int x, int y)
			: map_id(map_id), z(z), x(x), y(y) {}

		id(const id &other)
			: map_id(other.map_id)
			, z(other.z)
			, x(other.x)
			, y(other.y) {}

		inline bool operator!() const
		{
			return map_id == 0
				&& z == 0
				&& x == 0
				&& y == 0;
		}

		inline bool operator==(const id &other) const
		{
			return map_id == other.map_id
				&& z == other.z
				&& x == other.x
				&& y == other.y;
		}

		friend std::size_t hash_value(const id &t)
		{
			std::size_t seed = 0;
			boost::hash_combine(seed, t.map_id);
			boost::hash_combine(seed, t.z);
			boost::hash_combine(seed, t.x);
			boost::hash_combine(seed, t.y);

			return seed;
		}
	}; /* struct tile::id */


	tile(on_delete_t on_delete = on_delete_t())
		: image(on_delete) {}
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_IMAGE_H */
