#include "MultiThreadedDecoder.h"
#include "cimb_translator/CimbDecoder.h"
#include "cimb_translator/CimbReader.h"
#include "encoder/Decoder.h"
#include "extractor/Scanner.h"
#include "serialize/format.h"

#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>

#define TAG "CameraFileCopyCPP"

using namespace std;
using namespace cv;

namespace {
	std::shared_ptr<MultiThreadedDecoder> _proc;
	std::mutex _mutex; // for _proc
	std::set<std::string> _completed;

	unsigned _calls = 0;
	int _transferStatus = 0;
	clock_t _frameDecodeSnapshot = 0;
	clock_t _frameSuccessSnapshot = 0;
	clock_t _rateBytesSnapshot = 0;
	std::chrono::steady_clock::time_point _rateSnapshotTime = std::chrono::steady_clock::time_point{};
	std::chrono::steady_clock::time_point _transferStartTime = std::chrono::steady_clock::time_point{};
	double _liveBytesPerSec = 0.0;
	double _averageBytesPerSec = 0.0;

	unsigned millis(unsigned num, unsigned denom)
	{
		if (!denom)
			denom = 1;
		return (num / denom) * 1000 / CLOCKS_PER_SEC;
	}

	unsigned percent(unsigned num, unsigned denom)
	{
		if (!denom)
			denom = 1;
		return (num * 100) / denom;
	}

	void reset_session_state()
	{
		_completed.clear();
		_calls = 0;
		_transferStatus = 0;
		_frameDecodeSnapshot = 0;
		_frameSuccessSnapshot = 0;
		_rateBytesSnapshot = 0;
		_rateSnapshotTime = std::chrono::steady_clock::time_point{};
		_transferStartTime = std::chrono::steady_clock::time_point{};
		_liveBytesPerSec = 0.0;
		_averageBytesPerSec = 0.0;
		MultiThreadedDecoder::count = 0;
		MultiThreadedDecoder::bytes = 0;
		MultiThreadedDecoder::perfect = 0;
		MultiThreadedDecoder::decoded = 0;
		MultiThreadedDecoder::decodeTicks = 0;
		MultiThreadedDecoder::scanned = 0;
		MultiThreadedDecoder::scanTicks = 0;
		MultiThreadedDecoder::extractTicks = 0;
	}

	std::string telemetry_snapshot(const std::shared_ptr<MultiThreadedDecoder>& proc)
	{
		unsigned backlog = 0;
		int mode = 0;
		int detectedMode = 0;
		unsigned filesInFlight = 0;
		unsigned filesDecoded = 0;

		if (proc)
		{
			backlog = proc->backlog();
			mode = proc->mode();
			detectedMode = proc->detected_mode();
			filesInFlight = proc->files_in_flight();
			filesDecoded = proc->files_decoded();
		}

		std::stringstream stream;
		stream << _calls << '|'
		       << MultiThreadedDecoder::scanned << '|'
		       << MultiThreadedDecoder::decoded << '|'
		       << MultiThreadedDecoder::perfect << '|'
		       << MultiThreadedDecoder::bytes << '|'
		       << millis(MultiThreadedDecoder::scanTicks, MultiThreadedDecoder::scanned) << '|'
		       << millis(MultiThreadedDecoder::extractTicks, MultiThreadedDecoder::decoded) << '|'
		       << millis(MultiThreadedDecoder::decodeTicks, MultiThreadedDecoder::decoded) << '|'
		       << backlog << '|'
		       << mode << '|'
		       << detectedMode << '|'
		       << filesInFlight << '|'
		       << filesDecoded;
		return stream.str();
	}

