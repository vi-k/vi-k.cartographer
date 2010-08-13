#ifndef CARTOGRAPHER_DEFS_H
#define CARTOGRAPHER_DEFS_H

namespace cartographer
{

/*
	Соотношение - для масштабов и центральных точек
*/
struct ratio
{
	double kx;
	double ky;

	ratio() : kx(1.0), ky(1.0) {}
	ratio(double kx, double ky) : kx(kx), ky(ky) {}
};

/*
	Географические координаты
*/
struct coord
{
	double lat;
	double lon;

	coord() : lat(0), lon(0) {}
	coord(double lat, double lon) : lat(lat), lon(lon) {}
};

/*
	Размер
*/
class size
{
public:
	union
	{
		double width;
		double dx;
	};
	union
	{
		double height;
		double dy;
	};

	size() : width(0), height(0) {}
	size(double w, double h) : width(w), height(h) {}

	inline size operator *(double k) const
		{ return size(width * k, height * k); }

	inline size operator *(const ratio &scale) const
		{ return size(width * scale.kx, height * scale.ky); }

	inline size operator /(double k) const
		{ return size(width / k, height / k); }
};

/*
	Точка на проекции (мировые, тайловые, экранные координаты)
*/
class point
{
public:
	double x;
	double y;

	point() : x(0), y(0) {}
	point(double x, double y) : x(x), y(y) {}
	point(const size &sz) : x(sz.dx), y(sz.dy) {}

	inline size as_size() const
		{ return size(x, y); }

	/* Унарный минус */
	inline point operator -() const
		{ return point(-x, -y); }

	inline point operator *(double k) const
		{ return point(x * k, y * k); }

	inline point operator *(const ratio &scale) const
		{ return point(x * scale.kx, y * scale.ky); }

	inline point operator /(double k) const
		{ return point(x / k, y / k); }

	inline ratio operator /(const size &size) const
		{ return ratio(x / size.width, y / size.height); }

	inline point operator -(const size &offset) const
		{ return point(x - offset.dx, y - offset.dy); }

	inline size operator -(const point &other) const
		{ return size(x - other.x, y - other.y); }

	inline point operator +(const size &offset) const
		{ return point(x + offset.dx, y + offset.dy); }
};

class rel_point
{
private:
	ratio rel_pos_;
	size size_;

public:
	rel_point() {}
	rel_point(const ratio &rel_pos) : rel_pos_(rel_pos) {}
	rel_point(const ratio &rel_pos, const size &size)
		: rel_pos_(rel_pos), size_(size) {}

	inline ratio get_rel_pos() const
		{ return rel_pos_; }

	inline void set_rel_pos(const ratio &rel_pos)
		{ rel_pos_ = rel_pos; }

	inline size get_size() const
		{ return size_; }

	inline void set_size(const size &sz)
		{ size_ = sz; }

	inline point get_pos() const
		{ return point( size_ * rel_pos_ ); }

	inline point get_pos_for(const size &sz) const
		{ return point( sz * rel_pos_ ); }

	inline void set_pos(const point &pos)
		{ rel_pos_ = pos / size_; }
};

/*
	Цвет
*/
struct color
{
	double r;
	double g;
	double b;
	double a;

	color()
		: r(0), g(0), b(0), a(0) {}
	color(double r, double g, double b)
		: r(r), g(g), b(b), a(1.0) {}
	color(double r, double g, double b, double a)
		: r(r), g(g), b(b), a(a) {}
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_DEFS_H */
