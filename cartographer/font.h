#ifndef CARTOGRAPHER_FONT_H
#define CARTOGRAPHER_FONT_H

#include "config.h"
#include "defs.h"
#include "image.h"

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
	font() {}
	font(wxFont &font,
		image::on_delete_t on_image_delete = image::on_delete_t());

	void draw(const std::wstring &str, double x, double y);

private:
	typedef boost::unordered_map<int, image::ptr> images_list;
	typedef boost::unordered_map<wchar_t, char_info> chars_list;

	wxFont font_;
	image::on_delete_t on_image_delete_;
	int images_index_;
	images_list images_;
	chars_list chars_;

	std::wstring chars_from_range(wchar_t first, wchar_t last);
	void prepare_chars(const std::wstring &chars);
};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_FONT_H */
