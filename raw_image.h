#ifndef RAW_IMAGE_H
#define RAW_IMAGE_H

class raw_image
{
private:
	int width_;
	int height_;
	int bpp_;
	int type_;
	unsigned char *data_;

	void init()
	{
		width_ = 0;
		height_ = 0;
		bpp_ = 0;
		type_ = 0;
		data_ = 0;
	}

public:
	raw_image()
	{
		init();
	}

	raw_image(int width, int height, int bpp, int type = 0)
	{
		init();
		create(width, height, bpp, type);
	}

	~raw_image()
	{
		delete[] data_;
	}

	void create(int width, int height, int bpp, int type = 0)
	{
		delete[] data_;

		width_ = width;
		height_ = height;
		bpp_ = bpp;
		type_ = type;
		data_ = new unsigned char[ width * height * (bpp / 8) ];
	}

	inline int width() const
		{ return width_; }

	inline int height() const
		{ return height_; }

	inline int bpp() const
		{ return bpp_; }

	inline int type() const
		{ return type_; }

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
