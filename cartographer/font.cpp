#include "font.h"

#include <wx/string.h>
#include <wx/dcmemory.h>
#include <wx/bitmap.h>
#include <wx/image.h>

namespace cartographer
{

size font::draw(const std::wstring &str, const point &pos,
	const ratio &center, const ratio &scale)
{
	size sz;

	/* Готовим символы, для которых ещё не были созданы текстуры */
	prepare_chars(str);

	const wchar_t *ptr = str.c_str();
	wchar_t ch;

	/* Вычисляем размер строки */
	while ((ch = *ptr++) != 0)
	{
		char_info &ci = chars_[ch];
		image::ptr image_ptr = images_[ci.image_id];

		/* Символы толще своих размеров на величину рамки,
			при выводе символы накладываются друг на друга как раз
			на величину рамки */
		sz.width -= 2.0 * ci.border.width * scale.kx;
		double ch_w = ci.width * scale.kx;
		double ch_h = ci.height * scale.ky;

		sz.width += ch_w;
		if (ch_h > sz.height)
			sz.height = ch_h;
	}

	ptr = str.c_str();
	double x = pos.x - sz.width * center.kx;
	double y = pos.y - sz.height * center.ky;

	/* Выводим текст посимвольно */
	while ((ch = *ptr++) != 0)
	{
		char_info &ci = chars_[ch];
		image::ptr image_ptr = images_[ci.image_id];

		GLuint texture_id = image_ptr->texture_id();
		if (texture_id == 0)
			texture_id = image_ptr->convert_to_gl_texture();

		double raw_width = image_ptr->raw().width();
		double raw_height = image_ptr->raw().height();

		double ch_w = ci.width * scale.kx;
		double ch_h = ci.height * scale.ky;

		double tx = ci.pos / raw_width;
		double ty = 0.0;
		double tw = ci.width / raw_width;
		double th = ci.height / raw_height;

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glBegin(GL_QUADS);
			glTexCoord2d(tx,      ty);      glVertex3d(x,        y,        0);
			glTexCoord2d(tx + tw, ty);      glVertex3d(x + ch_w, y,        0);
			glTexCoord2d(tx + tw, ty + th); glVertex3d(x + ch_w, y + ch_h, 0);
			glTexCoord2d(tx,      ty + th); glVertex3d(x,        y + ch_h, 0);
		glEnd();

		x += (ci.width - 2.0 * ci.border.width) * scale.kx;
	}

#if 0
	y += 5 * sz.height;
	for (images_list::iterator iter = images_.begin();
		iter != images_.end(); ++iter)
	{
		image::ptr image_ptr = iter->second;

		GLuint texture_id = image_ptr->texture_id();
		if (texture_id == 0)
			texture_id = image_ptr->convert_to_gl_texture();

		double w = image_ptr->raw().width();
		double h = image_ptr->raw().height();

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0); glVertex3d(x,     y,     0);
			glTexCoord2d(1.0, 0.0); glVertex3d(x + w, y,     0);
			glTexCoord2d(1.0, 1.0); glVertex3d(x + w, y + h, 0);
			glTexCoord2d(0.0, 1.0); glVertex3d(x,     y + h, 0);
		glEnd();

		y += h;
	}
#endif

	return sz;
}

std::wstring font::chars_from_ranges(const std::wstring &ranges)
{
	std::wstring chars;

	const wchar_t *ptr = ranges.c_str();
	wchar_t prev = 0;
	wchar_t ch;

	while ((ch = *ptr++) != 0)
	{
		if (ch != L'-' || prev == 0 || *ptr == 0)
		{
			chars.push_back(ch);
			prev = ch;
		}
		else
		{
			ch = *ptr++;
			++prev;
			while (prev <= ch)
				chars.push_back(prev++);

			prev = 0;
		}
	}

	return chars;
}

void font::prepare_chars(const std::wstring &chars)
{
	const wchar_t *ptr = chars.c_str();
	wchar_t ch;
	std::wstring not_prepared_chars;

	/* Готовим символы, для которых ещё не были созданы текстуры */
	while ((ch = *ptr++) != 0)
	{
		chars_list::iterator iter = chars_.find(ch);
		if (iter == chars_.end())
			not_prepared_chars.push_back(ch);
	}

	if (!not_prepared_chars.empty())
		prepare_chars__(not_prepared_chars);
}

