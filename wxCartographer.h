﻿#ifndef WX_CARTOGRAPHER_H
#define WX_CARTOGRAPHER_H

/* Эта часть не должна изменяться! */
#include <boost/config/warning_disable.hpp> /* против unsafe в wxWidgets */

#undef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp> /* Сокеты, таймеры, асинхронные операции.
                             Обязательно до включения windows.h! */
#include <wx/msw/winundef.h>
#include <wx/msw/setup.h> /* Обязательно самым первым среди wxWidgets! */
#include <wx/msgdlg.h>    /* А это вторым! */


/* Все инклуды только отсюда! */
#include <my_inet.h> /* boost::asio */
#include <my_http.h> /* http-протокол */
#include <my_thread.h> /* boost::thread, boost::mutex... */
#include <my_employer.h> /* "Работодатель" - контроль работы потоков */
#include <my_mru.h> /* MRU-лист */
#include <my_fs.h> /* boost::filesystem */
#include <my_time.h> /* boost::posix_time */
#include <my_stopwatch.h> /* Секундомер */

#include <cstddef> /* std::size_t */
#include <string>
#include <map>
#include <vector>

#include <boost/unordered_map.hpp>

#include <wx/window.h>
#include <wx/bitmap.h> 
#include <wx/dcgraph.h> /* wxGCDC */
#include <wx/graphics.h> /* wxGraphicsContext */
#include <wx/mstream.h>  /* wxMemoryInputStream */

class wxCartographer : my::employer
{
public:
	
	/* Карта */
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

	/* Тайл */
	class tile
	{
	public:
		typedef shared_ptr<tile> ptr;

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
		};

	private:
		bool need_for_load_; /* Тайл нуждается в загрузке */
		int level_; /* Расстояние до предка, отображённого на тайле
			(при ручном построении тайла), при 0 - на тайле отображён сам тайл
			(т.е. тайл был успешно загружен) */
		wxBitmap bitmap_;
	
	public:
		/* Создание "пустого" тайла - чтобы не загружать повторно */
		tile()
			: need_for_load_(false)
			, level_(999) {}

		/* Создание "чистого" тайла для построения */
		tile(int level)
			: need_for_load_(true)
			, level_(level)
			, bitmap_(256, 256) {}

		/* Загрузка из файла */
		tile(const std::wstring &filename)
			: need_for_load_(false)
		{
			if (fs::exists(filename))
			{
				//bitmap_.LoadFile(filename, wxBITMAP_TYPE_ANY);
				wxImage image(filename);
				if (image.IsOk())
				{
					image.InitAlpha();
					bitmap_ = wxBitmap(image);
				}
			}
			
			level_ = ok() ? 0 : 999;
		}

		/* Загрузка из памяти */
		tile(const void *data, std::size_t size)
			: need_for_load_(false)
		{
			wxImage image;
			wxMemoryInputStream stream(data, size);
			
			if (!image.LoadFile(stream, wxBITMAP_TYPE_ANY) )
			{
				level_ = 999;
			}
			else
			{
				image.InitAlpha();
				bitmap_ = wxBitmap(image);
				level_ = 0;
			}
		}

		inline wxBitmap& bitmap()
			{ return bitmap_; }

		inline bool ok()
			{ return bitmap_.IsOk(); }

		inline bool need_for_load()
			{ return need_for_load_; }

		inline void reset_need_for_load()
			{ need_for_load_ = false; }

		inline int level()
			{ return level_; }

		inline void set_level(int level)
			{ level_ = level; }

		inline bool need_for_build()
			{ return level_ > 1; }

		inline bool loaded()
			{ return level_ == 0; }
	};

