#pragma once

#include "compression/zstd_decompressor.h"
#include "encoder/Decoder.h"
#include "extractor/Anchor.h"
#include "extractor/Deskewer.h"
#include "extractor/Extractor.h"
#include "extractor/Scanner.h"
#include "fountain/concurrent_fountain_decoder_sink.h"

#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>
#include <array>
#include <deque>
#include <fstream>
#include <mutex>

class MultiThreadedDecoder
{
public:
	MultiThreadedDecoder(std::string data_path, int mode_val);

	inline static clock_t count = 0;
	inline static clock_t bytes = 0;
	inline static clock_t perfect = 0;
	inline static clock_t decoded = 0;
	inline static clock_t decodeTicks = 0;
	inline static clock_t scanned = 0;
	inline static clock_t scanTicks = 0;
	inline static clock_t extractTicks = 0;

	bool add(cv::Mat mat);

	void stop();

	int mode() const;
	bool set_mode(int mode_val);
	int detected_mode() const;

	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;
	std::vector<std::string> get_done() const;
	std::vector<double> get_progress() const;

protected:
	int do_extract(const cv::Mat& mat, cv::Mat& img);
	bool try_extract_roi(const cv::Mat& luma, const cv::Mat& color, cv::Mat& img);
	void save(const cv::Mat& img);
	void update_tracking(const Corners& corners, const cv::Size& frameSize);
	void clear_tracking();
	void remember_extract(const cv::Mat& img);
	cv::Mat fused_extract() const;
	unsigned next_autodetect_mode();
	void update_autodetect(unsigned modeVal, unsigned decodeRes);

	static cv::Rect expand_roi(const cv::Rect& roi, const cv::Size& frameSize, int padX, int padY);

	static unsigned fountain_chunk_size(int mode_val);

protected:
	int _modeVal;
	int _detectedMode;
	int _autodetectLockedMode;
	unsigned _autodetectSuccessStreak;
	unsigned _autodetectFailureStreak;

	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	concurrent_fountain_decoder_sink _writer;
	std::string _dataPath;
	unsigned _successCondition;
	mutable std::mutex _trackingMutex;
	bool _hasTracking;
	cv::Rect _trackingRoi;
	mutable std::mutex _modeMutex;
	mutable std::mutex _fusionMutex;
	std::deque<cv::Mat> _fusionFrames;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path, int mode_val)
	: _modeVal(mode_val)
	, _detectedMode(0)
	, _autodetectLockedMode(0)
	, _autodetectSuccessStreak(0)
	, _autodetectFailureStreak(0)
	, _dec(cimbar::Config::ecc_bytes(), cimbar::Config::color_bits())
	, _numThreads(std::max<int>(((int)std::thread::hardware_concurrency()/2), 1))
	, _pool(_numThreads, 1)
	, _writer(fountain_chunk_size(mode_val), decompress_on_store<std::ofstream>(data_path, true))
	, _dataPath(data_path)
	, _successCondition(cimbar::Config::temp_conf(mode_val).capacity() * .7)
	, _hasTracking(false)
{
	FountainInit::init();
	_pool.start();
}

inline int MultiThreadedDecoder::do_extract(const cv::Mat& mat, cv::Mat& img)
{
	cv::Mat luma;
	if (mat.channels() >= 3)
		cv::cvtColor(mat, luma, cv::COLOR_RGBA2GRAY);
	else
		luma = mat;

	if (try_extract_roi(luma, mat, img))
		return Extractor::SUCCESS;

	clock_t begin = clock();
	Scanner scanner(luma);
	std::vector<Anchor> anchors = scanner.scan();
	++scanned;
	scanTicks += (clock() - begin);

	//if (anchors.size() >= 3) save(mat);

	if (anchors.size() < 4)
	{
		clear_tracking();
		return Extractor::FAILURE;
	}

	begin = clock();
	Corners corners(anchors);
	Deskewer de;
	img = de.deskew(mat, corners);
	extractTicks += (clock() - begin);
	update_tracking(corners, mat.size());

	return Extractor::SUCCESS;
}

