#ifndef CART_IMAGE_H
#define CART_IMAGE_H

#include "config.h" /* Обязательно первым */
#include "defs.h"
#include "raw_image.h"

#include <my_thread.h> /* shared_mutex */
#include <my_ptr.h> /* shared_ptr */

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
	enum {unknown = -1, ready = 0};

	image(on_delete_t on_delete = on_delete_t())
		: state_(unknown)
		, width_(0.0)
		, height_(0.0)
		, scale_(1.0, 1.0)
		, texture_id_(0)
		, on_delete_(on_delete) {}

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

	
	inline recursive_mutex& get_mutex()
		{ return mutex_; }

	inline raw_image& raw()
		{ return raw_; }

	
	inline int state() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return state_;
	}
		
	inline void set_state(int state)
	{
		unique_lock<recursive_mutex> lock(mutex_);
		state_ = state;
	}

	inline bool ok() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return state_ == ready && raw_.data() != 0;
	}

	inline size get_size() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return size(width_, height_);
	}

	inline int width() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return width_;
	}
		
	inline int height() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return height_;
	}
		
	inline size scale() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return scale_;
	}
		
	inline void set_scale(const size &scale)
	{
		unique_lock<recursive_mutex> lock(mutex_);
		scale_ = scale;
	}

	inline GLuint texture_id() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return texture_id_;
	}

	inline void set_texture_id(GLuint texture_id)
	{
		unique_lock<recursive_mutex> lock(mutex_);
		texture_id_ = texture_id;
	}

protected:
	mutable recursive_mutex mutex_;
	raw_image raw_;
	int state_;
	int width_;
	int height_;
	size scale_;
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
		, offset_(0.0, 0.0) {}

	
	size offset() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return offset_;
	}

	void set_offset(double dx, double dy)
	{
		unique_lock<recursive_mutex> lock(mutex_);
		offset_.width = dx, offset_.height = dy;
	}

	point central_point() const
	{
		unique_lock<recursive_mutex> lock(mutex_);
		return point(-offset_.width - 0.5, -offset_.height - 0.5);
	}

	void set_central_point(double x, double y)
	{
		unique_lock<recursive_mutex> lock(mutex_);
		offset_.width = -x - 0.5, offset_.height = -y - 0.5;
	}
		

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

} /* namespace cart */

#endif /* CART_IMAGE_H */
