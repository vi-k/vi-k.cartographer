#ifndef CARTOGRAPHER_H
#define CARTOGRAPHER_H

#include "config.h" /* Обязательно первым */
#include "defs.h" /* point, coord, size */
#include "image.h" /* image, sprite, tile */
#include "font.h"
#include "geodesic.h"

#include <mylib.h>

#include <cstddef> /* std::size_t */
#include <string>
#include <map>
#include <list>
#include <vector>

#include <boost/unordered_map.hpp>
#include <boost/function.hpp>

#include <wx/dcgraph.h> /* wxGCDC и wxGraphicsContext */
#include <wx/mstream.h>  /* wxMemoryInputStream */
#include <wx/glcanvas.h> /* OpenGL */

extern my::log main_log;

namespace cartographer
{

/*
	Описание карты
*/
struct map_info
{
	std::wstring sid;
	std::wstring name;
	bool is_layer;
	std::wstring tile_type;
	std::wstring ext;
	projection pr;

	map_info()
		: is_layer(false)
		, pr(Unknown_Projection) {}
};


/*
	Картографер
*/
class Frame : public wxGLCanvas, my::employer
{
public:
	/* Тип обработчика */
	typedef boost::function<void (double z, int width, int height)> on_paint_proc_t;

	/* Конструктор */
	Frame(wxWindow *parent,
		const std::wstring &server_addr, const std::wstring &server_port,
		std::size_t cache_size, bool only_cache,
		const std::wstring &init_map, on_paint_proc_t on_paint_proc,
		int anim_period = 0, int def_min_anim_steps = 0);

	~Frame();

	void Stop();
	void Update();


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
	void MoveTo(int z, const coord &pt);


	/*
		Работа с изображениями
	*/
	int LoadImageFromFile(const std::wstring &filename);
	int LoadImageFromMem(const void *data, std::size_t size);
	int LoadImageFromRaw(const unsigned char *data, int width, int height, bool with_alpha);
	void DeleteImage(int image_id);

	/* Смещение изображения при выводе. По умолчанию: 0.0, 0.0 */
	size GetImageOffset(int image_id);
	void SetImageOffset(int image_id, double dx, double dy);

	/* Центральная точка изображения. На деле устанавливает смещение
		изображения равным -(x + 0.5), -(y + 0.5) */
	point GetImageCentralPoint(int image_id);
	void SetImageCentralPoint(int image_id, double x, double y);

	/* Размеры изображения */
	size GetImageSize(int image_id);
	size GetImageScale(int image_id);
	void SetImageScale(int image_id, const size &scale);
	inline void SetImageScale(int image_id, double scale_w, double scale_h)
		{ SetImageScale( image_id, size(scale_w, scale_h) ); }

	/* Вывод изображения */
	void DrawImage(int image_id, double x, double y, double scale_x, double scale_y);
	inline void DrawImage(int image_id, double x, double y, double k)
		{ return DrawImage(image_id, x, y, k, k); }
	inline void DrawImage(int image_id, double x, double y)
		{ return DrawImage(image_id, x, y, 1.0, 1.0); }
	inline void DrawImage(int image_id, const point &pos, double scale_x, double scale_y)
		{ DrawImage(image_id, pos.x, pos.y, scale_x, scale_y); }
	inline void DrawImage(int image_id, const point &pos, double k)
		{ DrawImage(image_id, pos.x, pos.y, k, k); }
	inline void DrawImage(int image_id, const point &pos)
		{ DrawImage(image_id, pos.x, pos.y, 1.0, 1.0); }
	inline void DrawImage(int image_id, const point &pos, const size &scale)
		{ DrawImage(image_id, pos.x, pos.y, scale.width, scale.height); }

	/*
		Работа с текстом
	*/
	int CreateFont(const wxFont &wxfont);
	void DeleteFont(int font_id);
	size DrawText(int font_id, const std::wstring &str, const point &pos,
		const color &text_color,
		const ratio &center = ratio(0.5, 0.5),
		const ratio &scale = ratio());

	DECLARE_EVENT_TABLE()


private:
	typedef std::map<int, map_info> maps_list;
	typedef boost::unordered_map<std::wstring, int> maps_name_to_id_list;
	typedef my::mru::list<tile::id, tile::ptr> tiles_cache;
	typedef boost::unordered_map<int, sprite::ptr> sprites_list;
	typedef boost::unordered_map<int, font::ptr> fonts_list;


	/*
		Open GL
	*/

	typedef std::list<GLuint> texture_id_list;
	wxGLContext gl_context_;
	GLuint magic_id_;
	int load_texture_debug_counter_;
	texture_id_list delete_texture_queue_;
	mutex delete_texture_mutex_;
	int delete_texture_debug_counter_;

	void magic_init();
	void magic_deinit();
	void magic_exec();

	static void check_gl_error();
	void paint_tile(const tile::id &tile_id, int level = 0);
	void load_textures();
	void delete_texture_later(GLuint texture_id);
	void delete_texture(GLuint id);
	void delete_textures();


	/*
		Работа с сервером
	*/

	asio::io_service io_service_; /* Служба, обрабатывающая запросы к серверу */
	asio::ip::tcp::endpoint server_endpoint_; /* Адрес сервера */

