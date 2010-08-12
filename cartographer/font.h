#ifndef CARTOGRAPHER_FONT_H
#define CARTOGRAPHER_FONT_H

#include "config.h"
#include "defs.h"
#include "image.h"

#include <my_ptr.h>

#include <string>

#include <wx/font.h>

namespace cartographer
{

struct char_info
{
	int image_id;
	int pos;
	int width;
	int height;
	size border;

	char_info()
		: image_id(0)
		, pos(0)
		, width(0)
		, height(0)
		, border(0, 0) {}

	char_info(int image_id, int pos, int width, int height, size border = size())
		: image_id(image_id)
		, pos(pos)
		, width(width)
		, height(height)
		, border(border) {}
};

class font
{
public:
	typedef shared_ptr<font> ptr;

	font() {}
	font(const wxFont &font, image::on_delete_t on_image_delete = image::on_delete_t())
		: font_(font)
		, on_image_delete_(on_image_delete)
		, images_index_(0) {}

	/* Предварительное создание текстур для символов, чтобы не делать
		это во время отрисовки. Символы задаются в виде: A-Za-zА-Яа-яЁё */
	inline void prepare(const std::wstring &ranges)
		{ prepare_chars( chars_from_ranges(ranges) ); }
	
	/* Вывод текста. Все символы, для которых ещё
		не были загружены текстуры, будут загружены */
	size draw(const std::wstring &str, const point &pos,
		const ratio &center, const ratio &scale);

private:
	typedef boost::unordered_map<int, image::ptr> images_list;
	typedef boost::unordered_map<wchar_t, char_info> chars_list;

	wxFont font_;
	image::on_delete_t on_image_delete_;
	int images_index_;
	images_list images_;
	chars_list chars_;

	std::wstring chars_from_ranges(const std::wstring &ranges);
	void prepare_chars(const std::wstring &chars);
	void prepare_chars__(const std::wstring &chars);
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_FONT_H */
