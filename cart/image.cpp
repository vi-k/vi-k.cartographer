#include "image.h"

#include <cstring>
#include <wx/mstream.h> /* wxMemoryInputStream */

namespace cart
{

/* Число кратное 2, большее или равное a */
inline int __p2(int a)
{
	int res = 1;
	while (res < a)
		res <<= 1;
	return res;
}

bool image::convert_from(const wxImage &src)
{
	if (!src.IsOk())
		return false;

	unique_lock<recursive_mutex> lock(mutex_);

	unsigned char *src_rgb = src.GetData();
	unsigned char *src_a = src.GetAlpha();

	width_ = src.GetWidth();
	height_ = src.GetHeight();

    /* Размеры OpenGL-текстур должны быть кратны 2 */
	int raw_width = __p2(width_);
	int raw_height = __p2(height_);
	int dw = raw_width - width_;

	/* Т.к. возможно понадобится дополнять текстуру прозрачными точками,
		делаем RGBA-изображение вне зависимости от его исходного bpp */
	raw_.create(raw_width, raw_height, 32, GL_RGBA);

	unsigned char *ptr = raw_.data();
	unsigned char *end = raw_.end();

	for (int i = 0; i < height_; ++i)
	{
		for (int j = 0; j < width_; ++j)
		{
			*ptr++ = *src_rgb++;
			*ptr++ = *src_rgb++;
			*ptr++ = *src_rgb++;
			*ptr++ = src_a ? *src_a++ : 255;
		}
		
		/* Дополняем ширину прозрачными точками */
		if (dw)
		{
			unsigned char *line_end = ptr + dw;
			std::memset(ptr, 0, line_end - ptr);
			ptr = line_end;
		}
	}

	/* Дополняем ширину прозрачными точками */
	std::memset(ptr, 0, end - ptr);

	set_state(ready);

	return true;
}

bool image::load_from_file(const std::wstring &filename)
{
	wxImage wx_image(filename);
	return convert_from(wx_image);
}

bool image::load_from_mem(const void *data, std::size_t size)
{
	wxImage wx_image;
	wxMemoryInputStream stream(data, size);
	return wx_image.LoadFile(stream, wxBITMAP_TYPE_ANY)
		&& convert_from(wx_image);
}

void image::load_from_raw(const unsigned char *data,
	int width, int height, bool with_alpha)
{
	unique_lock<recursive_mutex> lock(mutex_);

	if (with_alpha)
		raw_.create(width, height, 32, GL_RGBA);
	else
		raw_.create(width, height, 24, GL_RGB);

	unsigned char *ptr = raw_.data();

	memcpy(ptr, data, raw_.end() - data);

	set_state(ready);
}

} /* namespace cart */
