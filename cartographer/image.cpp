#include "image.h"

#include <cstring>
#include <wx/mstream.h> /* wxMemoryInputStream */

namespace cartographer
{

/* Число кратное 2, большее или равное a */
inline int __p2(int a)
{
	int res = 1;
	while (res < a)
		res <<= 1;
	return res;
}

void image::create(int width, int height)
{
	width_ = width;
	height_ = height;

	raw_.create( __p2(width), __p2(height), 32);

	unsigned char *ptr = raw_.data();
	unsigned char *end = raw_.end();

	/* Очищаем */
	std::memset(ptr, 0, end - ptr);

	set_state(ready);
}

bool image::convert_from(const wxImage &src)
{
	if (!src.IsOk())
		return false;

	unsigned char *src_rgb = src.GetData();
	unsigned char *src_a = src.GetAlpha();

	width_ = src.GetWidth();
	height_ = src.GetHeight();

    /* Размеры OpenGL-текстур должны быть кратны 2 */
	int raw_width = __p2(width_);
	int raw_height = __p2(height_);
	int dw = raw_width - width_;

	/* Т.к., возможно, понадобится дополнять текстуру прозрачными точками,
		делаем RGBA-изображение вне зависимости от его исходного bpp */
	raw_.create(raw_width, raw_height, 32);

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
			unsigned char *line_end = ptr + dw * 4;
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
	raw_.create(width, height, with_alpha ? 32 : 24);

	unsigned char *ptr = raw_.data();

	memcpy(ptr, data, raw_.end() - data);

	set_state(ready);
}

GLuint image::load_as_gl_texture()
{
	GLuint id;

	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); /* GL_CLAMP_TO_EDGE */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); /* GL_CLAMP_TO_EDGE */
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raw_.width(), raw_.height(),
		0, raw_.bpp() == 32 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, raw_.data());

	return glGetError() == GL_NO_ERROR ? id : 0;
}

GLuint image::convert_to_gl_texture()
{
	if (texture_id_ != 0)
		glDeleteTextures(1, &texture_id_);

	texture_id_ = load_as_gl_texture();

	if (texture_id_)
		raw_.clear(false);

	return texture_id_;
}

} /* namespace cartographer */