	/* Загрузка данных с сервера */
	void get(my::http::reply &reply, const std::wstring &request);
	/* Загрузка и сохранение файла с сервера */
	unsigned int load_and_save(const std::wstring &request,
		const std::wstring &local_filename);
	/* Загрузка и сохранение xml-файла (есть небольшие отличия от сохранения
		простых фалов) с сервера */
	unsigned int load_and_save_xml(const std::wstring &request,
		const std::wstring &local_filename);


	/*
		Кэш тайлов
	*/

	std::wstring cache_path_; /* Путь к кэшу */
	bool only_cache_; /* Использовать только кэш */
	tiles_cache cache_; /* Кэш */
	int cache_active_tiles_;
	int basis_map_id_;
	int basis_z_;
	int basis_tile_x1_;
	int basis_tile_y1_;
	int basis_tile_x2_;
	int basis_tile_y2_;
	shared_mutex cache_mutex_; /* Мьютекс для кэша */

	/* Проверка корректности координат тайла */
	inline bool check_tile_id(const tile::id &tile_id);

	/* Поиск тайла в кэше (только поиск!) */
	inline tile::ptr find_tile(const tile::id &tile_id);

	/* Поиск тайла в кэше. При необходимости - построение и загрузка */
	int builder_debug_counter_;
	inline tile::ptr get_tile(const tile::id &tile_id);


	/*
		Загрузка тайлов
	*/

	my::worker::ptr file_loader_; /* "Работник" файловой очереди (синхронизация) */
	tiles_cache::iterator file_iterator_; /* Итератор по кэшу */
	int file_loader_dbg_loop_;
	int file_loader_dbg_load_;

	my::worker::ptr server_loader_; /* "Работник" серверной очереди (синхронизация) */
	tiles_cache::iterator server_iterator_; /* Итератор по кэшу */
	int server_loader_dbg_loop_;
	int server_loader_dbg_load_;

	/* Функции потоков */
	void file_loader_proc(my::worker::ptr this_worker);
	void server_loader_proc(my::worker::ptr this_worker);


	/*
		Анимация
	*/

	my::worker::ptr animator_; /* "Работник" для анимации */
	posix_time::time_duration anim_period_; /* Период анимации */
	int def_min_anim_steps_; /* Минимальное кол-во шагов анимации */
	my::stopwatch anim_speed_sw_;
	double anim_speed_;
	my::stopwatch anim_freq_sw_;
	double anim_freq_;
	int animator_debug_counter_;

	void anim_thread_proc(my::worker::ptr this_worker);


	/*
		Список карт, имеющихся на сервере
	*/

	maps_list maps_; /* Список карт (по числовому id) */
	maps_name_to_id_list maps_name_to_id_; /* name -> id */

	/* Уникальный идентификатор загруженный карты */
	static int get_new_map_id()
	{
		static int id = 0;
		return ++id;
	}


	/*
		Отображение карты
	*/

	int draw_tile_debug_counter_;
	mutex paint_mutex_;
	recursive_mutex params_mutex_;
	int active_map_id_; /* Активная карта */
	double z_; /* Текущий масштаб */
	double new_z_;
	int z_step_;
	//fast_point central_point_; /* Координаты центра экрана */
	fast_point screen_pos_; /* Координаты центра экрана */
	//ratio center_scr_ratio_; /* Позиция центральной точки относительно границ экрана */
	rel_point center_pos_; /* Позиция центральной точки относительно границ экрана */
	int central_cross_step_;
	double central_cross_alpha_;
	int painter_debug_counter_;
	bool move_mode_;
	bool force_repaint_; /* Флаг обязательной перерисовки */
	point mouse_pos_;
	int system_font_id_;

	//void paint_debug_info(wxDC &gc, int width, int height);
	//void paint_debug_info(wxGraphicsContext &gc, int width, int height);
	//template<class DC>
	//void paint_debug_info_int(DC &gc, int width, int height);

	boost::thread::id paint_thread_id_;
	void repaint(wxPaintDC &dc);

	/* Размеры экрана */
	size get_screen_size();
	point get_screen_max_point();

	/* Центр экрана */
	void move_screen_to(const point &pos);
	void set_screen_pos(const point &pos);


	/*
		Обработчики событий окна
	*/

	on_paint_proc_t on_paint_handler_;

	void on_paint(wxPaintEvent& event);
	void on_erase_background(wxEraseEvent& event);
	void on_size(wxSizeEvent& event);
	void on_left_down(wxMouseEvent& event);
	void on_left_up(wxMouseEvent& event);
	void on_capture_lost(wxMouseCaptureLostEvent& event);
	void on_mouse_move(wxMouseEvent& event);
	void on_mouse_wheel(wxMouseEvent& event);


	/*
		Работа с изображениями
	*/
	int sprites_index_;
	sprites_list sprites_;
	shared_mutex sprites_mutex_;
	image::on_delete_t on_image_delete_;

	void on_image_delete_proc(image &img);


	/*
		Работа с текстом
	*/
	int fonts_index_;
	fonts_list fonts_;
	shared_mutex fonts_mutex_;

};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_H */
