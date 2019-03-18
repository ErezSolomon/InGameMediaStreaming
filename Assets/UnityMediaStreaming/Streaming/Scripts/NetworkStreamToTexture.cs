using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace UnityMediaStreaming
{
    [RequireComponent(typeof(UnityEngine.UI.RawImage))]
    public class NetworkStreamToTexture : MonoBehaviour
    {
        private Texture2D OutputTexture;
        private IntPtr _Receiver = IntPtr.Zero;
        private UnityEngine.UI.RawImage output;
        private byte[] texData;

        [SerializeField/*, ReadOnlyWhenPlaying*/]
        private string _Path = "";
        public string Path { get => _Path; }

        [SerializeField]
        private int _Width = 1024;
        public int Width { get => _Width; set => _Width = value; }

        [SerializeField]
        private int _Height = 1024;
        public int Height { get => _Height; set => _Height = value; }

        // Used for correctly default fields
        private NetworkStreamToTexture()
        {
        }

        public NetworkStreamToTexture(string Path)
        {
            _Path = Path;
        }

        public NetworkStreamToTexture(string Path, int Width, int Height)
        {
            _Path = Path;
            _Width = Width;
            _Height = Height;
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

            output = GetComponent<UnityEngine.UI.RawImage>();

            OutputTexture = new Texture2D(Width, Height, TextureFormat.RGB24, false);

            output.texture = OutputTexture;

            texData = new byte[Width * Height * 3];
        }

        void OnDestroy()
        {
            StopStream();
        }

        // Start stream if haven't already
        private void StartStream()
        {
            if (_Receiver == IntPtr.Zero)
                _Receiver = CFFMPEGBroadcasting.Receiver.Create(Path);
        }

        // Stop stream if haven't already
        private void StopStream()
        {
            if (_Receiver != IntPtr.Zero)
            {
                CFFMPEGBroadcasting.Receiver.Destroy(_Receiver);
                _Receiver = IntPtr.Zero;
            }
        }

        private void Update()
        {
            if (!enabled)
            {
                StopStream();
                return;
            }
            // otherwise

            StartStream();

            if (_Receiver == IntPtr.Zero)
                return;
            // otherwise

            if (OutputTexture.width != Width || OutputTexture.height != Height)
            {
                OutputTexture.Resize(Width, Height, TextureFormat.RGB24, false);
                texData = new byte[Width * Height * 3];
            }

            if (!CFFMPEGBroadcasting.Receiver.GetUpdatedFrame(_Receiver, OutputTexture.width, OutputTexture.height, texData))
                return;
            // otherwise

            OutputTexture.LoadRawTextureData(texData);
            OutputTexture.Apply();
        }
    }
}