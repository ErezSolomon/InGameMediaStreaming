using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class StreamToTransmitter : MonoBehaviour {
    private Texture2D OutputTexture;
    public CaptureScreen Source;
    private IntPtr _Transmitter = IntPtr.Zero;

    [SerializeField/*, ReadOnlyWhenPlaying*/]
    private string Path;
    [SerializeField/*, ReadOnlyWhenPlaying*/]
    [Tooltip("Leave blank for auto")]
    private string Format;
    [SerializeField/*, ReadOnlyWhenPlaying*/]
    [Tooltip("Leave blank for auto")]
    private string VideoCodec;

    [SerializeField/*, ReadOnlyWhenPlaying*/]
    private TransmittingOptions TransmittingOptions = new TransmittingOptions(40000000L, 256, 256, 1000, 25, 12);

    private Int64 _StartingTime = 0;

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

    void Start () {
        CFFMPEGBroadcasting.SetLogCallback(Log);

        if (Source == null)
            return;
        // otherwise

        OutputTexture = new Texture2D(Source.width, Source.height, TextureFormat.RGB24, false);

        Source.OnCapturingTime += Process;
    }
    void OnDestroy()
    {
        StopStream();
    }

    // Start stream if haven't already
    private void StartStream()
    {
        if (_StartingTime != 0)
            return;// Already started
        // otherwise

        _Transmitter = CFFMPEGBroadcasting.Transmitter.Create(Path, (Format.Length == 0 ? null : Format), (VideoCodec.Length == 0 ? null : VideoCodec),
                Source.width, Source.height, TransmittingOptions, 0, null, null);
        _StartingTime = CFFMPEGBroadcasting.GetMicrosecondsTimeRelative();
    }

    // Stop stream if haven't already
    private void StopStream()
    {
        _StartingTime = 0;

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

        Int64 currentTime = CFFMPEGBroadcasting.GetMicrosecondsTimeRelative();
        Int64 timeDiff = currentTime - _StartingTime;

        if (!CFFMPEGBroadcasting.Transmitter.ShellWriteVideoNow(_Transmitter, timeDiff))
            return;
        // otherwise

        captureFunc(OutputTexture);

        var pix = OutputTexture.GetRawTextureData();

        if (!CFFMPEGBroadcasting.Transmitter.WriteVideoFrame(_Transmitter, timeDiff, OutputTexture.width, OutputTexture.height, pix))
            StopStream();
    }
}
