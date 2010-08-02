#include "image.h"

#include <wx/mstream.h> /* wxMemoryInputStream */

namespace cart
{

bool image::convert_from(const wxImage &src)
{
	if (!src.IsOk())
		return false;

	unsigned char *src_rgb = src.GetData();
	unsigned char *src_a = src.GetAlpha();

	if (src_a)
		raw_.create(src.GetWidth(), src.GetHeight(), 32, GL_RGBA);
	else
		raw_.create(src.GetWidth(), src.GetHeight(), 24, GL_RGB);

	unsigned char *ptr = raw_.data();
	unsigned char *end = raw_.end();

	if (!src_a)
		memcpy(ptr, src_rgb, end - ptr);
	else
	{
		while (ptr != end)
		{
			*ptr++ = *src_rgb++;
			*ptr++ = *src_rgb++;
			*ptr++ = *src_rgb++;
			*ptr++ = *src_a++;
		}
	}

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
	return wx_image.LoadFile(stream, wxBITMAP_TYPE_ANY) && convert_from(wx_image);
}

void image::load_from_raw(const unsigned char *data,
	int width, int height, bool with_alpha)
{
	if (with_alpha)
		raw_.create(width, height, 32, GL_RGBA);
	else
		raw_.create(width, height, 24, GL_RGB);

	unsigned char *ptr = raw_.data();

	memcpy(ptr, data, raw_.end() - data);
}

} /* namespace cart */
