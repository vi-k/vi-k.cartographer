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
	int width;
	int height;

	char_info()
		: image_id(0)
		, offset(0)
		, width(0)
		, height(0) {}

	char_info(int image_id, int offset, int width, int height)
		: image_id(image_id)
		, offset(offset)
		, width(width)
		, height(height) {}
};

class font
{
public:
	font(wxFont &font, image::on_delete_t on_image_delete)
		: font_(font)
		, on_image_delete_(on_image_delete)
		, images_index_(0)
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
	typedef boost::unordered_map<int, image::ptr> images_list;
	typedef boost::unordered_map<wchar_t, char_info> chars_list;

	wxFont font_;
	image::on_delete_t on_image_delete_;
	int images_index_;
	images_list images_;
	chars_list chars_;

	void prepare_chars(const std::wstring &chars)
	{
		wxString str = chars;
		wxCoord width, h;

		wxBitmap bmp(1, 1);
		wxMemoryDC dc(bmp);

		dc.GetTextExtent(str, &width, &h, NULL, NULL, font_);

		dc.SelectObject(wxNullBitmap);
		bmp.Create(width, height_);
		dc.SelectObject(bmp);

		dc.DrawText(str, 0, 0);
		dc.SelectObject(wxNullBitmap);

		image::ptr image_ptr( new image(on_image_delete_) );
		if (image_ptr->convert_from( bmp.ConvertToImage() ))
		{
			int image_id = ++images_index_;
			images_[image_id] = image_ptr;
		}

		wchar_t *ptr = chars.c_str();
		while (*ptr)
		{
			char_info ci(int image_id, int offset, int size);
			chars_[*ptr]
		}
	}

};

} /* namespace cartographer */

#endif /* CARTOGRAPHER_FONT_H */