private:
	typedef std::map<int, map> maps_list;
	typedef boost::unordered_map<std::wstring, int> maps_name_to_id_list;
	typedef my::mru::list<tile::id, tile::ptr> tiles_cache;
	typedef my::mru::list<tile::id, int> tiles_queue;


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
	shared_mutex cache_mutex_; /* Мьютекс для кэша */
	
	/* Добавление загруженного тайла в кэш */
	void add_to_cache(const tile::id &tile_id, tile::ptr ptr);
	
	/* Проверка - нуждается ли тайл в загрузке */
	inline bool need_for_load(tile::ptr ptr);
	
	/* Проверка - нуждается ли тайл в построении */
	inline bool need_for_build(tile::ptr ptr);
	
	/* Проверка корректности координат тайла */
	inline bool check_tile_id(const tile::id &tile_id);

	/* Поиск тайла в кэше (только поиск!) */
	inline tile::ptr find_tile(const tile::id &tile_id);

	/* Построение тайлов на основании тайлов-предков (тайлов меньшего масштаба) */
	tile::ptr build_tile(const tile::id &tile_id);
	int builder_debug_counter_;

	/* Поиск тайла в кэше. При необходимости - построение и загрузка */
	inline tile::ptr get_tile(const tile::id &tile_id);


	/*
		Загрузка тайлов
	*/

	tiles_queue file_queue_; /* Очередь загрузки с дискового кэша (файловая очередь) */
	my::worker::ptr file_loader_; /* "Работник" файловой очереди (синхронизация) */
	int file_loader_debug_counter_;

	tiles_queue server_queue_; /* Очередь загрузки с сервера (серверная очередь) */
	my::worker::ptr server_loader_; /* "Работник" серверной очереди (синхронизация) */
	int server_loader_debug_counter_;

	/* Добавление тайла в очередь */
	void add_to_file_queue(const tile::id &tile_id);
	void add_to_server_queue(const tile::id &tile_id);

	/* Функции потоков */
	void file_loader_proc(my::worker::ptr this_worker);
	void server_loader_proc(my::worker::ptr this_worker);

	/* Сортировка тайлов по расстоянию от текущего центра экрана */
	void sort_queue(tiles_queue &queue, my::worker::ptr worker);

	/* Сортировка тайлов по расстоянию от заданного тайла */
	static void sort_queue(tiles_queue &queue, const tile::id &tile,
		my::worker::ptr worker);
	
	/* Функция сортировки */
	static bool sort_by_dist( tile::id tile,
		const tiles_queue::item_type &first,
		const tiles_queue::item_type &second );


	/*
		Анимация
	*/

	my::worker::ptr animator_; /* "Работник" для анимации */
	posix_time::time_duration anim_period_; /* Период анимации */
	int def_anim_steps_; /* Кол-во шагов анимации */
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

	wxWindow *window_; /* Окно для прорисовки */
	wxBitmap background_; /* Буфер для фона (т.е. для самой карты, до "порчи" пользователем ) */
	wxBitmap buffer_; /* Буфер для прорисовки (после "порчи пользователем) */
	int draw_tile_debug_dounter_;
	mutex paint_mutex_;
	shared_mutex params_mutex_;
	int active_map_id_; /* Активная карта */
	wxDouble z_; /* Текущий масштаб */
	wxDouble lat_; /* Координаты центра экрана: */
	wxDouble lon_; /* долгота и широта */
	
	/* Нарисовать карту */
	void paint_map(wxDC &gc, wxCoord width, wxCoord height,
		int map_id, int z, wxDouble lat, wxDouble lon);
	
	void paint_debug_info(wxDC &gc, wxCoord width, wxCoord height);
	void paint_debug_info(wxGraphicsContext &gc, wxCoord width, wxCoord height);
	
	template<class DC>
	void paint_debug_info_int(DC &gc, wxCoord width, wxCoord height);

	void repaint();


	bool check_buffer();

	/* Координаты, размеры... */
	
	/* Размер мира в тайлах для заданного масштаба */
	static inline int size_for_int_z(int z)
		{ return 1 << (z - 1); }
	/* Размер мира в тайлах - для дробного z чуть сложнее, чем для целого */
	static inline wxDouble size_for_z(wxDouble z);
	/* Долгота -> x */
	static inline wxDouble lon_to_x(wxDouble lon, wxDouble z);
	/* Широта -> y (зависти от типа проекции (spheroid, ellipsoid )) */
	static inline wxDouble lat_to_y(wxDouble lat, wxDouble z, 
		map::projection_t projection);

	
	/* Обработчики событий окна */
	void OnPaint(wxPaintEvent& event);
	void OnEraseBackground(wxEraseEvent& event);
	void OnLeftDown(wxMouseEvent& event);
	void OnLeftUp(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);
public:
	wxCartographer(wxWindow *window, const std::wstring &server,
		const std::wstring &port, std::size_t cache_size,
		std::wstring cache_path, bool only_cache,
		const std::wstring &init_map, int init_z, wxDouble init_lat, wxDouble init_lon,
		int anim_period = 60, int def_anim_steps = 4);
	~wxCartographer();

	void Update();
	void GetMaps(std::vector<map> &maps);
	wxCartographer::map GetActiveMap();
	bool SetActiveMap(const std::wstring &map_name);
};


#endif
