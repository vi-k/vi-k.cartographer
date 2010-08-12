#ifndef CARTOGRAPHER_DEFS_H
#define CARTOGRAPHER_DEFS_H

namespace cartographer
{

/*
	Точка на экране
*/
struct point
{
	double x;
	double y;

	point() : x(0), y(0) {}
	point(double x, double y) : x(x), y(y) {}
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
struct size
{
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
};

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
