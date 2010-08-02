#ifndef CART_IMAGE_H
#define CART_IMAGE_H

#include "config.h" /* Обязательно первым */
#include "defs.h"
#include "raw_image.h"

#include <boost/function.hpp>

#include <wx/image.h> /* wxImage */
#include <gl/gl.h> /* OpenGL */

namespace cart
{

/*
	Изображение
*/
class image
{
public:
	typedef boost::function<void (image&)> on_delete_t;

	image(on_delete_t on_delete = on_delete_t())
		: on_delete_(on_delete)
		, texture_id_(0)
		, scale_(1.0, 1.0) {}

	~image()
	{
		if (on_delete_)
			on_delete_(*this);
	}

	bool convert_from(const wxImage &src);
	bool load_from_file(const std::wstring &filename);
	bool load_from_mem(const void *data, std::size_t size);
	void load_from_raw(const unsigned char *data,
		int width, int height, bool with_alpha);

	inline raw_image& raw()
		{ return raw_; }

	inline size get_size() const
		{ return size(width_, height_); }

	inline int width() const
		{ return width_; }
		
	inline int height() const
		{ return height_; }
		
	inline GLuint texture_id() const
		{ return texture_id_; }

	inline void set_texture_id(GLuint texture_id)
		{ texture_id_ = texture_id; }

	inline size scale() const
		{ return scale_; }
		
	inline void set_scale(const size &scale)
		{ scale_ = scale; }

	inline bool ok() const
		{ return raw_.data() != 0; }

protected:
	on_delete_t on_delete_;
	raw_image raw_;
	int width_;
	int height_;
	GLuint texture_id_;
	size scale_;
};


/*
	Спрайт - изображение со смещённым центром
*/
class sprite : public image
{
public:
	sprite(on_delete_t on_delete = on_delete_t())
		: image(on_delete)
		, offset_(0.0, 0.0) {}

	size offset() const
		{ return offset_; }

	void set_offset(double dx, double dy)
		{ offset_.width = dx, offset_.height = dy; }

	point central_point() const
		{ return point(-offset_.width - 0.5, -offset_.height - 0.5); }

	void set_central_point(double x, double y)
		{ offset_.width = -x - 0.5, offset_.height = -y - 0.5; }
		

private:
	size offset_;
};


/*
	Тайл
*/
class tile : public image
{
public:
	typedef shared_ptr<tile> ptr;
	enum step_t {unknown, file_loading, server_loading, ready};

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


	tile(on_delete_t on_delete = on_delete_t(), step_t step = unknown)
		: image(on_delete)
		, step_(step)
	{
	}

	inline void set_step(step_t step)
		{ step_ = step; }

	inline step_t step()
		{ return step_; }

private:
	step_t step_;

}; /* class tile */

} /* namespace cart */

#endif /* CART_IMAGE_H */