	void drawGuidance(cv::Mat& mat, int in_progress)
	{
		int minsz = std::min(mat.cols, mat.rows);
		int guideWidth = minsz >> 7;
		int outlineWidth = guideWidth + (minsz >> 8);
		int guideLength = guideWidth << 3;
		int guideOffset = minsz >> 5;
		int outlineOffset = (outlineWidth - guideWidth) >> 1;

		cv::Scalar color = cv::Scalar(255,255,255);
		if (in_progress == 1)
			color = cv::Scalar(255,244,94); // 0,191,255
		else if (in_progress == 2)
			color = cv::Scalar(0,255,0);
		cv::Scalar outline = cv::Scalar(0,0,0);

		int xextra = 0;
		if (mat.cols > mat.rows)
			xextra = (mat.cols - mat.rows) >> 1;
		int yextra = 0;
		if (mat.rows > mat.cols)
			yextra = (mat.rows - mat.cols) >> 1;

		int lx = guideOffset + xextra;
		int ty = guideOffset + yextra;
		int outlinex = lx - outlineOffset;
		int outliney = ty - outlineOffset;
		cv::line(mat, cv::Point(lx, outliney), cv::Point(lx + guideLength, outliney), outline, outlineWidth);
		cv::line(mat, cv::Point(outlinex, guideOffset), cv::Point(outlinex, ty + guideLength), outline, outlineWidth);
		cv::line(mat, cv::Point(lx, guideOffset), cv::Point(lx + guideLength, ty), color, guideWidth);
		cv::line(mat, cv::Point(lx, guideOffset), cv::Point(lx, ty + guideLength), color, guideWidth);

		int rx = mat.cols - guideOffset - guideWidth - xextra;
		outlinex = rx + outlineOffset;
		outliney = ty - outlineOffset;
		cv::line(mat, cv::Point(rx, outliney), cv::Point(rx - guideLength, outliney), outline, outlineWidth);
		cv::line(mat, cv::Point(outlinex, guideOffset), cv::Point(outlinex, ty + guideLength), outline, outlineWidth);
		cv::line(mat, cv::Point(rx, guideOffset), cv::Point(rx - guideLength, ty), color, guideWidth);
		cv::line(mat, cv::Point(rx, guideOffset), cv::Point(rx, ty + guideLength), color, guideWidth);

		int by = mat.rows - guideOffset - guideWidth - yextra;
		outlinex = lx - outlineOffset;
		outliney = by + outlineOffset;
		cv::line(mat, cv::Point(lx, outliney), cv::Point(lx + guideLength, outliney), outline, outlineWidth);
		cv::line(mat, cv::Point(outlinex, by), cv::Point(outlinex, by - guideLength), outline, outlineWidth);
		cv::line(mat, cv::Point(lx, by), cv::Point(lx + guideLength, by), color, guideWidth);
		cv::line(mat, cv::Point(lx, by), cv::Point(lx, by - guideLength), color, guideWidth);
	}

	void drawProgress(cv::Mat& mat, const std::vector<double>& progress)
	{
		if (progress.empty())
			return;

		int minsz = std::min(mat.cols, mat.rows);
		int fillWidth = minsz >> 7;
		int outlineWidth = fillWidth + (minsz >> 8) + 1;

		int barLength = (minsz >> 1) + (minsz >> 2);
		int barOffsetW = (minsz - barLength) >> 3;
		int barOffsetL = (minsz - barLength) >> 1;
		int outlineOffset = (outlineWidth - fillWidth) >> 1;

		cv::Scalar color = cv::Scalar(255,255,255);
		cv::Scalar outline = cv::Scalar(0,0,0);

		int px = barOffsetW;
		int py = mat.rows - barOffsetL;
		for (double p : progress)
		{
			int fillLength = (barLength * p);
			cv::line(mat, cv::Point(px - outlineOffset, py), cv::Point(px - outlineOffset, py - barLength), outline, outlineWidth);
			cv::line(mat, cv::Point(px, py), cv::Point(px, py - fillLength), color, fillWidth);

			px += outlineWidth + outlineWidth;
		}
	}

