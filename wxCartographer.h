#ifndef WX_CARTOGRAPHER_H
#define WX_CARTOGRAPHER_H

/***** Эта часть не должна изменяться! *****/
	#include <boost/config/warning_disable.hpp> /* против unsafe в wxWidgets */
	#include <boost/config.hpp>

	#ifdef BOOST_WINDOWS
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
		#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN /* Необходимо для Asio */
	#endif

	#include <boost/asio.hpp> /* Обязательно до включения windows.h */
	#include <wx/wxprec.h>
/*******************************************/


#include "raw_image.h"

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

wxDECLARE_EVENT(WXCART_LOAD_TEXTURE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WXCART_DELETE_TEXTURE_EVENT, wxCommandEvent);

extern my::log main_log;

class wxCartographer : public wxGLCanvas, my::employer
{
public:
	typedef boost::function<void (wxGCDC &gc, wxCoord width, wxCoord height)> OnPaintProc_t;

	
	/*
		Буфер для отрисовки карты
	*/
	struct wxCartographerBuffer
	{
		wxBitmap bitmap;
		int map_id;
		int z;
		int first_tile_x;
		int first_tile_y;
		int last_tile_x;
		int last_tile_y;

		wxCartographerBuffer()
			: map_id(0)
			, z(0)
			, first_tile_x(0)
			, first_tile_y(0)
			, last_tile_x(0)
			, last_tile_y(0)
		{
		}
	};

	
	/*
		Описание карты
	*/
	struct map
	{
		enum projection_t {spheroid /*Google*/, ellipsoid /*Yandex*/};
		std::wstring sid;
		std::wstring name;
		bool is_layer;
		std::wstring tile_type;
		std::wstring ext;
		projection_t projection;
	};

	
	/*
		Тайл
	*/
	class tile
	{
	public:
		typedef shared_ptr<tile> ptr;
		enum state_t {fail, empty, file_loading, server_loading, texture_generating, absent, ready};

		/* Идентификатор тайла */
		struct id
		{
			int map_id;
			int z;
			int x;
			int y;

			id()
				: map_id(0), z(0), x(0), y(0) {}

			id(int map_id, int z, int x, int y)
				: map_id(map_id), z(z), x(x), y(y) {}

			id(const id &other)
				: map_id(other.map_id)
				, z(other.z)
				, x(other.x)
				, y(other.y) {}

			inline bool operator!() const
			{
				return map_id == 0
					&& z == 0
					&& x == 0
					&& y == 0;
			}

			inline bool operator==(const id &other) const
			{
				return map_id == other.map_id
					&& z == other.z
					&& x == other.x
					&& y == other.y;
			}

			friend std::size_t hash_value(const id &t)
			{
				std::size_t seed = 0;
				boost::hash_combine(seed, t.map_id);
				boost::hash_combine(seed, t.z);
				boost::hash_combine(seed, t.x);
				boost::hash_combine(seed, t.y);

				return seed;
			}
		}; /* struct tile::id */

	private:
		friend class wxCartographer;
		wxCartographer &cartographer_;
		state_t state_;
		raw_image *image_;
		GLuint texture_id_;

	public:
		tile(wxCartographer &cartographer, state_t state = empty)
			: cartographer_(cartographer)
			, state_(state)
			, image_(0)
			, texture_id_(0)
		{
		}

		~tile()
		{
			clear();
		}

		void clear()
		{
			state_ = empty;

			if (image_)
			{
				delete image_;
				image_ = 0;
			}

			if (texture_id_)
			{
				cartographer_.post_delete_texture(texture_id_);
				texture_id_ = 0;
			}
		}

		/* Загрузка из файла */
		bool load_from_file(const std::wstring &filename)
		{
			clear();

			wxImage wx_image(filename);
			if (wx_image.IsOk())
			{
				image_ = wxCartographer::convert_to_raw(wx_image);
				if (image_)
					state_ = texture_generating;
			}

			return image_ != 0;
		}

		/* Загрузка из памяти */
		bool load_from_mem(const void *data, std::size_t size)
		{
			clear();

			wxImage wx_image;
			wxMemoryInputStream stream(data, size);
			if (wx_image.LoadFile(stream, wxBITMAP_TYPE_ANY) )
			{
				image_ = wxCartographer::convert_to_raw(wx_image);
				if (image_)
					state_ = texture_generating;
			}

			return image_ != 0;
		}

		inline void set_texture_id(GLuint texture_id)
		{
			clear();
			texture_id_ = texture_id;
			if (texture_id_)
				state_ = ready;
		}

