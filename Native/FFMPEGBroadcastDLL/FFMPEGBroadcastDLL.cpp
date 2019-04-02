// FFMPEGBroadcastDLL.cpp : Defines the exported functions for the DLL application.
//

// This file based on FFMPEG example muxing.c

#include "stdafx.h"

#include "FFMPEGBroadcastDLL.h"

using namespace std::literals;


int FINITE = 0;
#define INTERLEAVED_WRITE 1
#define FINITE            0
double STREAM_DURATION = 10.0;
int STREAM_FRAME_RATE = 25; // 25 images/s
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
struct OutputStream {
	AVStream *st;
	AVCodecContext *enc;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;
	AVRational time_threshold;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
};

struct Transmitter {
	OutputStream video_st/*, audio_st*/;
	std::string filename;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVCodec /**audio_codec,*/ *video_codec;
	int have_video/*, have_audio*/;
	int encode_video/*, encode_audio*/;
	AVDictionary *opt;
};

LogCallback log_callback = nullptr;

unsigned int tempBufIndex = 0;
const unsigned int tempBufsCount = 32;
const unsigned int tempBufSize = 128;
char tempBufs[tempBufsCount][tempBufSize];

#undef av_ts2str
#undef av_ts2timestr
#undef av_err2str

#define av_ts2str(ts) av_ts_make_string(tempBufs[++tempBufIndex % tempBufsCount], ts)

#define av_ts2timestr(ts, tb) av_ts_make_time_string(tempBufs[++tempBufIndex % tempBufsCount], ts, tb)

#define av_err2str(errnum) av_make_error_string(tempBufs[++tempBufIndex % tempBufsCount], tempBufSize, errnum)

void call_log_info(const char* message)
{
	if (log_callback != nullptr)
		log_callback(LOG_LEVEL_INFO, message);
	else
		printf("%s", message);
}

void call_log_error(const char* message)
{
	if (log_callback != nullptr)
		log_callback(LOG_LEVEL_ERROR, message);
	else
		fprintf(stderr, "%s", message);
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	log_info("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		pkt->stream_index);
}

static int shell_end(OutputStream *ost)
{
	AVCodecContext *c = ost->enc;

	/* check if we want to generate more frames */
	return (FINITE && av_compare_ts(ost->next_pts, c->time_base, STREAM_DURATION, av_make_q(1, 1)) >= 0);
}

static int shell_write_frame_now(OutputStream *ost, int64_t time_diff)
{
	AVCodecContext *c = ost->enc;

	int64_t next_frame_time_diff = av_rescale_q(ost->next_pts, c->time_base, av_make_q(1, 1000000));

	//log_info("video: %d real: %d\n", video_time_diff, time_diff);

	return (next_frame_time_diff <= time_diff) ? 1 : 0;
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	//log_packet(fmt_ctx, pkt);

#if INTERLEAVED_WRITE
	int res = av_interleaved_write_frame(fmt_ctx, pkt);
#else
	int res = av_write_frame(fmt_ctx, pkt);
#endif

	return res;
}

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
	AVCodec **codec, const char *codec_name, enum AVCodecID default_codec_id, TransmittingOptions options)
{
	AVCodecContext *c;
	int i;

	/* find the encoder */
	*codec = codec_name != nullptr ? avcodec_find_encoder_by_name(codec_name) : avcodec_find_encoder(default_codec_id);
	if (!(*codec)) {
		log_error("Could not find encoder for '%s'\n",
			codec_name != nullptr ? codec_name : avcodec_get_name(default_codec_id));
		exit(1);
	}

	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) {
		log_error("Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		log_error("Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;

	ost->time_threshold = av_make_q(1, options.max_framerate);

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = 64000;
		c->sample_rate = 44100;
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if ((*codec)->channel_layouts) {
			c->channel_layout = (*codec)->channel_layouts[0];
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		ost->st->time_base = av_make_q(1, c->sample_rate);
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = (*codec)->id;

		c->bit_rate = options.bit_rate;
		/* Resolution must be a multiple of two. */
		c->width = options.output_width;
		c->height = options.output_height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		* of which frame timestamps are represented. For fixed-fps content,
		* timebase should be 1/framerate and timestamp increments should be
		* identical to 1. */
		ost->st->time_base = av_make_q(1, options.frame_units_in_sec);
		c->time_base = ost->st->time_base;

		c->gop_size = options.gop_size; ///* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B-frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			* This does not happen with normal video, it just happens here as
			* the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;

	default:
		break;
	}

	// Test
	//c->gop_size = 5;
	//c->mb_decision = 2;

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

#ifdef WHEN_AUDIO_REQUIRED
/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
	uint64_t channel_layout,
	int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		log_error("Error allocating an audio frame\n");
		exit(1);
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			log_error("Error allocating an audio buffer\n");
			exit(1);
		}
	}

	return frame;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;

	c = ost->enc;

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		log_error("Could not open audio codec: %s\n", av_err2str(ret));
		exit(1);
	}

	/* init signal generator */
	ost->t = 0;
	ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	ost->frame = alloc_audio_frame(c->sample_fmt, c->channel_layout,
		c->sample_rate, nb_samples);
	ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
		c->sample_rate, nb_samples);

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		log_error("Could not copy the stream parameters\n");
		exit(1);
	}

	/* create resampler context */
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		log_error("Could not allocate resampler context\n");
		exit(1);
	}

	/* set options */
	av_opt_set_int(ost->swr_ctx, "in_channel_count", c->channels, 0);
	av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(ost->swr_ctx, "out_channel_count", c->channels, 0);
	av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		log_error("Failed to initialize the resampling context\n");
		exit(1);
	}
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
* 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
	AVFrame *frame = ost->tmp_frame;
	int j, i, v;
	int16_t *q = (int16_t*)frame->data[0];

	if (shell_end(ost))
		return NULL;

	for (j = 0; j <frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->enc->channels; i++)
			*q++ = v;
		ost->t += ost->tincr;
		ost->tincr += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;

	return frame;
}

