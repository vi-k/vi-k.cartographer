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
#include <my_http.h>
#include <my_inet.h> /* boost::asio */
#include <my_thread.h> /* boost::thread, boost::mutex... */
#include <my_employer.h> /* "Работодатель" - контроль работы потоков */
#include <my_mru.h> /* MRU-лист */

#include <cstddef> /* std::size_t */
#include <string>
#include <map>

#include <boost/unordered_map.hpp>

#include <wx/window.h>
#include <wx/bitmap.h> 

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
	private:
		wxBitmap bitmap_;
	
	public:
		typedef shared_ptr<tile> ptr;

		tile(const std::wstring &filename)
			: bitmap_(filename) {}

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

	wxWindow *window_; /* Окно для прорисовки */
	asio::io_service io_service_; /* Служба, обрабатывающая запросы к серверу */
	asio::ip::tcp::endpoint server_endpoint_; /* Сервер */
	maps_list maps_; /* Список карт, имеющихся на сервере */
	maps_id_list maps_id_; /* Соответствие номера карты и его строкового идентификатора */
	tiles_cache cache_; /* Кэш тайлов ждущие своей загрузки */
	shared_mutex cache_mutex_; /* Мьютекс для кэша */
	tiles_queue queue_; /* Очередь загрузки тайлов */
	mutex queue_mutex_; /* Мьютекс для очереди */
	my::worker::ptr loader_; /* Указатель на загрузчик тайлов */
	wxBitmap background_; /* Буфер для фона (т.е. для самой карты, до "порчи" пользователем ) */
	wxBitmap buffer_; /* Буфер для прорисовки (после "порчи пользователем) */
	mutex paint_mutex_;

	void loader_proc(my::worker::ptr this_worker);
	//void io_thread_proc();

	bool check_buffer();

	static int get_new_map_id()
	{
		static int id = 0;
		return ++id;
	}

	/* Загрузка данных с сервера */
	void get(my::http::reply &reply, const std::wstring &request);
	unsigned int load_file(const std::wstring &file,
		const std::wstring &file_local);

	/* Обработчики событий окна */
	void OnPaint(wxPaintEvent& event);
	void OnEraseBackground(wxEraseEvent& event);
	void OnLeftDown(wxMouseEvent& event);
	void OnLeftUp(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);
public:
	wxCartographer(wxWindow *window, const std::wstring &server,
		const std::wstring &port, std::size_t cache_size, long flags);
	~wxCartographer();

	void Repaint();
};


#endif