inline bool MultiThreadedDecoder::try_extract_roi(const cv::Mat& luma, const cv::Mat& color, cv::Mat& img)
{
	cv::Rect roi;
	{
		std::lock_guard<std::mutex> lock(_trackingMutex);
		if (!_hasTracking)
			return false;
		roi = expand_roi(_trackingRoi, color.size(), std::max(12, _trackingRoi.width / 6), std::max(12, _trackingRoi.height / 6));
	}

	clock_t begin = clock();
	Scanner scanner(luma(roi));
	std::vector<Anchor> anchors = scanner.scan();
	++scanned;
	scanTicks += (clock() - begin);
	if (anchors.size() < 4)
		return false;

	std::vector<Anchor> shifted;
	shifted.reserve(anchors.size());
	for (const Anchor& anchor : anchors)
		shifted.emplace_back(anchor.x() + roi.x, anchor.xmax() + roi.x, anchor.y() + roi.y, anchor.ymax() + roi.y);

	begin = clock();
	Corners corners(shifted);
	Deskewer de;
	img = de.deskew(color, corners);
	extractTicks += (clock() - begin);
	update_tracking(corners, color.size());
	return true;
}

inline void MultiThreadedDecoder::update_tracking(const Corners& corners, const cv::Size& frameSize)
{
	cv::Rect roi = cv::boundingRect(corners.all());
	roi = expand_roi(roi, frameSize, std::max(12, roi.width / 8), std::max(12, roi.height / 8));

	std::lock_guard<std::mutex> lock(_trackingMutex);
	_trackingRoi = roi;
	_hasTracking = true;
}

inline void MultiThreadedDecoder::clear_tracking()
{
	std::lock_guard<std::mutex> lock(_trackingMutex);
	_hasTracking = false;
}

inline void MultiThreadedDecoder::remember_extract(const cv::Mat& img)
{
	std::lock_guard<std::mutex> lock(_fusionMutex);
	if (!_fusionFrames.empty())
	{
		const cv::Mat& last = _fusionFrames.back();
		if (last.size() != img.size() or last.type() != img.type())
			_fusionFrames.clear();
	}

	_fusionFrames.push_back(img.clone());
	constexpr size_t kMaxFusionFrames = 3;
	while (_fusionFrames.size() > kMaxFusionFrames)
		_fusionFrames.pop_front();
}

inline cv::Mat MultiThreadedDecoder::fused_extract() const
{
	std::lock_guard<std::mutex> lock(_fusionMutex);
	if (_fusionFrames.size() < 2)
		return {};

	int floatType = CV_MAKETYPE(CV_32F, _fusionFrames.front().channels());
	cv::Mat accum;
	_fusionFrames.front().convertTo(accum, floatType);
	for (size_t i = 1; i < _fusionFrames.size(); ++i)
	{
		cv::Mat temp;
		_fusionFrames[i].convertTo(temp, floatType);
		accum += temp;
	}
	accum /= static_cast<double>(_fusionFrames.size());

	cv::Mat fused;
	accum.convertTo(fused, _fusionFrames.front().type());
	return fused;
}

inline cv::Rect MultiThreadedDecoder::expand_roi(const cv::Rect& roi, const cv::Size& frameSize, int padX, int padY)
{
	int x = std::max(0, roi.x - padX);
	int y = std::max(0, roi.y - padY);
	int right = std::min(frameSize.width, roi.x + roi.width + padX);
	int bottom = std::min(frameSize.height, roi.y + roi.height + padY);
	return cv::Rect(x, y, std::max(1, right - x), std::max(1, bottom - y));
}

inline unsigned MultiThreadedDecoder::next_autodetect_mode()
{
	std::lock_guard<std::mutex> lock(_modeMutex);
	if (_autodetectLockedMode != 0)
		return _autodetectLockedMode;

	static constexpr std::array<unsigned, 4> kCommonModes = {68, 67, 66, 4};
	static constexpr std::array<unsigned, 6> kExtendedModes = {68, 67, 66, 4, 69, 70};
	const bool probeExperimentalModes = count > (kCommonModes.size() * 3);
	const unsigned probeIndex = static_cast<unsigned>(std::max<clock_t>(count - 1, 0));

	if (probeExperimentalModes)
		return kExtendedModes[probeIndex % kExtendedModes.size()];
	return kCommonModes[probeIndex % kCommonModes.size()];
}

