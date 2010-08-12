#include "geodesic.h"

#include <math.h>

namespace cartographer
{

double DMSToDD(double d, double m, double s)
{
	double sign = d < 0.0 ? -1.0 : m < 0.0 ? -1.0 : s < 0.0 ? -1.0 : 1.0;
	return sign * (fabs(d) + fabs(m) / 60.0 + fabs(s) / 3600.0);
}

void DDToDMS(double dd, int *p_d, int *p_m, double *p_s)
{
	int d = (int)dd;
	double m_d = (dd - d) * 60.0;
	int m = (int)m_d;
	double s = (m_d - (double)m) * 60.0;

	*p_d = d;
	*p_m = m;
	*p_s = s;
}

coord Direct(const coord &pt, double azimuth, double distance,
	double *p_rev_azimuth)
{
	/*
		Решение прямой геодезической задачи по методу Vincenty
		http://www.movable-type.co.uk/scripts/latlong-vincenty-direct.html
	*/

	const double A1 = azimuth * M_PI / 180.0;
	const double sin_A1 = sin(A1);
	const double cos_A1 = cos(A1);

	const double tan_u1 = c_k * tan(pt.lat * M_PI / 180.0);
	const double cos_u1 = 1.0 / sqrt(1.0 + tan_u1 * tan_u1);
	const double sin_u1 = tan_u1 * cos_u1;

	const double sigma1 = atan2(tan_u1, cos_A1);
	const double sin_A = cos_u1 * sin_A1;
	const double cos2_A = 1.0 - sin_A * sin_A;

	const double u2 = c_eb2 * cos2_A;
	const double A = 1.0 + u2 / 16384.0 * (4096 + u2 * (-768 + u2 * (320 - 175 * u2)));
	const double B = u2 / 1024 * (256 + u2 * (-128 + u2 * (74 - 47 * u2)));

	double sigma = distance / (c_b * A);
	double sigmaP = 2.0 * M_PI;
	double sin_sigma;
	double cos_sigma;
	double cos2sigmaM;

	while (fabs(sigma - sigmaP) > 1e-12)
	{
		cos2sigmaM = cos(2.0 * sigma1 + sigma);
		sin_sigma = sin(sigma);
		cos_sigma = cos(sigma);

		const double delta_sigma = B * sin_sigma
			* (cos2sigmaM + B / 4.0 * (cos_sigma * (-1.0 + 2.0 * cos2sigmaM * cos2sigmaM)
			- B / 6 * cos2sigmaM * (-3.0 + 4.0 * sin_sigma * sin_sigma)
			* (-3.0 + 4.0 * cos2sigmaM * cos2sigmaM)));

		sigmaP = sigma;
		sigma = distance / (c_b * A) + delta_sigma;
	}

	const double tmp = sin_u1 * sin_sigma - cos_u1 * cos_sigma * cos_A1;
	const double lat2 = atan2(
		sin_u1 * cos_sigma + cos_u1 * sin_sigma * cos_A1,
		c_k * sqrt(sin_A * sin_A + tmp * tmp));
	const double lambda = atan2(sin_sigma * sin_A1,
		cos_u1 * cos_sigma - sin_u1 * sin_sigma * cos_A1);
	const double C = c_f / 16.0 * cos2_A * (4.0 + c_f * (4.0 - 3.0 * cos2_A));
	const double L = lambda - (1.0 - C) * c_f * sin_A
		* (sigma + C * sin_sigma * (cos2sigmaM + C * cos_sigma
		* (-1.0 + 2.0 * cos2sigmaM * cos2sigmaM)));

	double A2 = atan2(-sin_A, tmp);
	if (A2 < 0.0)
		A2 += 2.0 * M_PI;

	if (p_rev_azimuth)
		*p_rev_azimuth = A2 * 180.0 / M_PI;

	double lon2 = pt.lon + L * 180.0 / M_PI;
	if (lon2 > 180.0)
		lon2 -= 360.0;
	if (lon2 < -180.0)
		lon2 += 360.0;

	return coord(lat2 * 180.0 / M_PI, lon2);
}

double Inverse(const coord &pt1, const coord &pt2,
	double *p_azi1, double *p_azi2, double eps_in_m)
{
	/***
		Функция служит для решения обратной геодезической задачи:
		расчёт кратчайшего расстояния между двумя точками,
		начального и обратного азимутов.

		Решается задача по способу Бесселя согласно его описания
		в "Курсе сфероидической геодезии" В.П.Морозова сс.133-135.

			(Для желающих найти что-либо получше: ищите алгоритм по методу
			Vincenty (http://www.ga.gov.au/geodesy/datums/vincenty_inverse.jsp).
			На сегодня это наиболее применяемый метод, но я слишком поздно
			об этом узнал. Точность у обоих методов одинаковая, но, быть может,
			он окажется быстрее)

		В данном учебнике многие формулы используют константы для эллипсоида
		Красовского, принятого в СССР в 1940 г. Сегодня же более актуально
		использование эллипсоида WGS84. Это заставило автора вспомнить годы,
		проведённые в университете, и "покурить" учебник с целью приведения
		формул к универсальному виду. Что и было сделано.

		Теперь данный алгоритм не привязан к конкретному эллипсоиду (лишь бы
		он был двухосный и не повёрнутый - это, конечно, тоже ограничение,
		поэтому для расчёта траектории баллистических ракет его использовать
		не рекомендуется :)).

		Параметры для эллипсоида задаются константами:
		c_a - большая полуось
			и
		c_f - степень сжатия (flattening)

		Остальные константы вычисляются на основании этих двух.
	***/

	double distance; /* Вычисленное расстояние */
	double A1; /* Начальный азимут (в радианах) */
	double A2; /* Обратный азимут (в радианах) */

	/* Переводим точность из мм (по экватору) в радианы */
	const double eps = eps_in_m / c_a;

	/*
		B1,L1,B2,L2 - геодезические широта и долгота точек, выраженные в радианах
		c_e2 - квадрат эксцентриситета эллипса Земли (c_e - эксцентриситет эллипса)
		V,W - основные сфероидические функции (V = W / c_k)
		u - приведённая широта точки (с.11)
		A - азимут точки
		А0 - азимут геодезической линии в точке пересения с экватором
		lambda - разница долгот (с.97)
		sigma - сферическое расстояние (длина дуги большого круга,
			выраженная в частях радиуса шара) (с.97)
	*/

	/* 1. Подготовительные вычисления */
	const double sin_B1 = sin(pt1.lat * M_PI / 180.0);
	const double sin_B2 = sin(pt2.lat * M_PI / 180.0);
	const double cos_B1 = cos(pt1.lat * M_PI / 180.0);
	const double cos_B2 = cos(pt2.lat * M_PI / 180.0);

	const double l = (pt2.lon - pt1.lon) * M_PI / 180.0;

	const double W1 = sqrt(1.0 - c_e2 * sin_B1 * sin_B1);
	const double W2 = sqrt(1.0 - c_e2 * sin_B2 * sin_B2);
	const double sin_u1 = sin_B1 / W1 * c_k; /* = sin_B1 / V1 */
	const double sin_u2 = sin_B2 / W2 * c_k; /* = sin_B2 / V2 */
	const double cos_u1 = cos_B1 / W1;
	const double cos_u2 = cos_B2 / W2;
	const double a1 = sin_u1 * sin_u2;
	const double a2 = cos_u1 * cos_u2;
	const double b1 = cos_u1 * sin_u2;
	const double b2 = sin_u1 * cos_u2;

	/* 2. Последовательные приближения */
	double delta = 0.0;
	double dd = 0.0;

	int count = 0;

	while (1)
	{
		++count;

		/* Разница долгот */
		const double lambda = l + delta;
		const double sin_lambda = sin(lambda);
		const double cos_lambda = cos(lambda);

		/* Начальный азимут */
		const double p = cos_u2 * sin_lambda;
		const double q = b1 - b2 * cos_lambda;
		A1 = atan2(p, q);
		if (A1 < 0.0)
			A1 += 2.0 * M_PI;

		/* Сферическое расстояние */
		const double sin_sigma = p * sin(A1) + q * cos(A1);
		const double cos_sigma = a1 + a2 * cos_lambda;
		double sigma = atan2(sin_sigma, cos_sigma);

		/* Азимут линии на экваторе */
		const double sin_A0 = cos_u1 * sin(A1);
		const double cos2_A0 = 1.0 - sin_A0 * sin_A0;

		const double x = 2.0 * a1 - cos2_A0 * cos_sigma;

		/*
			alpha = (1/2 * e^2 + 1/8 * e^4 + 1/16 * e^6 + ...)
				- (1/16 * e^4 + 1/16 * e^6 + ...) * cos^2(A0)
				+ (3/128 * e^6 + ...) * cos^4(A0)
				- ...

			Для эллипсоида Красовского (из учебника):
			const double alpha =
				(33523299 - (28189 - 70 * cos2_A0) * cos2_A0) * 0.0000000001;
		*/
		const double alpha =
			(0.5 * c_e2 + 0.125 * c_e4 + 0.0625 * c_e6)
			- (0.0625 * (c_e4 + c_e6) - 0.0234375 * c_e6 * cos2_A0) * cos2_A0;

		/*
			beta' = 2beta / Cos^2(A0)
			beta = (1/32 * e^4 + 1/32 * e^6 + ...) * cos^2(A0)
				- (1/64 * e^6 + ...) * cos^4(A0)
				+ ...
			=> beta' = (1/16 * e^4 + 1/16 * e^6 + ...)
				- (1/32 * e^6 + ...) * cos^2(A0)
				+ ...

			Для эллипсоида Красовского (из учебника):
			const double beta_ = (28189 - 94 * cos2_A0) * 0.0000000001;
		*/
		const double beta_ =
			0.0625 * (c_e4 + c_e6) - (0.03125 * c_e6) * cos2_A0;

		const double prev_dd = dd;
		dd = delta;
		delta = (alpha * sigma - beta_ * x * sin_sigma) * sin_A0;
		dd = fabs(delta - dd);

		if ( (count > 1 && dd >= prev_dd) /*|| count >= 1000*/)
		{
			/*
				При попадании в точку-антипод или в область вокруг неё
				delta начинает метаться с одного значения на другое,
				в результате чего функция входит в бесконечный цикл.
				Также избегаем затяжных циклов.

				В теории (по учебнику) точки-антиподы легко обнаружить
				на ранней стадии: lambda = PI, u1 = -u2. А потом,
				соответственно, их решить математическим способом.
				В реальности дело обстоит сложнее. Существует большая
				область вокруг точки-антипода, для которой данный метод
				не подходит. По долготе эта область начинается на расстоянии
				~179.3965 градусов от первой точки. Если вторая точка лежит
				до этой точки - кратчайший путь будет пролегать вдоль экватора,
				если после неё - начинается плавное смещение к пути через полюс.
				К самим точкам-антиподам кратчайшее расстояние есть всегда
				константа и проходит через любой из полюсов.

				179.3965 - это 67 км до точки-антипода. И каким-либо образом
				пренебречь рассчётами для метода с погрешностью в сантиметрах
				не простительно.

				Нашёл простое, но медленное решение: разделить путь на два
				отрезка. Измерить каждый, вычислить сумму. Вопрос лишь в том,
				в какой точке разделить отрезок?

				Методом приближений: нахожу расстояния через полюс и через
				экватор. Сравниваю, точку, в которой расстояние получилось
				большим, приближаю к другой. Так продолжаю до требуемой
				точности.

				Приближаю на половину разницы широт - не оптимально, но работает!
			*/

			const double lonM = (pt1.lon + pt2.lon) / 2.0;

			coord ptM1(0.0, lonM);
			coord ptM2(90.0, lonM);
			double aziM1_1, aziM1_2;
			double aziM2_1, aziM2_2;

			double s1 = Inverse(pt1, ptM1, &aziM1_1, NULL, eps_in_m)
				+ Inverse(ptM1, pt2, NULL, &aziM1_2, eps_in_m);
			double s2 = Inverse(pt1, ptM2, &aziM2_1, NULL, eps_in_m)
				+ Inverse(ptM2, pt2, NULL, &aziM2_2, eps_in_m);

			while ( fabs(s2 - s1) > eps_in_m )
			{
				if (s1 > s2)
				{
					ptM1.lat = (ptM1.lat + ptM2.lat) / 2.0;
					s1 = Inverse(pt1, ptM1, &aziM1_1, NULL, eps_in_m)
						+ Inverse(ptM1, pt2, NULL, &aziM1_2, eps_in_m);
				}
				else
				{
					ptM2.lat = (ptM1.lat + ptM2.lat) / 2.0;
					s2 = Inverse(pt1, ptM2, &aziM2_1, NULL, eps_in_m)
						+ Inverse(ptM2, pt2, NULL, &aziM2_2, eps_in_m);
				}
			}

			if (s1 < s2)
			{
				if (p_azi1) *p_azi1 = aziM1_1;
				if (p_azi2) *p_azi2 = aziM1_2;
				distance = s1;
			}
			else
			{
				if (p_azi1) *p_azi1 = aziM2_1;
				if (p_azi2) *p_azi2 = aziM2_2;
				distance = s2;
			}

			return distance;
		}

		/* Достигли требуемой точности */
		if (dd < eps)
		{
			/***
				Расстояние рассчитываем по формуле:
				distance = A * sigma + (B_ * x + C_ * y) * sin_sigma;

				sigma, x - уже рассчитали
				A, B_, C_, y - рассчитаем ниже
			***/

			/*
				A = b * (1 + 1/4 * k^2 - 3/64 * k^4 + 5/256 * k^6 - ...)
					= b * (1 + k^2 * (1/4 - 3/64 * k^2 + 5/256 * k^4 - ...))

				где k^2 = e'^2 * Cos^2(A0)
					(const double k2 = c_eb2 * cos2_A0;)

				Примеры расчётов (выносим константы вперёд):

				Если переменные c_xxx не определены на стадии компиляции:
				const double A_HI = c_b * (1.0 + k2 * (0.25 + k2 * (-0.046875 + 0.01953125 * k2)));
				const double A_LO = c_b * (1.0 + k2 * (0.25 - 0.046875 * k2));

				Если c_xxx - статические константы:
				const double A_HI = c_b + (0.25 * c_b_eb2 + (-0.046875 * c_b_eb4 + 0.01953125 * c_b_eb6 * cos2_A0) * cos2_A0) * cos2_A0;
				const double A_LO = c_b + (0.25 * c_b_eb2 - 0.046875 * c_b_eb4 * cos2_A0) * cos2_A0;

				Для эллипсоида Красовского (из учебника):
				const double A = 6356863.020 + (10708.949 - 13.474 * cos2_A0) * cos2_A0;
			*/
			const double A = c_b + (0.25 * c_b_eb2 + (-0.046875 * c_b_eb4 + 0.01953125 * c_b_eb6 * cos2_A0) * cos2_A0) * cos2_A0;

			/*
				B' = 2B / Cos^2(A0)
				B = b * (1/8 * k^2 - 1/32 * k^4 + 15/1024 * k^6 - ...)
					= b * k^2 * (1/8 - 1/32 * k^2 + 15/1024 * k^4 - ...)
				=> B' = b * e'^2 * (1/4 - 1/16 * k^2 + 15/512 * k^4 - ...)

				Примеры расчётов:

				Если переменные c_xxx не определены на стадии компиляции:
				const double B_HI = c_b * c_eb2 * (1.0 / 4.0 - k2 / 16.0 + 15.0 / 512.0 * k2 * k2);
				const double B_LO = c_b * c_eb2 * (1.0 / 4.0 - k2 / 16.0);

				Если c_xxx - статические константы:
				const double B_HI = 0.25 * c_b_eb2 + (-0.0625 * c_b_eb4 + 0.029296875 * c_b_eb6 * cos2_A0) * cos2_A0;
				const double B_LO = 0.25 * c_b_eb2 - 0.0625 * c_b_eb4 * cos2_A0;

				Для эллипсоида Красовского (из учебника):
				const double B_ = 10708.938 - 17.956 * cos2_A0;
			*/
			const double B_ = 0.25 * c_b_eb2 + (-0.0625 * c_b_eb4 + 0.029296875 * c_b_eb6 * cos2_A0) * cos2_A0;

			/*
				C' = 2C / Cos^4(A0)
				C = b * (1/128 * k^4 - 3/512 * k^6 + ...)
					= b * k^4 * (1/128 - 3/512 * k^2 + ...)
				=> C' = b * e'^4 * (1/64 - 3/256 * k^2 + ...)

				Примеры расчётов:

				Если переменные c_xxx не определены на стадии компиляции:
				const double C_HI = c_b_eb4 * (0.015625 - 0.01171875 * k2);
				const double C_LO = 0.015625 * c_b_eb4;

				Если c_xxx - статические константы:
				const double C_HI = 0.015625 * c_b_eb4 - 0.01171875 * c_b_eb6 * cos2_A0;
				const double C_LO = 0.015625 * c_b_eb4;

				Для эллипсоида Красовского (из учебника):
				const double C_ = 4.487;
			*/
			const double C_ = 0.015625 * c_b_eb4 - 0.01171875 * c_b_eb6 * cos2_A0;

			const double y = (cos2_A0 * cos2_A0 - 2 * x * x) * cos_sigma;
			distance = A * sigma + (B_ * x + C_ * y) * sin_sigma;

			/* Обратный азимут.
				По учебнику обратный азимут - это азимут, с которым мы приходим
				в конечную точку. Мне показалось более правильным считать такой
				азимут азимутом, с которым мы будем возвращаться назад - это дело
				лишь смены знака в выражениях p2 и q2 */
			const double p2 = - cos_u1 * sin_lambda;
			const double q2 = b2 - b1 * cos_lambda;

			A2 = atan2(p2, q2);
			if (A2 < 0.0)
				A2 += 2.0 * M_PI;


			break;
		}
	} /* while (1) */

	if (p_azi1)
		*p_azi1 = A1 * 180.0 / M_PI;

	if (p_azi2)
		*p_azi2 = A2 * 180.0 / M_PI;

	return distance;
}

double FastDistance(const coord &pt1, const coord &pt2)
{
	/***
		Быстрый рассчёт расстояния между точками по методу хорды
	***/

	/*
		r1,r2 - радиус кривизны первого вертикала на данной широте
	*/

	/* Координаты первой точки */
	const double sin_B1 = sin(pt1.lat * M_PI / 180);
	const double cos_B1 = cos(pt1.lat * M_PI / 180);
	const double sin_L1 = sin(pt1.lon * M_PI / 180);
	const double cos_L1 = cos(pt1.lon * M_PI / 180);

	const double r1 = c_a / sqrt(1.0 - c_e2 * sin_B1 * sin_B1);

	const double x1 = r1 * cos_B1 * cos_L1;
	const double y1 = r1 * cos_B1 * sin_L1;
	const double z1 = (1.0 - c_e2) * r1 * sin_B1;

	/* Координаты второй точки */
	const double sin_B2 = sin(pt2.lat * M_PI / 180);
	const double cos_B2 = cos(pt2.lat * M_PI / 180);
	const double sin_L2 = sin(pt2.lon * M_PI / 180);
	const double cos_L2 = cos(pt2.lon * M_PI / 180);

	const double r2 = c_a / sqrt(1.0 - c_e2 * sin_B2 * sin_B2);

	const double x2 = r2 * cos_B2 * cos_L2;
	const double y2 = r2 * cos_B2 * sin_L2;
	const double z2 = (1.0 - c_e2) * r2 * sin_B2;

	/* Расстояние между точками (размер хорды) */
	const double d = sqrt( (x2 - x1) * (x2 - x1)
		+ (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1) );

	/* Длина дуги по хорде. Главная проблема - выбрать радиус.
		Проблема не решена! Но для местных условий (по краю),
		оптимальным оказалось выбрать наименьший радиус
		кривизны из двух рассчитанных */
	const double r = r1 < r2 ? r1 : r2;
	return 2.0 * r * asin(0.5 * d / r);
}

} /* namespace cartographer */
