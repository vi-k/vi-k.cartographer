#ifndef CARTOGRAPHER_BASE_H
#define CARTOGRAPHER_BASE_H

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

#ifndef __swprintf
#ifdef BOOST_WINDOWS
	#if defined(_MSC_VER)
		#define __swprintf swprintf_s
	#else
		#define __swprintf snwprintf
	#endif
#else
	#define __swprintf swprintf
#endif
#endif

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


wxDECLARE_EVENT(MY_EVENT, wxCommandEvent);

/*
	Картографер
*/
class Base : protected my::employer, public wxGLCanvas
{
public:
	/* Обработчик прорисовки */
	typedef boost::function<void (double z, const size &screen_size)> on_paint_proc_t;

	/* Конструктор */
	Base(wxWindow *parent, const std::wstring &server_addr,
		const std::wstring &init_map, std::size_t cache_size);

	virtual ~Base();

	DECLARE_EVENT_TABLE()

protected:
	typedef std::map<int, map_info> maps_list;
	typedef boost::unordered_map<std::wstring, int> maps_name_to_id_list;
	typedef my::mru::list<tile::id, tile::ptr> tiles_cache;
	typedef boost::unordered_map<int, sprite::ptr> sprites_list;
	typedef boost::unordered_map<int, font::ptr> fonts_list;

	void stop(); /* Остановка Картографера */
	void update(); /* Сейчас не действует. Только перерисовка за счёт анимации! */


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
	int map_id_; /* Активная карта */
	projection map_pr_;
	double z_; /* Текущий масштаб */
	double new_z_;
	int z_step_;
	fast_point screen_pos_; /* Координаты центра экрана */
	rel_point center_pos_; /* Позиция центральной точки относительно границ экрана */
	int central_cross_step_;
	double central_cross_alpha_;
	int painter_debug_counter_;
	bool move_mode_;
	point mouse_pos_;

	boost::thread::id paint_thread_id_;
	void repaint();
	virtual void after_repaint(const size &screen_size) {};

	/* Размеры экрана */
	size get_screen_size();
	point get_screen_max_point();

	/* Центр экрана */
	void move_screen_to(const point &pos);
	void set_screen_pos(const point &pos);

	/* Масштаб */
	void set_z(int z);


	/*
		Обработчики событий окна
	*/
	on_paint_proc_t on_paint_handler_;

	enum {MY_ID_REPAINT = 1};

	void send_my_event(int cmd_id);
	void on_my_event(wxCommandEvent& event);

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
	image::on_delete_t on_image_delete_;

	void on_image_delete_proc(image &img);
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_BASE_H */