inline void MultiThreadedDecoder::update_autodetect(unsigned modeVal, unsigned decodeRes)
{
	std::lock_guard<std::mutex> lock(_modeMutex);
	if (_modeVal != 0)
		return;

	if (decodeRes)
	{
		_detectedMode = modeVal;
		if (_autodetectLockedMode == modeVal)
			++_autodetectSuccessStreak;
		else
		{
			_autodetectLockedMode = modeVal;
			_autodetectSuccessStreak = 1;
		}
		_autodetectFailureStreak = 0;
		return;
	}

	if (_autodetectLockedMode == modeVal)
	{
		++_autodetectFailureStreak;
		if (_autodetectFailureStreak >= 5)
		{
			_autodetectLockedMode = 0;
			_autodetectSuccessStreak = 0;
			_autodetectFailureStreak = 0;
			_detectedMode = 0;
		}
	}
}

inline bool MultiThreadedDecoder::add(cv::Mat mat)
{
	++count;
	unsigned modeVal = 0;
	{
		std::lock_guard<std::mutex> lock(_modeMutex);
		modeVal = _modeVal;
	}
	if (modeVal == 0)
		modeVal = next_autodetect_mode();
	return _pool.try_execute( [&, mat, modeVal] () {
		cimbar::Config::update(modeVal);
		cv::Mat img;
		int res = do_extract(mat, img);
		if (res == Extractor::FAILURE)
			return;
		remember_extract(img);

		// if extracted image is small, we'll need to run some filters on it
		clock_t begin = clock();
		bool should_preprocess = (res == Extractor::NEEDS_SHARPEN);
		int color_correction = modeVal==4? 1 : 2;
		unsigned decodeRes = _dec.decode_fountain(img, _writer, should_preprocess, color_correction);
		if (!decodeRes)
		{
			cv::Mat fused = fused_extract();
			if (!fused.empty())
				decodeRes = _dec.decode_fountain(fused, _writer, should_preprocess, color_correction);
		}
		bytes += decodeRes;
		++decoded;
		decodeTicks += clock() - begin;

		update_autodetect(modeVal, decodeRes);

		if (decodeRes >= _successCondition)
			++perfect;
	} );
}

inline void MultiThreadedDecoder::save(const cv::Mat& mat)
{
	std::stringstream fname;
	fname << _dataPath << "/scan" << (scanned-1) << ".png";
	cv::Mat bgr;
	cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
	cv::imwrite(fname.str(), bgr);
}

inline void MultiThreadedDecoder::stop()
{
	_pool.stop();
}

unsigned MultiThreadedDecoder::fountain_chunk_size(int mode_val)
{
	return cimbar::Config::temp_conf(mode_val).fountain_chunk_size();
}

inline int MultiThreadedDecoder::mode() const
{
	std::lock_guard<std::mutex> lock(_modeMutex);
	return _modeVal;
}

inline bool MultiThreadedDecoder::set_mode(int mode_val)
{
	std::lock_guard<std::mutex> lock(_modeMutex);
	if (_modeVal == mode_val)
		return true;

	if (mode_val != 0 and _writer.chunk_size() != fountain_chunk_size(mode_val))
		return false; // if so, we need to reset to change it

	// reset detectedMode iff we're switching back to autodetect
	if (mode_val == 0)
	{
		_detectedMode = 0;
		_autodetectLockedMode = 0;
		_autodetectSuccessStreak = 0;
		_autodetectFailureStreak = 0;
	}

	_modeVal = mode_val;
	return true;
}

inline int MultiThreadedDecoder::detected_mode() const
{
	std::lock_guard<std::mutex> lock(_modeMutex);
	return _detectedMode;
}

inline unsigned MultiThreadedDecoder::num_threads() const
{
	return _numThreads;
}

inline unsigned MultiThreadedDecoder::backlog() const
{
	return _pool.queued();
}

inline unsigned MultiThreadedDecoder::files_in_flight() const
{
	return _writer.num_streams();
}

inline unsigned MultiThreadedDecoder::files_decoded() const
{
	return _writer.num_done();
}

inline std::vector<std::string> MultiThreadedDecoder::get_done() const
{
	return _writer.get_done();
}

inline std::vector<double> MultiThreadedDecoder::get_progress() const
{
	return _writer.get_progress();
}
