#include "stdafx.h"

#include "FFMPEGBroadcastDLL.h"

struct Receiver
{
private:
	AVFrame* m_frameBuffers[2] = { av_frame_alloc(), av_frame_alloc() };

	AVFrame* m_userFrame = nullptr;
	struct SwsContext *m_user_convert_ctx = nullptr;
	int m_lastFrameUserRead = 0;

	std::atomic<int> last_buffer = 0;
	std::atomic_flag buffer_lock = ATOMIC_FLAG_INIT;
	std::atomic<uint64_t> lastAvailableFrame = 0;

	std::string m_filePath;

	std::atomic<bool> m_isThreadFailed = false;
	std::atomic<bool> m_threadShouldRun = true;
	// It's very important it will be the last member, because of construction order and since thread is lanched on construction!
	std::thread m_receiverThread;

	int get_writing_buffer_number()
	{
		return (last_buffer + 1) % 2;
	}

	AVFrame* get_writing_buffer()
	{
		return m_frameBuffers[get_writing_buffer_number()];
	}

	void swap_buffers(uint64_t frameNumber)
	{
		if (!buffer_lock.test_and_set())
		{
			last_buffer = (last_buffer + 1) % 2;
			lastAvailableFrame = frameNumber;
			buffer_lock.clear();
		}
	}

	void threadFail() {
		m_isThreadFailed = true;
		while (buffer_lock.test_and_set())
			std::this_thread::yield();
	}
	bool isThreadFailed() { return m_isThreadFailed; }

	static int check_interrupt(void* usr_data)
	{
		const std::atomic<bool>& threadShouldRun = *reinterpret_cast<std::atomic<bool>*>(usr_data);
		return !threadShouldRun;
	}

	void threadFunc()
	{
		AVFormatContext *pFormatCtx;
		AVCodecContext *pCodecCtx;
		AVCodec *pCodec;
		AVPacket packet;
		AVDictionary *options = nullptr;

		/*//av_dict_set(&options, "buffer_size", "655360", 0);// TODO: Configure this!
		int result = av_dict_set(&options, "buffer_size", "2048", 0);// TODO: Configure this!
		if (result < 0)
			log_error("FFMPEG error while setting buffer_size.\n");
		result = av_dict_set(&options, "rtmp_buffer_size", "2048", 0);// TODO: Configure this!
		if (result < 0)
			log_error("FFMPEG error while setting rtmp_buffer_size.\n");*/

		pFormatCtx = avformat_alloc_context();
		pFormatCtx->interrupt_callback.opaque = &m_threadShouldRun;
		pFormatCtx->interrupt_callback.callback = &check_interrupt;

		// Try to reduce latency and memory
		//pFormatCtx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;// It doesn't work with that!
		pFormatCtx->flags = AVFMT_FLAG_NOBUFFER;// TODO: Check how helpful is it.
		// Future TODO: Maybe use NOBLOCK flag? (single threaded non-blocking)

		if (avformat_open_input(&pFormatCtx, m_filePath.c_str(), nullptr, &options) != 0)
		{
			log_error("Couldn't open input stream.\n");
			threadFail();
			return;// TODO: Fix memory leak on failure
		}

		if (avformat_find_stream_info(pFormatCtx, &options) < 0) {
			log_error("Couldn't find stream information.\n");
			threadFail();
			return;// TODO: Fix memory leak on failure
		}

		int videoIndex = -1;
		for (int i = 0; i < pFormatCtx->nb_streams; ++i)
			if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
				videoIndex = i;
				break;
			}

		if (videoIndex == -1) {
			log_error("Didn't find a video stream.\n");
			threadFail();
			return;// TODO: Fix memory leak on failure
		}

		pCodecCtx = pFormatCtx->streams[videoIndex]->codec;
		pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
		if (pCodec == nullptr)
		{
			log_error("Codec not found.\n");
			threadFail();
			return;// TODO: Fix memory leak on failure
		}
		if (avcodec_open2(pCodecCtx, pCodec, &options) < 0) {
			log_error("Could not open codec.\n");
			threadFail();
			return;// TODO: Fix memory leak on failure
		}
		
		av_dump_format(pFormatCtx, 0, m_filePath.c_str(), 0);// Output Info

		int pictureNumber = 0;

