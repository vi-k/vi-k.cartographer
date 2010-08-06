#ifndef CARTOGRAPHER_FONT_H
#define CARTOGRAPHER_FONT_H

#include "config.h"
#include "defs.h"
#include "image.h"

#include <string>

namespace cartographer
{

struct char_info
{
	int image_id;
	int offset;
	int size;
	GLuint texture_id;

	char_info() : image_id(0), offset(0), size(0), texture_id(0) {}
	char_info(int image_id, int offset, int size)
		: image_id(image_id), offset(offset), size(size), texture_id(0) {}
};

class font
{
public:
	font(wxFont &font, image::on_delete_t on_image_delete)
		: font_(font)
		, height_( font.GetPixelSize().GetHeight() )
		, on_image_delete_(on_image_delete)
	{
		prepare_ranges(L" -~"); /* Ansi: 0x20..0x7E */
	}

	~font()
	{
		for (images_list::iterator iter = images_.begin();
			iter != images_.end(); ++iter)
		{
			
			if (on_image_delete_)
				on_image_delete_(iter->second);
		}
	}

private:
	typedef boost::unordered_map<int, image> images_list;
	typedef boost::unordered_map<wchar_t, char_info> chars_list;

	wxFont font_;
	int height;
	image::on_delete_t on_image_delete_;
	images_list images_;
	chars_list chars_;

	void prepare_chars(const std::wstring &chars)
	{
		wxString str = chars;
		wxCoord w, h;
		wxMemoryDC dc();

		GetTextExtent(str, &w, &h, NULL, NULL, font_);
		DrawText(str, 0, 0);
		GetText
	}

};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_FONT_H */
