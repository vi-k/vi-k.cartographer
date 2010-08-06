﻿#ifndef CARTOGRAPHER_DEFS_H
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
	double width;
	double height;

	size() : width(0), height(0) {}
	size(double w, double h) : width(w), height(h) {}
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_DEFS_H */