	void drawDebugInfo(cv::Mat& mat, MultiThreadedDecoder& proc)
	{
		std::stringstream sstop;
		sstop << "cfc using " << proc.num_threads() << " thread(s). " << proc.mode() << ":" << proc.detected_mode() << "..." << proc.backlog() << "? ";
		sstop << (MultiThreadedDecoder::bytes / std::max<double>(1, MultiThreadedDecoder::decoded)) << "b v0.6.4";
		std::stringstream ssmid;
		ssmid << "#: " << MultiThreadedDecoder::perfect << " / " << MultiThreadedDecoder::decoded << " / " << MultiThreadedDecoder::scanned << " / " << _calls;
		std::stringstream ssperf;
		ssperf << "scan: " << millis(MultiThreadedDecoder::scanTicks, MultiThreadedDecoder::scanned);
		ssperf << ", extract: " << millis(MultiThreadedDecoder::extractTicks, MultiThreadedDecoder::decoded);
		ssperf << ", decode: " << millis(MultiThreadedDecoder::decodeTicks, MultiThreadedDecoder::decoded);
		std::stringstream sstats;
		sstats << "Files received: " << proc.files_decoded() << ", in flight: " << proc.files_in_flight() << ". ";
		sstats << percent(MultiThreadedDecoder::perfect, MultiThreadedDecoder::decoded) << "% decode. ";
		sstats << percent(MultiThreadedDecoder::decoded, MultiThreadedDecoder::scanned) << "% scan.";

		cv::putText(mat, sstop.str(), cv::Point(5,50), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		cv::putText(mat, ssmid.str(), cv::Point(5,100), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		cv::putText(mat, ssperf.str(), cv::Point(5,150), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		cv::putText(mat, sstats.str(), cv::Point(5,200), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);

		/*std::stringstream ssperf2;
		ssperf2 << "reader ctor: " << millis(Decoder::readerInitTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", fount: " << millis(Decoder::fountTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", dodecode: " << millis(Decoder::decodeTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", readloop: " << millis(Decoder::bbTicks, MultiThreadedDecoder::decoded);
		ssperf2 << ", rss: " << millis(Decoder::rssTicks, MultiThreadedDecoder::decoded);
		cv::putText(mat, ssperf2.str(), cv::Point(5,300), cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255,255,80), 2);
		//*/
	}

	std::string mode_name(int modeVal)
	{
		switch (modeVal)
		{
			case 4:
				return "4C";
			case 66:
				return "Bu";
			case 67:
				return "Bm";
			case 68:
				return "B";
			case 69:
				return "5x5";
			case 70:
				return "5x5d";
			case 0:
			default:
				return "?";
		}
	}

	void update_live_rate()
	{
		auto now = std::chrono::steady_clock::now();
		const bool transferActive = MultiThreadedDecoder::bytes > 0 || _transferStatus > 0;
		if (transferActive && _transferStartTime == std::chrono::steady_clock::time_point{})
			_transferStartTime = now;
		if (_rateSnapshotTime == std::chrono::steady_clock::time_point{})
		{
			_rateSnapshotTime = now;
			_rateBytesSnapshot = MultiThreadedDecoder::bytes;
			_liveBytesPerSec = 0.0;
			_averageBytesPerSec = 0.0;
			return;
		}

		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _rateSnapshotTime).count();
		if (elapsed < 250)
			return;

		const clock_t bytesDelta = MultiThreadedDecoder::bytes - _rateBytesSnapshot;
		const double instantRate = (bytesDelta > 0 && elapsed > 0)
			? (static_cast<double>(bytesDelta) * 1000.0 / static_cast<double>(elapsed))
			: 0.0;
		_liveBytesPerSec = (_liveBytesPerSec <= 0.0)
			? instantRate
			: (_liveBytesPerSec * 0.65 + instantRate * 0.35);
		if (_transferStartTime != std::chrono::steady_clock::time_point{})
		{
			const auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _transferStartTime).count();
			if (totalElapsed > 0)
				_averageBytesPerSec = static_cast<double>(MultiThreadedDecoder::bytes) * 1000.0 / static_cast<double>(totalElapsed);
		}
		_rateBytesSnapshot = MultiThreadedDecoder::bytes;
		_rateSnapshotTime = now;
	}

	std::string format_rate(double bytesPerSec)
	{
		std::stringstream stream;
		stream.setf(std::ios::fixed);
		stream.precision(1);
		if (bytesPerSec >= 1024.0 * 1024.0)
			stream << (bytesPerSec / (1024.0 * 1024.0)) << " MB/s";
		else if (bytesPerSec >= 1024.0)
			stream << (bytesPerSec / 1024.0) << " KB/s";
		else
			stream << bytesPerSec << " B/s";
		return stream.str();
	}

	void drawTransferOverlay(cv::Mat& mat, const MultiThreadedDecoder& proc)
	{
		const int configuredMode = proc.mode();
		const int detectedMode = proc.detected_mode();
		const std::string activeMode = configuredMode == 0
			? fmt::format("AUTO -> {}", mode_name(detectedMode))
			: mode_name(configuredMode);
		const std::string modeLine = fmt::format(
			"Mode: {}  [{}|{}]  Files: {}/{}",
			activeMode,
			configuredMode == 0 ? "auto" : "lock",
			detectedMode ? mode_name(detectedMode) : "?",
			proc.files_decoded(),
			proc.files_in_flight()
		);
		const std::string rateLine = fmt::format(
			"Rate: {}  Decoded: {} KB",
			format_rate(_liveBytesPerSec),
			MultiThreadedDecoder::bytes / 1024
		);
		double maxProgress = 0.0;
		for (double p : proc.get_progress())
			maxProgress = std::max(maxProgress, p);
		std::string progressLine = fmt::format(
			"Avg: {}  Progress: {}%",
			format_rate(_averageBytesPerSec),
			static_cast<int>(maxProgress * 100.0)
		);
		if (maxProgress > 0.01 && maxProgress < 0.999 && _transferStartTime != std::chrono::steady_clock::time_point{})
		{
			auto now = std::chrono::steady_clock::now();
			const double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - _transferStartTime).count() / 1000.0;
			if (elapsedSec > 0.0)
			{
				const double etaSec = elapsedSec * (1.0 - maxProgress) / maxProgress;
				progressLine += fmt::format("  ETA: {}s", static_cast<int>(etaSec + 0.5));
			}
		}

		const int minsz = std::min(mat.cols, mat.rows);
		const double fontScale = std::max(0.55, minsz / 900.0);
		const int thickness = std::max(1, minsz / 420);
		const int left = std::max(10, minsz / 40);
		const int top = std::max(36, minsz / 18);
		const int lineGap = std::max(26, minsz / 18);
		const cv::Scalar outline(0, 0, 0);
		const cv::Scalar color(255, 255, 80);

		cv::putText(mat, modeLine, cv::Point(left, top), cv::FONT_HERSHEY_DUPLEX, fontScale, outline, thickness + 2);
		cv::putText(mat, modeLine, cv::Point(left, top), cv::FONT_HERSHEY_DUPLEX, fontScale, color, thickness);
		cv::putText(mat, rateLine, cv::Point(left, top + lineGap), cv::FONT_HERSHEY_DUPLEX, fontScale, outline, thickness + 2);
		cv::putText(mat, rateLine, cv::Point(left, top + lineGap), cv::FONT_HERSHEY_DUPLEX, fontScale, color, thickness);
		cv::putText(mat, progressLine, cv::Point(left, top + lineGap * 2), cv::FONT_HERSHEY_DUPLEX, fontScale, outline, thickness + 2);
		cv::putText(mat, progressLine, cv::Point(left, top + lineGap * 2), cv::FONT_HERSHEY_DUPLEX, fontScale, color, thickness);
	}

