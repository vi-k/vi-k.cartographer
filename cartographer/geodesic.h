#ifndef CARTOGRAPHER_GEODESIC_H
#define CARTOGRAPHER_GEODESIC_H

#include "defs.h"

namespace cartographer
{

enum projection
{
	Unknown_Projection
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
void DDToDMS(double dd, int *p_sign, int *p_d, int *p_m, double *p_s);
inline void DDToDMS(const coord &pt,
	int *p_lat_sign, int *p_lat_d, int *p_lat_m, double *p_lat_s,
	int *p_lon_sign, int *p_lon_d, int *p_lon_m, double *p_lon_s)
{
	DDToDMS(pt.lat, p_lat_sign, p_lat_d, p_lat_m, p_lat_s),
	DDToDMS(pt.lon, p_lon_sign, p_lon_d, p_lon_m, p_lon_s);
}


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

inline point coord_to_tiles(const coord &pt, projection pr, double z)
	{ return world_to_tiles( coord_to_world(pt, pr), z ); }

inline coord tiles_to_coord(const point &pos, projection pr, double z)
	{ return world_to_coord( tiles_to_world(pos, z), pr); }


/*
	Экранные координаты
	screen_pos - расположение экрана "в мире" (в мировых координатах)
	center_pos - позиция центра экрана (в экранных координатах)
*/
inline point world_to_screen(const point &pos, double z,
	const point &screen_pos, const point &center_pos)
{
	return (pos - screen_pos.as_size()) * world_px_size(z)
		+ center_pos.as_size();
}

inline point screen_to_world(const point &pos, double z,
	const point &screen_pos, const point &center_pos)
{
	return (pos - center_pos.as_size()) / world_px_size(z)
		+ screen_pos.as_size();
}

inline point coord_to_screen(const coord &pt, projection pr, double z,
	const point &screen_pos, const point &center_pos)
{
	return world_to_screen( coord_to_world(pt, pr), z,
		screen_pos, center_pos );
}

inline coord screen_to_coord(const point &pos, projection pr, double z,
	const point &screen_pos, const point &center_pos)
{
	return world_to_coord(
		screen_to_world(pos, z, screen_pos, center_pos), pr);
}


/*
	"Быстрая" точка
*/
class fast_point
{
private:
	coord pt_;
	projection pr_;
	point world_pos_;

public:

	fast_point()
		: pt_()
		, pr_(Unknown_Projection)
		, world_pos_() {}

	fast_point(const coord &pt)
		: pt_(pt)
		, pr_(Unknown_Projection)
		, world_pos_() {}

	/* Установка новых координат */
	//inline void set_coord(const coord &pt)
	//{
	//	pt_ = pt;
	//	pr_ = Unknown_Projection;
	//}

	inline coord get_coord() const
		{ return pt_; }

	/* Мировые координаты точки */
	inline point get_world_pos(projection pr)
	{
		if (pr != pr_)
		{
			world_pos_ = coord_to_world(pt_, pr);
			pr_ = pr;
		}

		return world_pos_;
	}

	/* Тайловые координаты точки */
	inline point get_tiles_pos(projection pr, double z)
		{ return world_to_tiles( get_world_pos(pr), z ); }

	/* Экранные координаты точки */
	inline point get_screen_pos(projection pr, double z,
		const point &screen_pos, const point &center_pos)
	{
		return world_to_screen(
			get_world_pos(pr), z, screen_pos, center_pos );
	}
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_GEODESIC_H */