void font::prepare_chars__(const std::wstring &chars)
{
	static const int border_v = 1;
	static const int border_h = 1;
	static const int border_2v = 2 * border_v;
	static const int border_2h = 2 * border_h;
	static const int box_v = border_2v + 1;
	static const int box_h = border_2h + 1;
	static const double box[box_v][box_h] =
	{
		{0.6, 1.0, 0.6},
		{1.0, 1.0, 1.0},
		{0.6, 1.0, 0.6}
	};

	GLint max_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);


	/*
		Готовим буфер
	*/
	wxBitmap bmp(1, 1);
	wxMemoryDC dc(bmp);
	dc.SetFont(font_);

	wxString str;
	int width = 0;
	int height = 0;

	/* Рассчитываем размер строки. GetTextExtent() для всей строки выдаёт
		не тот результат, что в сумме для каждого символа отдельно.
		Видимо, сказывается кернинг */
	{
		const wchar_t *ptr = chars.c_str();
		wchar_t ch;

		while ((ch = *ptr++) != 0)
		{
			wxCoord w, h;

			str = ch;
			dc.GetTextExtent(str, &w, &h);

			/* Добавляем место для рамки */
			w += border_2h;
			h += border_2v;

			width += w;
			if (h > height)
				height = h;
		}
	}

	if (width > max_size)
		width = max_size;

	dc.SelectObject(wxNullBitmap);
	bmp = wxBitmap(width, height);
	dc.SelectObject(bmp);

	dc.SetBackground(*wxBLACK_BRUSH);
	dc.Clear();


	/*
		Выводим текст посимвольно
	*/
	int image_id = ++images_index_;

	{
		dc.SetTextForeground(*wxWHITE);

		int pos = 0;
		const wchar_t *ptr = chars.c_str();
		wchar_t ch;

		while ((ch = *ptr++) != 0)
		{
			int w, h;

			str = ch;
			dc.GetTextExtent(str, &w, &h);

			w += border_2h;
			h += border_2v;

			/* Проверяем выход за границы текстуры */
			if (pos + w > max_size)
				break;

			dc.DrawText(str, pos + border_h, border_v);

			char_info ci(image_id, pos, w, h, size(border_h, border_v));
			pos += w;

			chars_[ch] = ci;
		}

		/* Если вся строка не вместилась в текстуру,
			переносим остаток строки в следующую */
		if (ch != 0)
			prepare_chars__( std::wstring(ptr) );
	}

	image tmp_image(on_image_delete_);
	if (!tmp_image.convert_from( bmp.ConvertToImage() ))
		return;


	/*
		Создаём "красивые" буквы
	*/
	image::ptr image_ptr( new image(on_image_delete_) );
	image_ptr->create(width, height);
	{
		size_t src_line_sz = tmp_image.raw().width() * 4;
		unsigned char *src_begin = tmp_image.raw().data()
			+ border_v * src_line_sz + border_h * 4;
		unsigned char *src_end = tmp_image.raw().end()
			- border_v * src_line_sz - border_h * 4;
		unsigned char *src_ptr = src_begin;

		unsigned char *dest_ptr = image_ptr->raw().data() + 3; /* Толька альфа */
		size_t line_sz = image_ptr->raw().width() * 4;

		/* Чёрная рамка вокруг букв */
		while (src_ptr != src_end)
		{
			unsigned char ch = *src_ptr;
			for (int i = 0; i < box_v; ++i)
			{
				unsigned char *line_ptr = dest_ptr + i * line_sz;

				for (int j = 0; j < box_h; ++j)
				{
					unsigned char *point_ptr = line_ptr + j * 4;

					double k = box[i][j] * ch / 255.0;
					unsigned char a = (unsigned char)(255.0 * k + 0.5);
					if (*point_ptr < a)
						*point_ptr = a;
				}
			}
			src_ptr += 4;
			dest_ptr += 4;
		}

		src_ptr = src_begin;
		dest_ptr = image_ptr->raw().data() + border_v * line_sz + border_h * 4;

		/* Сами буквы */
		while (src_ptr != src_end)
		{
			unsigned char ch = *src_ptr;
			*dest_ptr++ = ch;
			*dest_ptr++ = ch;
			*dest_ptr++ = ch;
			++dest_ptr;
			src_ptr += 4;
		}
	}

	images_[image_id] = image_ptr;
}

} /* namespace cartographer */
