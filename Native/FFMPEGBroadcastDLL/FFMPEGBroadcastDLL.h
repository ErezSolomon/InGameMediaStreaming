#pragma once

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the FFMPEGBROADCASTDLL_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// FFMPEGBROADCASTDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef FFMPEGBROADCASTDLL_EXPORTS
#define FFMPEGBROADCASTDLL_API extern "C" __declspec(dllexport)
#else
#define FFMPEGBROADCASTDLL_API extern "C" __declspec(dllimport)
#endif

#include <string>

struct Transmitter;

enum LogLevel
{
	LOG_LEVEL_INFO,
	LOG_LEVEL_ERROR
};

typedef void(__stdcall * LogCallback)(enum LogLevel log_level, const char* message);

struct TransmittingOptions
{
	int64_t bit_rate;// e.g. 40000000 is very good
	/* Resolution must be a multiple of two. */
	int output_width;
	int output_height;
	int frame_units_in_sec;// e.g. 1000
	int max_framerate;// e.g. 25
	int gop_size;// e.g. 12 (but has delay)
};

struct FrameInfo
{
	int width;
	int height;
	int nb_samples;
	int format;
	int key_frame;
	int pict_type;
	int sample_aspect_ratio_num;
	int sample_aspect_ratio_den;
	int64_t pts;
	int64_t pkt_dts;
	int coded_picture_number;
	int display_picture_number;
	int quality;
	int repeat_pict;
	int sample_rate;// (audio)
	uint64_t channel_layout;
	int flags;
	int color_range;
	int color_primaries;
	int color_trc;
	int colorspace;
	int chroma_location;
	int64_t best_effort_timestamp;
	int64_t pkt_pos;
	int64_t pkt_duration;
	int channels;// (audio)
	int pkt_size;
};

void call_log_info(const char* message);
void call_log_error(const char* message);

#define log_info(fmt, ...) do { \
	char _____buf[1024]; \
	sprintf(_____buf, fmt, ##__VA_ARGS__); \
	call_log_info(_____buf); \
} while (0)

#define log_error(fmt, ...) do { \
	char _____buf[1024]; \
	sprintf(_____buf, fmt, ##__VA_ARGS__); \
	call_log_error(_____buf); \
} while (0)

FFMPEGBROADCASTDLL_API void SetLogCallback(LogCallback log_callback);

FFMPEGBROADCASTDLL_API int64_t GetMicrosecondsTimeRelative();

FFMPEGBROADCASTDLL_API Transmitter* Transmitter_Create(const char *path, const char *format, const char *video_codec,
	int input_width, int input_height, TransmittingOptions options, const int dict_count, const char *dict_keys[], const char *dict_vals[]);

FFMPEGBROADCASTDLL_API void Transmitter_Destroy(Transmitter* transmitter);

FFMPEGBROADCASTDLL_API bool Transmitter_ShellWriteVideoNow(Transmitter* transmitter, int64_t time_diff);

FFMPEGBROADCASTDLL_API bool Transmitter_WriteVideoFrame(Transmitter* transmitter, int64_t time_diff, int input_width, int input_height, char* rgb_data);

struct Receiver;

FFMPEGBROADCASTDLL_API Receiver* Receiver_Create(const char *path);

FFMPEGBROADCASTDLL_API bool Receiver_GetUpdatedFrame(Receiver* receiver, int input_width, int input_height, char* rgb_data, FrameInfo* frame_info);

FFMPEGBROADCASTDLL_API void Receiver_Destroy(Receiver* receiver);