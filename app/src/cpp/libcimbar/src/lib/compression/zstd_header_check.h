/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "zstd/zstd.h"
#include <string>

namespace cimbar {

class zstd_header_check
{
public:
	static std::string get_filename(const unsigned char* data, size_t len)
	{
		return get_metadata(data, len, 1);
	}

	static std::string get_sha256_hex(const unsigned char* data, size_t len)
	{
		return get_metadata(data, len, 2);
	}

protected:
	static std::string get_metadata(const unsigned char* data, size_t len, unsigned char expected_type)
	{
		size_t offset = 0;
		while (offset + 8 <= len)
		{
			const unsigned char* frame = data + offset;
			size_t remaining = len - offset;
			if (!ZSTD_isSkippableFrame(frame, remaining))
				break;

			size_t payload = static_cast<size_t>(frame[4]) |
			                 (static_cast<size_t>(frame[5]) << 8) |
			                 (static_cast<size_t>(frame[6]) << 16) |
			                 (static_cast<size_t>(frame[7]) << 24);
			size_t total = payload + 8;
			if (payload == 0 || total > remaining)
				break;

			if (frame[8] == expected_type)
				return std::string(reinterpret_cast<const char*>(frame + 9), payload - 1);

			offset += total;
		}
		return "";
	}
};

}