	std::string jstring_to_cppstr(JNIEnv *env, const jstring& dataPathObj)
	{
		const char* temp = env->GetStringUTFChars(dataPathObj, NULL);
		string res(temp);
		env->ReleaseStringUTFChars(dataPathObj, temp);
		return res;
	}
}

extern "C" {
jstring JNICALL
Java_org_cimbar_camerafilecopy_MainActivity_processImageJNI(JNIEnv *env, jobject instance, jlong matAddr, jstring dataPathObj, jint modeInt)
{
	++_calls;

	// get params from raw address
	Mat &mat = *(Mat *) matAddr;
	string dataPath = jstring_to_cppstr(env, dataPathObj);
	int modeVal = (int)modeInt;

	std::shared_ptr<MultiThreadedDecoder> proc;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_proc or !_proc->set_mode(modeVal))
			_proc = std::make_shared<MultiThreadedDecoder>(dataPath, modeVal);
		proc = _proc;
	}

	clock_t begin = clock();
	cv::Mat img = mat.clone();
	proc->add(img);

	if (_calls & 31)
	{
		clock_t decodeSnapshot = proc->decoded;
		clock_t perfectSnapshot = proc->perfect;
		_transferStatus = perfectSnapshot > _frameSuccessSnapshot; // a bit silly, but 1 == partial decode
		_transferStatus += (decodeSnapshot > _frameDecodeSnapshot); // 2 == full decode
		_frameDecodeSnapshot = decodeSnapshot;
		_frameSuccessSnapshot = perfectSnapshot;
	}

	update_live_rate();
	drawProgress(mat, proc->get_progress());
	drawGuidance(mat, _transferStatus);
	drawTransferOverlay(mat, *proc);

	// log computation time to Android Logcat
	double totalTime = double(clock() - begin) / CLOCKS_PER_SEC;
	__android_log_print(ANDROID_LOG_INFO, TAG, "processImage computation time = %f seconds\n",
						totalTime);

	// return a decoded file to prompt the user to save it, if there is a new one
	string result;
	if (proc->detected_mode()) // repurpose str for special message passing
		result = fmt::format("/{}", proc->detected_mode());

	std::vector<string> all_decodes = proc->get_done();
	for (string& s : all_decodes)
		if (_completed.find(s) == _completed.end())
		{
			_completed.insert(s);
			result = s;
		}
	return env->NewStringUTF(result.c_str());
}

jstring JNICALL
Java_org_cimbar_camerafilecopy_MainActivity_getDecoderTelemetryJNI(JNIEnv *env, jobject instance)
{
	std::lock_guard<std::mutex> lock(_mutex);
	std::string snapshot = telemetry_snapshot(_proc);
	return env->NewStringUTF(snapshot.c_str());
}

void JNICALL
Java_org_cimbar_camerafilecopy_MainActivity_shutdownJNI(JNIEnv *env, jobject instance) {
	__android_log_print(ANDROID_LOG_INFO, TAG, "Shutdown cfc-cpp\n");

	std::lock_guard<std::mutex> lock(_mutex);
	if (_proc)
		_proc->stop();
	_proc = nullptr;
	reset_session_state();
}

}
