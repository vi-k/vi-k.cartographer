#ifndef CARTOGRAPHER_GEODESIC_H
#define CARTOGRAPHER_GEODESIC_H

#include "defs.h"

namespace cartographer
{

enum projection
{
	Unknown
	, Sphere_Mercator  /* Google Maps */
	, WGS84_Mercator   /* Yandex, Kosmosnimki */
	//, WGS84_PlateCaree /* Google Earth */
};


/*
	Константы - параметры эллипсоида
*/

/* Эллипсоид Красовского */
//static const double c_a  = 6378245.0; /* большая полуось */
//static const double c_f = 1.0 / 298.3; /* сжатие (flattening) */

/* Эллипсоид WGS84 */
static const double c_a  = 6378137.0; /* большая полуось */
static const double c_f = 1.0 / 298.257223563; /* сжатие (flattening) */

static const double c_b  = c_a * (1.0 - c_f); /* малая полуось */
static const double c_e = sqrt(c_a * c_a - c_b * c_b) / c_a; /* эксцентриситет эллипса (eccentricity) */
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
	Хабаровск - Москва:       +200 м
	Хабаровск - Якутск:        -13 м
	Хабаровск - Магадан:        -9 м
	Хабаровск - Владивосток:  -1,5 м
	Хабаровск - Бикин:       -0,04 м
	10 м в Хабаровске: совпадают 8 знаков после запятой
*/
double FastDistance(const coord &pt1, const coord &pt2);


/*
	Перевод географических координат в координаты проекции и обратно

	Поддерживаемые типы картографических проекций:
		Sphere_Mercator - проекция сферы на нормальный Меркатор (Google Maps)
		WGS84_Mercator  - проекция эллипсоида WGS84 на Меркатор (Yandex, Kosmosnimki)

	Проекции используют обратную Декартову систему координат:
		- начало координат (0,0) в левом верхнем углу;
		- ось y направлена вниз;
		- ось x - вправо.

	Координаты проекций могут быть представлены в различных единицах
	измерения:

		- world (единица измерения - мир; мировые координаты) - относительные
			величины, максимальное значение которых (в правом нижнем углу)
			равно 1.0,1.0. Удобны тем, что зависят только от типа проекции
			- т.е. пересчёт координат (достаточно трудоёмкая операция)
			необходимо будет делать только при переходе между картами,
			имеющими разные проекции.

		- tiles (единицы измерения - тайлы; тайловые координаты) - зависят
			от проекции и масштаба карты. Максимальное значение - количество
			тайлов в данном масштабе по одной из осей - 1). Количество тайлов
			рассчитывается по формуле: 2 ^ (z - 1) или 1 << (z - 1).
			Применяется для внутренних процессов Картографера.
		
		- pixels (единицы измерения - экранные точки (пиксели);
			пиксельные координаты) - тайловые координаты * 256 (где 256
			- размер одного тайла по любой из осей). Соответственно,
			также зависят от типа проекции и масшатаба карты.

		- screen (экранные координаты) - пиксельные координаты, используемы
			для отображения объектов на экране. Т.к. карта находится
			в постоянном движении, то делать каждый раз пересчёт
			из географических координат в экранные - излишняя трата
			ресурсов процессора. Оптимальным вариантом является одноразовый
			пересчёт (при переходе между картами с различными проекциями,
			к примеру - между Google Maps и Yandex) географических координат
			в мировые и последующий их пересчёт при каждой перерисовке карты
			в экранные - это существенно разгрузит процессор.
*/

/* Количество тайлов для заданного масштаба */
inline int tiles_count(int z)
{
	return 1 << (z - 1);
}

/* Размер мира в тайловых координатах для переходного z */
inline double world_tl_size(double z)
{
	int z_i = (int)z;
	return (double)tiles_count(z_i) * (1.0 + z - z_i);
}

inline double world_px_size(double z)
	{ return world_tl_size(z) * 256.0; }


/* Мировые координаты */
point coord_to_world(const coord &pt, projection pr);
coord world_to_coord(const point &pos, projection pr);


/* Тайловые координаты */
inline point world_to_tiles(const point &pos, double z)
	{ return pos * world_tl_size(z); }

inline point tiles_to_world(const point &pos, double z)
	{ return pos / world_tl_size(z); }

inline point coord_to_tiles(const coord &pt, double z, projection pr)
	{ return world_to_tiles( coord_to_world(pt, pr) ); }

inline coord tiles_to_coord(const point &pos, double z, projection pr)
	{ return world_to_coord( tiles_to_world(pos, z), pr); }


/* Экранные координаты.
	Смещение экрана в тех же единицах, что и заданная позиция! */
point world_to_screen(const point &pos, double z,
	const size &screen_offset, const size &center_offset)
{
	return (pos - screen_offset) * world_px_size(z) + center_offset;
}

point screen_to_world(const point &pos, double z,
	const size &screen_offset, const size &center_offset)
{
	return (pos - center_offset) / world_px_size(z) + screen_offset;
}

#if 0
double lon_to_tile_x(double lon, double z);
inline double lat_to_tile_y(double lat, double z,
		map_info::projection_t projection);

	/* Градусы -> экранные координаты */
	static inline double lon_to_scr_x(double lon, double z,
		double fix_lon, double fix_scr_x);
	static inline double lat_to_scr_y(double lat, double z,
		map_info::projection_t projection, double fix_lat, double fix_scr_y);

	/* Тайловые координаты -> градусы */
	static inline double tile_x_to_lon(double x, double z);
	static inline double tile_y_to_lat(double y, double z,
		map_info::projection_t projection);

	/* Экранные координаты -> градусы */
	static inline double scr_x_to_lon(double x, double z,
		double fix_lon, double fix_scr_x);
	static inline double scr_y_to_lat(double y, double z,
		map_info::projection_t projection, double fix_lat, double fix_scr_y);
#endif
} /* namespace cartographer */

#endif /* CARTOGRAPHER_GEODESIC_H */
