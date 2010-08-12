#ifndef CARTOGRAPHER_GEODESIC_H
#define CARTOGRAPHER_GEODESIC_H

#include "defs.h"

namespace cartographer
{

/*
	Константы - параметры эллипсоида
*/

/* Эллипсоид Красовского */
//static const double c_a  = 6378245.0; /* большая полуось */
//static const double c_f = 1.0 / 298.3; /* flattening / сжатие */

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


/*
	Простые преобразования из градусов, минут
	и секунд в десятичные градусы

	Если хотя бы одно из чисел (d,m,s)
	отрицательное, то на выходе отрицательное число
*/
double DMSToDD(double d, double m, double s);

/* Преобразование сразу для обоих координат */
inline coord DMSToDD(
	double lat_d, double lat_m, double lat_s,
	double lon_d, double lon_m, double lon_s)
{
	return coord(
		DMSToDD(lat_d, lat_m, lat_s),
		DMSToDD(lon_d, lon_m, lon_s));
}

/*
	Простые преобразования из десятичных градусов
	в минут градусы, минуты и секунды
*/
void DDToDMS(double dd, int *p_d, int *p_m, double *p_s);


/*
	Решение прямой геодезической задачи (нахождение точки, отстоящей
	от заданной на определённое расстояние по начальному азимуту)
	по способу Vincenty
*/
coord Direct(const coord &pt, double azimuth, double distance,
	double *p_rev_azimuth = NULL);


/*
	Решение обратной геодезической задачи (расчёт кратчайшего расстояния
	между двумя точками, начального и обратного азимутов) по способу Бесселя

	pt1, pt2 - координаты точек
	p_azi1, p_azi2 - соответственно, начальный и обратный азимут
	eps_in_m - точность в метрах
	Возврат: расстояние в метрах
*/
double Inverse(const coord &pt1, const coord &pt2,
	double *p_azi1 = NULL, double *p_azi2 = NULL, double eps_in_m = 0.1);


/*
	Быстрый (с соответствующей точностью) рассчёт расстояния
	между точками по методу хорды

	Погрешности измерений относительно методов Бесселя и Vincenty:
	Хабаровск - Москва:       200 м
	Хабаровск - Якутск:        13 м
	Хабаровск - Магадан:        9 м
	Хабаровск - Владивосток:  1,5 м
	Хабаровск - Бикин:       0,04 м
	10 м в Хабаровске: совпадают 8 знаков после запятой
*/
double FastDistance(const coord &pt1, const coord &pt2);

} /* namespace cartographer */

#endif /* CARTOGRAPHER_GEODESIC_H */
