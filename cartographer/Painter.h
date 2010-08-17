#ifndef CARTOGRAPHER_PAINTER_H
#define CARTOGRAPHER_PAINTER_H

#include "Base.h"

namespace cartographer
{

/*
	Картографер
*/
class Painter : public Base
{
public:
	/* Обработчик статус-строки */
	typedef boost::function<void (std::wstring &str)> on_status_proc_t;

	/* Конструктор
		Параметры:
			parent - родительское окно
			server_addr - адрес сервера в виде L"name:port".
				При умолчанию (пустая строка или не указан порт) используется
				адрес 127.0.0.1 и порт 27543. Если вместо адреса указать L"cache"
				- будет использован только уже имеющийся кэш.
			init_map - Исходная карта:
				L"Яндекс.Карта", L"Яндекс.Спутник", L"Google.Спутник".
				Если строка пустая - устанавливается первая из имеющихся.
			cache_size - Количество тайлов, хранимых в кэше:
				для экрана 1280 на 1024 минимальное кол-во - ~300
	*/
	Painter(wxWindow *parent,
		const std::wstring &server_addr = std::wstring(),
		const std::wstring &init_map = std::wstring(),
		std::size_t cache_size = 500);

	~Painter();

	void SetPainter(on_paint_proc_t on_paint_proc);
	void Update();
	void Stop();

	void SetStatusHandler(on_status_proc_t on_status_proc);


	/*
		Карты
	*/
	int GetMapsCount();
	map_info GetMapInfo(int index);
	map_info GetActiveMapInfo();
	bool SetActiveMapByIndex(int index);
	bool SetActiveMapByName(const std::wstring &map_name);


	/*
		Преобразование координат: географические в экранные и обратно
	*/
	point CoordToScreen(const coord &pt);
	point CoordToScreen(fast_point &pt);
	coord ScreenToCoord(const point &pt);


	/*
		Масштаб
	*/
	double GetActiveZ();
	void SetActiveZ(int z);
	void ZoomIn();
	void ZoomOut();


	/*
		Текущая позиция
	*/
	fast_point GetScreenPos();
	void MoveTo(const coord &pt);
	void MoveTo(int z, const coord &pt);


	/*
		Работа с изображениями
	*/
	int LoadImageFromFile(const std::wstring &filename);
	int LoadImageFromMem(const void *data, std::size_t size);
	int LoadImageFromRaw(const unsigned char *data, int width, int height, bool with_alpha);

	template<class C_ImageStruct>
	int LoadImageFromC(const C_ImageStruct &st)
	{
		return LoadImageFromRaw( st.pixel_data,
			st.width, st.height, st.bytes_per_pixel == 4);
	}

	void DeleteImage(int image_id);

	/* Центр изображения - задаётся относительно размеров изображения от 0.0 до 1.0 */
	ratio GetImageCenter(int image_id);
	void SetImageCenter(int image_id, const ratio &pos);
	inline void SetImageCenter(int image_id, double kx, double ky)
		{ SetImageCenter(image_id, ratio(kx, ky)); }

	/* Центральная точка изображения - для удобства
		установки центра изображения на конкретную точку */
	point GetImageCentralPoint(int image_id);
	void SetImageCentralPoint(int image_id, const point &pos);
	inline void SetImageCentralPoint(int image_id, double x, double y)
		{ SetImageCentralPoint(image_id, point(x, y)); }

	/* Размеры изображения */
	size GetImageSize(int image_id);
	ratio GetImageScale(int image_id);
	void SetImageScale(int image_id, const ratio &scale);
	inline void SetImageScale(int image_id, double kx, double ky)
		{ SetImageScale(image_id, ratio(kx, ky)); }
	inline void SetImageScale(int image_id, double k)
		{ SetImageScale(image_id, ratio(k, k)); }

	/* Вывод изображения */
	void DrawImage(int image_id, const point &pos, const ratio &scale = ratio(1.0, 1.0));
	inline void DrawImage(int image_id, const point &pos, double kx, double ky)
		{ return DrawImage(image_id, pos, ratio(kx, ky)); }
	inline void DrawImage(int image_id, const point &pos, double k)
		{ return DrawImage(image_id, pos, ratio(k, k)); }

	/*
		Работа с текстом
	*/
	int CreateFont(const wxFont &wxfont);
	void DeleteFont(int font_id);
	size DrawText(int font_id, const std::wstring &str, const point &pos,
		const color &text_color,
		const ratio &center = ratio(0.5, 0.5),
		const ratio &scale = ratio());

protected:
	int sprites_index_;
	sprites_list sprites_;
	shared_mutex sprites_mutex_;

	int fonts_index_;
	fonts_list fonts_;
	shared_mutex fonts_mutex_;
	int system_font_id_;

	on_status_proc_t on_status_;

	virtual void after_repaint(const size &screen_size);
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_PAINTER_H */