		while (m_threadShouldRun && !isThreadFailed())
		{
			// Get a frame from a format context
			int ret = av_read_frame(pFormatCtx, &packet);
			if (ret < 0)
			{
				log_error("FFMPEG error on reading number %d.\n", ret);
				threadFail();
				break;
			}

			if (packet.stream_index == videoIndex)
			{
				// Send packet to the codec context
				int ret = avcodec_send_packet(pCodecCtx, &packet);
				if (ret < 0)
				{
					log_error("Decode Error.\n");
					threadFail();
					break;
				}

				// Read each frame in the packet from the codec context
				while (ret >= 0)
				{
					AVFrame* frameBuffer = get_writing_buffer();
					ret = avcodec_receive_frame(pCodecCtx, frameBuffer);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					{
						// TODO: Handle better while getting EOF
						break;// Exit gracfully with no error
					}
					// otherwise

					if (ret < 0)
					{
						log_error("Decode Error.\n");
						threadFail();
						break;
					}
					// otherwise

					swap_buffers(++pictureNumber);
				}
			}

			av_packet_unref(&packet);
		}

		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
	}

public:
	Receiver(const char* filePath) : m_filePath(filePath), m_receiverThread([](Receiver* self) { self->threadFunc(); }, this)
	{
	}

	~Receiver()
	{
		// Stop thread and wait
		m_threadShouldRun = false;
		m_receiverThread.join();

		av_frame_free(&m_frameBuffers[0]);
		av_frame_free(&m_frameBuffers[1]);
		if (m_userFrame != nullptr)
			av_frame_free(&m_userFrame);

		sws_freeContext(m_user_convert_ctx);
	}

	struct SwsContext*& getUserConvertCtx()
	{
		return m_user_convert_ctx;
	}

	AVFrame*& getUserFrame()
	{
		return m_userFrame;
	}

	AVFrame* tryFetchNewBuffer()
	{
		if (lastAvailableFrame > m_lastFrameUserRead && !buffer_lock.test_and_set())
		{
			// Notice that lastAvailableFrame could be increased between last row and now(locked),
			//  but it's still ok since still: lastAvailableFrame > m_lastFrameUserRead
			m_lastFrameUserRead = lastAvailableFrame;
			return m_frameBuffers[last_buffer];
		}
		else
			return nullptr;
	}

	// TODO: Use it
	bool wasFailed()
	{
		return m_isThreadFailed;
	}

	void unlockBuffers()
	{
		return buffer_lock.clear();
	}
};

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

static void resize_frame(AVFrame*& frame, int width, int height)
{
	if (frame == nullptr || width != frame->width || height != frame->height)
	{
		if (frame != nullptr)
			av_frame_free(&frame);

		frame = alloc_picture(AV_PIX_FMT_RGB24, width, height);
	}
}

FFMPEGBROADCASTDLL_API Receiver* Receiver_Create(const char *path)
{
	Receiver* receiver = new Receiver(path);
	return receiver;
}

FFMPEGBROADCASTDLL_API bool Receiver_GetUpdatedFrame(Receiver* receiver, int output_width, int output_height, char* rgb_data, FrameInfo* frame_info)
{
	struct SwsContext*& convert_ctx = receiver->getUserConvertCtx();

	AVFrame*& userFrame = receiver->getUserFrame();
	const AVFrame* frame = receiver->tryFetchNewBuffer();
	if (frame == nullptr)
		return false;
	// otherwise

	resize_frame(userFrame, output_width, output_height);

	convert_ctx =
		sws_getCachedContext(convert_ctx, frame->width, frame->height, (AVPixelFormat)frame->format,
			output_width, output_height, AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr, nullptr, nullptr);

	sws_scale(convert_ctx, frame->data, frame->linesize, 0, frame->height,
		userFrame->data, userFrame->linesize);

	for (int y = 0; y < userFrame->height; y++)
	{
		const uint8_t* row_start = userFrame->data[0] + userFrame->linesize[0] * (userFrame->height - y - 1);
		std::copy(row_start, row_start + 3 * output_width, rgb_data + 3 * output_width * y);
	}

	av_frame_unref(userFrame);

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

	receiver->unlockBuffers();
	return true;
}

FFMPEGBROADCASTDLL_API void Receiver_Destroy(Receiver* receiver)
{
	delete receiver;
}