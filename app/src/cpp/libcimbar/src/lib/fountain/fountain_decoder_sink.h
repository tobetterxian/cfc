/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "fountain_decoder_stream.h"
#include "FountainMetadata.h"
#include "compression/zstd_decompressor.h"
#include "compression/zstd_header_check.h"
#include "serialize/format.h"
#include "util/File.h"
#include "PicoSHA2/picosha2.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

inline std::string sha256_file_hex(const std::string& file_path)
{
	std::ifstream f(file_path, std::ios::binary);
	if (!f.good())
		return "";
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(f, hash.begin(), hash.end());
	return picosha2::bytes_to_hex_string(hash);
}

template <typename OUTSTREAM>
std::function<std::string(const std::string&, const std::vector<uint8_t>&)> write_on_store(std::string data_dir, bool log_writes=false)
{
	return [data_dir, log_writes](const std::string& filename, const std::vector<uint8_t>& data) -> std::string
	{
		std::string file_path = fmt::format("{}/{}", data_dir, filename);
		OUTSTREAM f(file_path, std::ios::binary);
		if (!f.good())
			return "";
		f.write((char*)data.data(), data.size());
		if (!f.good())
		{
			std::filesystem::remove(file_path);
			return "";
		}
		if (log_writes)
			printf("%s\n", file_path.c_str());
		return filename;
	};
}

template <typename OUTSTREAM>
std::function<std::string(const std::string&, const std::vector<uint8_t>&)> decompress_on_store(std::string data_dir, bool log_writes=false)
{
	return [data_dir, log_writes](const std::string& fallback_name, const std::vector<uint8_t>& data) -> std::string
	{
		std::string filename = cimbar::zstd_header_check::get_filename(data.data(), data.size());
		std::string expected_sha256 = cimbar::zstd_header_check::get_sha256_hex(data.data(), data.size());
		if (!filename.empty())
			filename = File::basename(filename);
		if (filename.empty())
			filename = fallback_name;

		std::string file_path = fmt::format("{}/{}", data_dir, filename);
		bool write_ok = false;
		{
			cimbar::zstd_decompressor<OUTSTREAM> f(file_path, std::ios::binary);
			write_ok = f.good() && f.write((char*)data.data(), data.size());
		}
		if (!write_ok)
		{
			std::filesystem::remove(file_path);
			return "";
		}
		if (!expected_sha256.empty())
		{
			std::string actual_sha256 = sha256_file_hex(file_path);
			if (actual_sha256 != expected_sha256)
			{
				std::filesystem::remove(file_path);
				return "";
			}
		}
		if (log_writes)
			printf("%s\n", file_path.c_str());
		return filename;
	};
}


class fountain_decoder_sink
{
public:
	fountain_decoder_sink(unsigned chunk_size, const std::function<std::string(const std::string&, const std::vector<uint8_t>&)>& on_store=nullptr)
		: _chunkSize(chunk_size)
		, _onStore(on_store)
	{
	}

	bool good() const
	{
		return true;
	}

	unsigned chunk_size() const
	{
		return _chunkSize;
	}

	std::string get_filename(const FountainMetadata& md) const
	{
		return fmt::format("{}.{}", md.encode_id(), md.file_size());
	}

	bool store(const FountainMetadata& md, fountain_decoder_stream& s)
	{
		if (_onStore)
		{
			auto res = s.recover();
			if (!res)
				return false;
			std::string filename = _onStore(get_filename(md), *res);
			if (filename.empty())
				return false;
			mark_done(md, filename);
		}
		return true;
	}

	void mark_done(const FountainMetadata& md, const std::string& filename)
	{
		_done[md.id()] = filename;
		auto it = _streams.find(stream_slot(md));
		if (it != _streams.end())
			_streams.erase(it);
	}

	unsigned num_streams() const
	{
		return _streams.size();
	}

	unsigned num_done() const
	{
		return _done.size();
	}

	std::vector<std::string> get_done() const
	{
		std::vector<std::string> done;
		for (auto&& [id, filename] : _done)
			done.push_back( filename );
		return done;
	}

	std::vector<double> get_progress() const
	{
		std::vector<double> progress;
		for (auto&& [slot, s] : _streams)
		{
			unsigned br = s.blocks_required();
			if (br)
				progress.push_back( s.progress() * 1.0 / s.blocks_required() );
		}
		return progress;
	}

	bool is_done(uint32_t id) const
	{
		return _done.find(id) != _done.end();
	}

	int64_t decode_frame(const char* data, unsigned size)
	{
		if (size < FountainMetadata::md_size)
			return -10;

		FountainMetadata md(data, size);
		if (!md.file_size())
		{
			/*std::cout << fmt::format("decode frame {} ... {},{},{},{}",
									 md.file_size(), (unsigned)data[0], (unsigned)data[1], (unsigned)data[2], (unsigned)data[3]) << std::endl;*/
			return -11;
		}

		// check if already done
		if (is_done(md.id()))
			return -1;

		// find or create
		auto p = _streams.try_emplace(stream_slot(md), md.file_size(), _chunkSize);
		fountain_decoder_stream& s = p.first->second;
		if (s.data_size() != md.file_size())
			return -12;

		bool finished = s.write(data, size);
		if (!finished)
			return 0;

		// when you provide a write callback,
		// store() will call mark_done() afterwards
		// -- and the assembled file will be dropped from RAM.
		// but if no callback is provided, you can do something else.
		store(md, s);
		return (int64_t)0 + md.id();
	}

	bool write(const char* data, unsigned length)
	{
		return decode_frame(data, length) > 0;
	}

	fountain_decoder_sink& operator<<(const std::string& buffer)
	{
		write(buffer.data(), buffer.size());
		return *this;
	}

	bool recover(uint32_t id, unsigned char* data, unsigned size)
	{
		// iff you don't provide a write callback
		// this finalizes the write.
		// after the data is copied to `data`,
		// the stream will be dropped from RAM (`mark_done()`)
		FountainMetadata md(id);
		auto p = _streams.find(stream_slot(md));
		if (p == _streams.end())
			return false;

		fountain_decoder_stream& s = p->second;
		bool res = s.recover(data, size);
		mark_done(md, get_filename(md));
		return res;
	}

protected:
	// streams is limited to at most 8 decoders at a time. Currently, we just use the lower bits of the encode_id.
	uint8_t stream_slot(const FountainMetadata& md) const
	{
		return md.encode_id() & 0x7;
	}

protected:
	unsigned _chunkSize;
	std::function<std::string(const std::string&, const std::vector<uint8_t>&)> _onStore;

	// maybe instead of unordered_map+set, something where we can "age out" old streams?
	// e.g. most recent 16/8, or something?
	// question is what happens to _done/_streams when we wrap for continuous data streaming...
	std::unordered_map<uint8_t, fountain_decoder_stream> _streams;
	// track the uint32_t combo of (encode_id,size) to avoid redundant work
	std::unordered_map<uint32_t, std::string> _done;
	bool _logWrites;
};
