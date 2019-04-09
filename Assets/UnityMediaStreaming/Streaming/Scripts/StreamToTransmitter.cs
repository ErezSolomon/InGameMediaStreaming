using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace UnityMediaStreaming
{
    public class StreamToTransmitter : MonoBehaviour
    {
        private Texture2D OutputTexture;
        public CaptureScreen Source;
        private IntPtr _Transmitter = IntPtr.Zero;

        [SerializeField, ReadOnlyWhilePlaying]
        private string _Path = "";
        public string Path { get => _Path; }

        [SerializeField, ReadOnlyWhilePlaying]
        [Tooltip("Leave blank for auto")]
        private string _Format = "";
        public string Format { get => _Format; }

        [SerializeField, ReadOnlyWhilePlaying]
        [Tooltip("Leave blank for auto")]
        private string _VideoCodec = "";
        public string VideoCodec { get => _VideoCodec; }

        // TODO: Allow to change them also while running?
        [SerializeField, ReadOnlyWhilePlaying]
        private TransmittingOptions _TransmittingOptions = new TransmittingOptions();
        public TransmittingOptions TransmittingOptions { get => _TransmittingOptions; set => _TransmittingOptions = value; }

        [SerializeField]
        private FrameInfo _LastFrameInfo;
        public FrameInfo LastFrameInfo { get => _LastFrameInfo; }

        /*[SerializeField, ReadOnlyWhilePlaying]
        private long _BitRate = 40000000L;
        public long BitRate { get => _BitRate; set => _BitRate = value; }
        [SerializeField, ReadOnlyWhilePlaying]
        private long _OutputWidth = 256;
        public long OutputWidth { get => _OutputWidth; set => _OutputWidth = value; }
        [SerializeField, ReadOnlyWhilePlaying]
        private int _OutputHeight = 256;
        public int OutputHeight { get => _OutputHeight; set => _OutputHeight = value; }
        [SerializeField, ReadOnlyWhilePlaying]
        private int _FrameUnitsInSec = 1000;
        public int FrameUnitsInSec { get => _FrameUnitsInSec; set => _FrameUnitsInSec = value; }
        [SerializeField, ReadOnlyWhilePlaying]
        private int _MaxFramerate = 25;
        public int MaxFramerate { get => _MaxFramerate; set => _MaxFramerate = value; }
        [SerializeField, ReadOnlyWhilePlaying]
        private int _GopSize = 12;
        public int GopSize { get => _GopSize; set => _GopSize = value; }*/
        
        private System.Diagnostics.Stopwatch _clock = new System.Diagnostics.Stopwatch();

        // Used for correctly default fields
        private StreamToTransmitter()
        {
        }

        public StreamToTransmitter(string Path, string Format = "", string VideoCodec = "")
        {
            _Path = Path;
            _Format = Format;
            _VideoCodec = VideoCodec;
        }

        private static void Log(CFFMPEGBroadcasting.LogLevel logLevel, string message)
        {
            switch (logLevel)
            {
                case CFFMPEGBroadcasting.LogLevel.Info:
                    Debug.Log(message);
                    break;
                case CFFMPEGBroadcasting.LogLevel.Error:
                    Debug.LogError(message);
                    break;
            }
        }

        void Start()
        {
            CFFMPEGBroadcasting.SetLogCallback(Log);

            if (Source == null)
                return;
            // otherwise

            OutputTexture = new Texture2D(Source.Width, Source.Height, TextureFormat.RGB24, false);

            Source.OnCapturingTime += Process;
        }
        void OnDestroy()
        {
            StopStream();
        }

        // Start stream if haven't already
        private void StartStream()
        {
            if (_clock.IsRunning)
                return;// Already started
                       // otherwise

            _Transmitter = CFFMPEGBroadcasting.Transmitter.Create(Path, (Format.Length == 0 ? null : Format), (VideoCodec.Length == 0 ? null : VideoCodec),
                    Source.Width, Source.Height, TransmittingOptions, 0, null, null);

            _clock.Restart();
        }

        // Stop stream if haven't already
        private void StopStream()
        {
            _clock.Stop();

            if (_Transmitter != IntPtr.Zero)
            {
                CFFMPEGBroadcasting.Transmitter.Destroy(_Transmitter);
                _Transmitter = IntPtr.Zero;
            }
        }

        private void Process(CaptureScreen.CaptureFunc captureFunc)
        {
            if (!enabled)
            {
                StopStream();
                return;
            }
            // otherwise

            StartStream();

            if (_Transmitter == IntPtr.Zero)
                return;
            // otherwise
            
            Int64 timeDiff = (_clock.ElapsedTicks * 1000000) / System.Diagnostics.Stopwatch.Frequency;

            if (!CFFMPEGBroadcasting.Transmitter.ShellWriteVideoNow(_Transmitter, timeDiff))
                return;
            // otherwise

            captureFunc(OutputTexture);

            var pix = OutputTexture.GetRawTextureData();

            if (!CFFMPEGBroadcasting.Transmitter.WriteVideoFrame(_Transmitter, timeDiff, OutputTexture.width,
                OutputTexture.height, pix, out _LastFrameInfo))
                StopStream();
        }
    }
}