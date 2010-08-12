#ifndef RAW_IMAGE_H
#define RAW_IMAGE_H

class raw_image
{
private:
	int width_;
	int height_;
	int bpp_;
	int tag_;
	unsigned char *data_;

	void init()
	{
		width_ = 0;
		height_ = 0;
		bpp_ = 0;
		tag_ = 0;
		data_ = 0;
	}

public:
	raw_image()
	{
		init();
	}

	raw_image(int width, int height, int bpp, int tag = 0)
	{
		init();
		create(width, height, bpp, tag);
	}

	~raw_image()
	{
		clear();
	}

	void create(int width, int height, int bpp, int tag = 0)
	{
		clear();

		width_ = width;
		height_ = height;
		bpp_ = bpp;
		tag_ = tag;
		data_ = new unsigned char[ width * height * (bpp / 8) ];
	}

	void clear(bool with_init = true)
	{
		delete[] data_;

		if (with_init)
			init();
		else
			data_ = 0;
	}

	inline int width() const
		{ return width_; }

	inline int height() const
		{ return height_; }

	inline int bpp() const
		{ return bpp_; }

	inline void set_tag(int tag)
		{ tag_ = tag; }

	inline int tag() const
		{ return tag_; }

	inline unsigned char* data()
		{ return data_; }

	inline const unsigned char * data() const
		{ return data_; }

	inline unsigned char* end()
		{ return data_ + width_ * height_ * (bpp_ / 8); }

	inline const unsigned char* end() const
		{ return data_ + width_ * height_ * (bpp_ / 8); }
};

#endif
