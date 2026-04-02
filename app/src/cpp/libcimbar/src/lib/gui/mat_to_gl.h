/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "gl_headers.h"
#include <opencv2/opencv.hpp>

namespace cimbar {
namespace mat_to_gl {

	inline void load_gl_texture(GLuint texid, const cv::Mat& mat)
	{
		const cv::Mat* upload = &mat;
		cv::Mat converted;
		glBindTexture(GL_TEXTURE_2D, texid);

		// not sure whether we need this or not
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLenum format = GL_RGB;
		switch (mat.channels())
		{
		case 1:
			cv::cvtColor(mat, converted, cv::COLOR_GRAY2RGB);
			upload = &converted;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			;
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, format, upload->cols, upload->rows, 0, format, GL_UNSIGNED_BYTE, upload->data);
		glBindTexture(GL_TEXTURE_2D, 0); // clean up
	}

}
}