		inline void set_state(state_t state)
			{ state_ = state; }

		inline state_t state()
			{ return state_; }

		inline raw_image* image()
			{ return image_; }
	
		inline GLuint texture_id()
			{ return texture_id_; }

	}; /* class tile */

private:
	typedef std::map<int, map> maps_list;
	typedef boost::unordered_map<std::wstring, int> maps_name_to_id_list;
	typedef my::mru::list<tile::id, tile::ptr> tiles_cache;
	//typedef my::mru::list<tile::id, int> tiles_queue;

	/*
		Open GL
	*/

	wxGLContext gl_context_;
	GLuint m_textures[7];

	void init_gl();
	void draw_gl(int widthi, int heighti);
	void draw_tile(int tile_x, int tile_y, double alpha, double z, GLuint texture);
	static raw_image* convert_to_raw(const wxImage &src);
	void paint_tile(const tile::id &tile_id, int level = 0);

	int load_texture_debug_counter_;
	GLuint load_texture(raw_image *image);
	void load_textures();

	typedef std::list<GLuint> texture_id_list;
	texture_id_list delete_texture_queue_;
	mutex delete_texture_mutex_;
	int delete_texture_debug_counter_;

	void post_delete_texture(GLuint texture_id);
	void delete_texture(GLuint id);
	void clear_textures();

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

	/* Добавление загруженного тайла в кэш */
	//void add_to_cache(const tile::id &tile_id, tile::ptr ptr);

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

	//tiles_queue file_queue_; /* Очередь загрузки с дискового кэша (файловая очередь) */
	my::worker::ptr file_loader_; /* "Работник" файловой очереди (синхронизация) */
	tiles_cache::iterator file_iterator_; /* Итератор по кэшу */
	int file_loader_dbg_loop_;
	int file_loader_dbg_load_;

	//tiles_queue server_queue_; /* Очередь загрузки с сервера (серверная очередь) */
	my::worker::ptr server_loader_; /* "Работник" серверной очереди (синхронизация) */
	tiles_cache::iterator server_iterator_; /* Итератор по кэшу */
	int server_loader_dbg_loop_;
	int server_loader_dbg_load_;

	/* Добавление тайла в очередь */
	//void add_to_file_queue(const tile::id &tile_id);
	//void add_to_server_queue(const tile::id &tile_id);

	/* Функции потоков */
	void file_loader_proc(my::worker::ptr this_worker);
	void server_loader_proc(my::worker::ptr this_worker);

	/* Сортировка тайлов по расстоянию от текущего центра экрана */
	//void sort_queue(tiles_queue &queue, my::worker::ptr worker);

	/* Сортировка тайлов по расстоянию от заданного тайла */
	//static void sort_queue(tiles_queue &queue, const tile::id &tile,
	//	my::worker::ptr worker);

	/* Функция сортировки */
	//static bool sort_by_dist( tile::id tile,
	//	const tiles_queue::item_type &first,
	//	const tiles_queue::item_type &second );

	/* Проверка на наличие тайла в очереди */
	//bool tile_in_queue(const tiles_queue &queue,
	//	my::worker::ptr worker, const tile::id &tile_id);


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

	wxCartographerBuffer background1_; /* Буфер для фона (карта без объектов) */
	wxCartographerBuffer background2_; /* Буфер для фона (карта без объектов) */
	wxBitmap buffer_; /* Буфер для прорисовки, равен размерам экрана */
	int draw_tile_debug_counter_;
	mutex paint_mutex_;
	recursive_mutex params_mutex_;
	int active_map_id_; /* Активная карта */
	double z_; /* Текущий масштаб */
	double new_z_;
	int z_step_;
	double fix_kx_; /* Координаты точки экрана (от 0.0 до 1.0), */
	double fix_ky_; /* остающейся фиксированной при изменении масштаба */
	double fix_lat_; /* Географические координаты этой точки */
	double fix_lon_;
	int painter_debug_counter_;
	int backgrounder_debug_counter_;
	bool move_mode_;
	bool force_repaint_; /* Флаг обязательной перерисовки */

	/* Подготовка фона (карты) к отрисовке */
	void prepare_background(wxCartographerBuffer &buffer,
		wxCoord width, wxCoord height, bool force_repaint, int map_id, int z,
		double fix_tile_x, double fix_tile_y,
		double fix_scr_x, double fix_scr_y);

	/* Нарисовать карту */
	void paint_map(wxGCDC &gc, wxCoord width, wxCoord height);