/*
* encode one audio frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
*/
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame;
	int ret;
	int got_packet;
	int dst_nb_samples;

	av_init_packet(&pkt);
	c = ost->enc;

	frame = get_audio_frame(ost);

	if (frame) {
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
			c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);

		/* when we pass a frame to the encoder, it may keep a reference to it
		* internally;
		* make sure we do not overwrite it here
		*/
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);

		/* convert to destination format */
		ret = swr_convert(ost->swr_ctx,
			ost->frame->data, dst_nb_samples,
			(const uint8_t **)frame->data, frame->nb_samples);
		if (ret < 0) {
			log_error("Error while converting\n");
			exit(1);
		}
		frame = ost->frame;

		frame->pts = av_rescale_q(ost->samples_count, (AVRational) { 1, c->sample_rate }, c->time_base);
		ost->samples_count += dst_nb_samples;
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		log_error("Error encoding audio frame: %s\n", av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			log_error("Error while writing audio frame: %s\n",
				av_err2str(ret));
			exit(1);
		}
	}

	return (frame || got_packet) ? 0 : 1;
}
#endif

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 0);
	if (ret < 0) {
		log_error("Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->enc;
	AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		log_error("Could not open video codec: %s\n", av_err2str(ret));
		exit(1);
	}

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		log_error("Could not allocate video frame\n");
		exit(1);
	}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		log_error("Could not copy the stream parameters\n");
		exit(1);
	}
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index,
	int width, int height)
{
	int x, y, i;

	i = frame_index;

	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

/* Prepare a dummy image. */
static void fill_image(AVFrame *pict, int width, int height, char* rgb_data)
{
	/*
	Note: This is WRONG!
	int i = 0;// R->G->B
	
	for (int i = 0; i < 3; i++)
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				pict->data[i][y * pict->linesize[i] + x] = rgb_data[3 * (x + y*width) + i];*/

	// This is wrong too, since we their is padding in the end of every row
	//std::copy(rgb_data, rgb_data + 3 * width * height, pict->data[0]);

	for (int y = 0; y < height; y++)
		std::copy(rgb_data + 3 * width * y, rgb_data + 3 * width * (y + 1), pict->data[0] + pict->linesize[0] * (height - y));
}

static AVFrame *get_video_frame(OutputStream *ost, int64_t time_diff, char* rgb_data)
{
	AVCodecContext *c = ost->enc;

	if (rgb_data == nullptr)
		return NULL;

	/* when we pass a frame to the encoder, it may keep a reference to it
	* internally; make sure we do not overwrite it here */
	if (av_frame_make_writable(ost->frame) < 0)
		exit(1);

	ost->sws_ctx = sws_getCachedContext(ost->sws_ctx, ost->tmp_frame->width, ost->tmp_frame->height,
		AV_PIX_FMT_RGB24,
		c->width, c->height,
		c->pix_fmt,
		SCALE_FLAGS, NULL, NULL, NULL);
	if (!ost->sws_ctx) {
		log_error("Could not initialize the conversion context\n");
		exit(1);
	}
	//fill_yuv_image(ost->tmp_frame, ost->next_pts, c->width, c->height);
	fill_image(ost->tmp_frame, ost->tmp_frame->width, ost->tmp_frame->height, rgb_data);
	sws_scale(ost->sws_ctx, (const uint8_t * const *)ost->tmp_frame->data,
		ost->tmp_frame->linesize, 0, ost->tmp_frame->height, ost->frame->data,
		ost->frame->linesize);

	//ost->frame->pts = ost->next_pts++;
	ost->frame->pts = av_rescale_q_rnd(time_diff, av_make_q(1, 1000000), c->time_base, AV_ROUND_DOWN);
	ost->next_pts = ost->frame->pts + 1 + av_rescale_q_rnd(1, ost->time_threshold, c->time_base, AV_ROUND_DOWN);

	return ost->frame;
}

/*
* encode one video frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
*/
static int write_video_frame(AVFormatContext *oc, OutputStream *ost, int64_t time_diff, char* rgb_data, FrameInfo* frame_info)
{
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt = { 0 };

	c = ost->enc;

	frame = get_video_frame(ost, time_diff, rgb_data);

	av_init_packet(&pkt);

	/* encode the image */
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		log_error("Error encoding video frame: %s\n", av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		//av_interleaved_write_frame(oc, NULL);// flush
		//av_write_frame(oc, NULL);// flush
	}
	else {
		ret = 0;
	}

	av_packet_unref(&pkt);
	//av_frame_unref(frame);

	if (ret < 0) {
		log_error("Error while writing video frame: %s\n", av_err2str(ret));
		exit(1);
	}

	// TODO: Export this duplicated code(with receiver) to helper functions.
	if (frame_info != nullptr)
	{
		frame_info->width = frame->width;
		frame_info->height = frame->height;
		frame_info->nb_samples = frame->nb_samples;
		frame_info->format = frame->format;
		frame_info->key_frame = frame->key_frame;
		frame_info->pict_type = frame->pict_type;
		frame_info->sample_aspect_ratio_num = frame->sample_aspect_ratio.num;
		frame_info->sample_aspect_ratio_den = frame->sample_aspect_ratio.den;
		frame_info->pts = frame->pts;
		frame_info->pkt_dts = frame->pkt_dts;
		frame_info->coded_picture_number = frame->coded_picture_number;
		frame_info->display_picture_number = frame->display_picture_number;
		frame_info->quality = frame->quality;
		frame_info->repeat_pict = frame->repeat_pict;
		frame_info->sample_rate = frame->sample_rate;
		frame_info->channel_layout = frame->channel_layout;
		frame_info->flags = frame->flags;
		frame_info->color_range = frame->color_range;
		frame_info->color_primaries = frame->color_primaries;
		frame_info->color_trc = frame->color_trc;
		frame_info->colorspace = frame->colorspace;
		frame_info->chroma_location = frame->chroma_location;
		frame_info->best_effort_timestamp = frame->best_effort_timestamp;
		frame_info->pkt_pos = frame->pkt_pos;
		frame_info->pkt_duration = frame->pkt_duration;
		frame_info->channels = frame->channels;
		frame_info->pkt_size = frame->pkt_size;
	}

	return (frame || got_packet) ? 0 : 1;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */

void transmitter_resize_video(Transmitter* transmitter, int width, int height)
{
	if (transmitter->video_st.tmp_frame != nullptr)
		av_frame_free(&transmitter->video_st.tmp_frame);

	transmitter->video_st.tmp_frame = alloc_picture(AV_PIX_FMT_RGB24, width, height);
	if (transmitter->video_st.tmp_frame == nullptr) {
		throw std::runtime_error("Could not allocate temporary picture\n");
	}
}

FFMPEGBROADCASTDLL_API void SetLogCallback(LogCallback log_callback)
{
	::log_callback = log_callback;
}

Transmitter* Transmitter_TryCreate(const char *path, const char *format, const char* video_codec_name, int width, int height,
	TransmittingOptions options, const int dict_count, const char *dict_keys[], const char *dict_vals[])
{
	if (!(options.output_width % 2 == 0 && options.output_height % 2 == 0))
		throw std::runtime_error("Resolution must be a multiple of two!");

	if (options.frame_units_in_sec <= 0)
		throw std::runtime_error("Frame units must be integer greater than zero!");

	if (options.max_framerate <= 0)
		throw std::runtime_error("Max framerate must be integer greater than zero!");

	if (options.gop_size < 3)
		throw std::runtime_error("Gop size must be at least 3!");

	if (options.bit_rate < 0)
		throw std::runtime_error("Bit rate must be integer greater or equal to zero!");

	std::unique_ptr<Transmitter> transmitter(new Transmitter());
	//OutputStream video_st = { 0 }, audio_st = { 0 };

	std::memset(&transmitter->video_st, 0, sizeof(OutputStream));
	//std::memset(&transsmitter->audio_st, 0, sizeof(OutputStream));

	//AVCodec *audio_codec, *video_codec;
	//int ret;
	//int have_video = 0, have_audio = 0;
	//int encode_video = 0, encode_audio = 0;


	transmitter->filename = path;
	transmitter->opt = NULL;

	for (int i = 0; i < dict_count; ++i)
		av_dict_set(&transmitter->opt, dict_keys[i], dict_vals[i], 0);

	// Try to guess protocol
	if (format == nullptr)
	{
		std::string rtmpPrefix = "rtmp://";
		std::string pathStr = path;

		auto res = std::mismatch(rtmpPrefix.begin(), rtmpPrefix.end(), pathStr.begin());

		if (res.first == rtmpPrefix.end())
			format = "flv";
	}

	/* allocate the output media context */
	avformat_alloc_output_context2(&transmitter->oc, NULL, format, path);
	if (!transmitter->oc)
		throw std::runtime_error("FFMPEG failed to create output context!");

	transmitter->fmt = transmitter->oc->oformat;

	/* Add the audio and video streams using the default format codecs
	* and initialize the codecs. */
	if (transmitter->fmt->video_codec != AV_CODEC_ID_NONE) {
		add_stream(&transmitter->video_st, transmitter->oc, &transmitter->video_codec, video_codec_name, transmitter->fmt->video_codec, options);
		transmitter->have_video = 1;
		transmitter->encode_video = 1;
	}
	/*if (fmt->audio_codec != AV_CODEC_ID_NONE) {
	add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
	have_audio = 1;
	encode_audio = 1;
	}*/

	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */
	if (transmitter->have_video) {
		open_video(transmitter->oc, transmitter->video_codec, &transmitter->video_st, transmitter->opt);

		transmitter_resize_video(transmitter.get(), width, height);
	}

	/*if (transsmitter->have_audio)
	open_audio(transsmitter->oc, transsmitter->audio_codec, &transsmitter->audio_st, transsmitter->opt);*/

	av_dump_format(transmitter->oc, 0, path, 1);

	/* open the output file, if needed */
	if (!(transmitter->fmt->flags & AVFMT_NOFILE)) {
		int ret = avio_open(&transmitter->oc->pb, path, AVIO_FLAG_WRITE);
		if (ret < 0) {
			log_error("Could not open '%s': %s\n", path,
				av_err2str(ret));
			throw std::runtime_error("FFMPEG couldn't open output file!");
		}
	}

	/* Write the stream header, if any. */
	int ret = avformat_write_header(transmitter->oc, &transmitter->opt);
	if (ret < 0) {
		log_error("Error occurred when opening output file: %s\n",
			av_err2str(ret));
		throw std::runtime_error("FFMPEG error while writing file!");
	}

	return transmitter.release();
}

FFMPEGBROADCASTDLL_API Transmitter* Transmitter_Create(const char *path, const char *format, const char* video_codec_name, int input_width, int input_height,
	TransmittingOptions options, const int dict_count, const char *dict_keys[], const char *dict_vals[])
{
	try
	{
		Transmitter* result = Transmitter_TryCreate(path, format, video_codec_name, input_width, input_height, options, dict_count, dict_keys, dict_vals);
		return result;
	}
	catch (std::exception& e)
	{
		// TODO: Handler on error?
		std::cerr << "Error while creating FFMPEG Transmitter: " << e.what() << std::endl;
		return nullptr;
	}
}

FFMPEGBROADCASTDLL_API bool Transmitter_ShellWriteVideoNow(Transmitter* transmitter, int64_t time_diff)
{
	return transmitter->encode_video && shell_write_frame_now(&transmitter->video_st, time_diff);
}

FFMPEGBROADCASTDLL_API bool Transmitter_WriteVideoFrame(Transmitter* transmitter, int64_t time_diff, int input_width, int input_height, char* rgb_data, FrameInfo* frame_info)
{
	OutputStream *ost = &transmitter->video_st;
	if (input_width != ost->tmp_frame->width || input_height != ost->tmp_frame->height)
		transmitter_resize_video(transmitter, input_width, input_height);

	if (transmitter->encode_video)
		transmitter->encode_video = !write_video_frame(transmitter->oc, ost, time_diff, rgb_data, frame_info);

	return transmitter->encode_video;
}

FFMPEGBROADCASTDLL_API void Transmitter_Destroy(Transmitter* transmitter)
{
	if (transmitter == nullptr)
		return;
	// otherwise

	log_info("Writing trailer and out\n");

	/* Write the trailer, if any. The trailer must be written before you
	* close the CodecContexts open when you wrote the header; otherwise
	* av_write_trailer() may try to use memory that was freed on
	* av_codec_close(). */
	av_write_trailer(transmitter->oc);

	/* Close each codec. */
	if (transmitter->have_video)
		close_stream(transmitter->oc, &transmitter->video_st);
	/*if (transsmitter->have_audio)
		close_stream(transsmitter->oc, &transsmitter->audio_st);*/

	if (!(transmitter->fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&transmitter->oc->pb);

	/* free the stream */
	avformat_free_context(transmitter->oc);

	delete transmitter;
}

FFMPEGBROADCASTDLL_API int64_t GetMicrosecondsTimeRelative()
{
	// TODO: Properly test timestamps
	return av_gettime_relative();
#if 0
#if defined(_MSC_VER)
	LARGE_INTEGER now;
	LARGE_INTEGER freq;

	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&now);

	int64_t result = (now.QuadPart * 1000000) / freq.QuadPart;

	return result;
#else

	using Clock = std::chrono::steady_clock;
	auto diff = Clock::now().time_since_epoch();
	return diff / 1us;
#endif
#endif
}