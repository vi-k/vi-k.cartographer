#ifndef WX_CARTOGRAPHER_H
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
#include <my_inet.h> /* Инклуды и namespace для boost::asio */
#include <my_http.h> /* http-протокол */
#include <my_thread.h> /* Инклуды и namespace для boost::thread, boost::mutex... */
#include <my_employer.h> /* "Работодатель" - контроль работы потоков */
#include <my_mru.h> /* MRU-лист */
#include <my_fs.h> /* boost::filesystem -> fs */

#include <cstddef> /* std::size_t */
#include <string>
#include <map>

#include <boost/unordered_map.hpp>

#include <wx/window.h>
#include <wx/bitmap.h> 
#include <wx/graphics.h> /* wxGraphicsContext */

#define wxCart_ONLYCACHE 1

class wxCartographer : my::employer
{
public:
	
	struct map
	{
		enum projection_t {spheroid /*Google*/, ellipsoid /*Yandex*/};
		std::wstring id;
		std::wstring name;
		bool is_layer;
		std::wstring tile_type;
		std::wstring ext;
		projection_t projection;
	};

	/* Идентификатор тайла */
	struct tile_id
	{
		int map_id;
		int z;
		int x;
		int y;

		tile_id()
			: map_id(0), z(0), x(0), y(0) {}

		tile_id(int map_id, int z, int x, int y)
			: map_id(map_id), z(z), x(x), y(y) {}

		tile_id(const tile_id &other)
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

		inline bool operator==(const tile_id &other) const
		{
			return map_id == other.map_id
				&& z == other.z
				&& x == other.x
				&& y == other.y;
		}

		friend std::size_t hash_value(const tile_id &t)
		{
			std::size_t seed = 0;
			boost::hash_combine(seed, t.map_id);
			boost::hash_combine(seed, t.z);
			boost::hash_combine(seed, t.x);
			boost::hash_combine(seed, t.y);

			return seed;
		}
	};

	/* Содержимое тайла */
	class tile
	{
	public:
		typedef shared_ptr<tile> ptr;

	private:
		wxBitmap bitmap_;
	
	public:
		tile(const std::wstring &filename)
		{
			if (fs::exists(filename))
				bitmap_.LoadFile(filename, wxBITMAP_TYPE_ANY);
		}

		inline bool loaded()
			{ return bitmap_.IsOk(); }

		inline wxBitmap& bitmap()
			{ return bitmap_; }
	};

private:
	typedef std::map<int, map> maps_list;
	typedef boost::unordered_map<std::wstring, int> maps_id_list;
	typedef my::mru::list<tile_id, tile::ptr> tiles_cache;
	typedef my::mru::list<tile_id, int> tiles_queue;

	unsigned long flags_; /* Параметры */
	std::wstring cache_path_; /* Путь к кэшу */
	asio::io_service io_service_; /* Служба, обрабатывающая запросы к серверу */
	asio::ip::tcp::endpoint server_endpoint_; /* Сервер */
	maps_list maps_; /* Список карт, имеющихся на сервере */
	maps_id_list maps_id_; /* Соответствие номера карты и его строкового идентификатора */
	tiles_cache cache_; /* Кэш тайлов */
	shared_mutex cache_mutex_; /* Мьютекс для кэша */
	tiles_queue file_queue_; /* Очередь загрузки тайлов с диска */
	my::worker::ptr file_loader_; /* "Работник" для "файловой" очереди */
	tiles_queue server_queue_; /* Очередь загрузки тайлов с сервера */
	my::worker::ptr server_loader_; /* "Работник" для "серверной" очереди */

	wxWindow *window_; /* Окно для прорисовки */
	wxBitmap background_; /* Буфер для фона (т.е. для самой карты, до "порчи" пользователем ) */
	wxBitmap buffer_; /* Буфер для прорисовки (после "порчи пользователем) */
	mutex paint_mutex_;
	wxDouble z_; /* Текущий масштаб */
	int active_map_id_; /* Активная карта */
	wxDouble lat_; /* Координаты центра экрана: */
	wxDouble lon_; /* долгота и широта */

	void file_loader_proc(my::worker::ptr this_worker);
	void server_loader_proc(my::worker::ptr this_worker);
	//void io_thread_proc();

	bool check_buffer();

	static int get_new_map_id()
	{
		static int id = 0;
		return ++id;
	}

	void add_to_cache(const tile_id &id, tile::ptr ptr);
	void add_to_file_queue(const tile_id &id);
	void add_to_server_queue(const tile_id &id);

	/* Загрузка данных с сервера */
	void get(my::http::reply &reply, const std::wstring &request);
	/* Загрузка и сохранение файла с сервера */
	unsigned int load_and_save(const std::wstring &request,
		const std::wstring &local_filename);
	/* Сохранение "простого" и xml- файлов отличаются! */
	unsigned int load_and_save_xml(const std::wstring &request,
		const std::wstring &local_filename);

	
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

	
	/* Прорисовка карты */

	void prepare_background(wxBitmap &bitmap, wxDouble width, wxDouble height,
		int map_id, int z, wxDouble lat, wxDouble lon);

	tile::ptr get_tile(int map_id, int z, int x, int y);
	void paint_map(wxGraphicsContext *gc, wxDouble width, wxDouble height,
		int map_id, wxDouble lat, wxDouble lon, wxDouble z);

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
		std::wstring cache_path, unsigned long flags);
	~wxCartographer();

	void Repaint();
};


#endif