	void paint_debug_info(wxDC &gc, wxCoord width, wxCoord height);
	void paint_debug_info(wxGraphicsContext &gc, wxCoord width, wxCoord height);

	template<class DC>
	void paint_debug_info_int(DC &gc, wxCoord width, wxCoord height);

	void repaint(wxDC &dc);

	/* Размеры рабочей области */
	template<typename SIZE>
	void get_viewport_size(SIZE *p_width, SIZE *p_height)
	{
		wxCoord w, h;
		GetClientSize(&w, &h);
		*p_width = (SIZE)w;
		*p_height = (SIZE)h;
	}

	/*-
	inline wxCoord widthi()
		{ return buffer_.GetWidth(); }
	inline wxCoord heighti()
		{ return buffer_.GetHeight(); }
	inline double widthd()
		{ return (double)buffer_.GetWidth(); }
	inline double heightd()
		{ return (double)buffer_.GetHeight(); }
	-*/

	/* Назначить новую fix-точку */
	void set_fix_to_scr_xy(double scr_x, double scr_y);

	/* Передвинуть fix-точку в новые координаты */
	void move_fix_to_scr_xy(double scr_x, double scr_y);


	/*
		Преобразование координат
	*/

	/* Размер "мира" в тайлах, для заданного масштаба */
	static inline int size_for_z_i(int z)
	{
		return 1 << (z - 1);
	}

	/* Размер мира в тайлах - для дробного z дробный результат */
	static inline double size_for_z_d(double z)
	{
		/* Размер всей карты в тайлах.
			Для дробного z - чуть посложнее, чем для целого */
		int iz = (int)z;
		return (double)(1 << (iz - 1)) * (1.0 + z - iz);
	}


	/* Градусы -> тайловые координаты */
	static inline double lon_to_tile_x(double lon, double z);
	static inline double lat_to_tile_y(double lat, double z,
		map::projection_t projection);

	/* Градусы -> экранные координаты */
	static inline double lon_to_scr_x(double lon, double z,
		double fix_lon, double fix_scr_x);
	static inline double lat_to_scr_y(double lat, double z,
		map::projection_t projection, double fix_lat, double fix_scr_y);

	/* Тайловые координаты -> градусы */
	static inline double tile_x_to_lon(double x, double z);
	static inline double tile_y_to_lat(double y, double z,
		map::projection_t projection);

	/* Экранные координаты -> градусы */
	static inline double scr_x_to_lon(double x, double z,
		double fix_lon, double fix_scr_x);
	static inline double scr_y_to_lat(double y, double z,
		map::projection_t projection, double fix_lat, double fix_scr_y);


	/*
		Обработчики событий окна
	*/

	OnPaintProc_t on_paint_;

	void on_paint(wxPaintEvent& event);
	void on_erase_background(wxEraseEvent& event);
	void on_size(wxSizeEvent& event);
	void on_left_down(wxMouseEvent& event);
	void on_left_up(wxMouseEvent& event);
	void on_capture_lost(wxMouseCaptureLostEvent& event);
	void on_mouse_move(wxMouseEvent& event);
	void on_mouse_wheel(wxMouseEvent& event);

public:
	wxCartographer(wxWindow *parent, const std::wstring &serverAddr,
		const std::wstring &serverPort, std::size_t cacheSize,
		std::wstring cachePath, bool onlyCache,
		const std::wstring &initMap, int initZ, double initLat, double initLon,
		OnPaintProc_t onPaintProc,
		int animPeriod = 0, int defMinAnimSteps = 1);
	~wxCartographer();

	void Stop();

	void Repaint();
	void GetMaps(std::vector<map> &Maps);
	wxCartographer::map GetActiveMap();
	bool SetActiveMap(const std::wstring &MapName);

	wxCoord LatToY(double Lat);
	wxCoord LonToX(double Lon);

	int GetZ();
	void SetZ(int z);

	double GetLat();
	double GetLon();
	void MoveTo(int z, double lat, double lon);

	static inline double DegreesToCoord(double deg, double min, double sec)
	{
		return deg + min / 60.0 + sec / 3600.0;
	}

	static inline void CoordToDegrees(double coord, int &deg, int &min, double &sec)
	{
		deg = (int)coord;
		double m = (coord - deg) * 60.0;
		min = (int)m;
		sec = (m - min) * 60.0;
	}

	DECLARE_EVENT_TABLE()
};

#define FROM_DEG(d,m,s) wxCartographer::DegreesToCoord(d,m,s)
#define TO_DEG(c,d,m,s) wxCartographer::CoordToDegrees(c,d,m,s)


#endif
