using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[RequireComponent(typeof(UnityEngine.UI.RawImage))]
public class NetworkStreamToTexture : MonoBehaviour
{
    private Texture2D OutputTexture;
    private IntPtr _Receiver = IntPtr.Zero;
    private UnityEngine.UI.RawImage output;
    private byte[] texData;

    [SerializeField/*, ReadOnlyWhenPlaying*/]
    private string Path = "";
    [SerializeField]
    private int Width = 1024;
    [SerializeField]
    private int Heigth = 1024;

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

        output = GetComponent<UnityEngine.UI.RawImage>();

        //Vector2 size = output.rectTransform.sizeDelta;
        //OutputTexture = new Texture2D((int)size.x, (int)size.y, TextureFormat.RGB24, false);
        OutputTexture = new Texture2D(Width, Heigth, TextureFormat.RGB24, false);
        //OutputTexture = new Texture2D(Width, Heigth, TextureFormat.RGBA32, false);

        output.texture = OutputTexture;

        texData = new byte[Width * Heigth * 3];
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

        //var pix = OutputTexture.GetRawTextureData();

        if (!CFFMPEGBroadcasting.Receiver.GetUpdatedFrame(_Receiver, OutputTexture.width, OutputTexture.height, texData))
            return;
        // otherwise

        /*for (int i = 0; i < OutputTexture.height; ++i)
            for (int j = 0; j < OutputTexture.width; j++)
                pix[3 * (i * OutputTexture.width + j)] = (byte)((i + j)%256);*/



        /*var data = OutputTexture.GetRawTextureData<Color32>();
        // fill texture data with a simple pattern
        Color32 orange = new Color32(255, 165, 0, 255);
        Color32 teal = new Color32(0, 128, 128, 255);
        int index = 0;
        for (int y = 0; y < OutputTexture.height; y++)
        {
            for (int x = 0; x < OutputTexture.width; x++)
            {
                data[index++] = ((x & y) == 0 ? orange : teal);
            }
        }*/

        /*byte[] data = OutputTexture.GetRawTextureData();
        // fill texture data with a simple pattern
        long index = 0;
        for (int y = 0; y < OutputTexture.height; y++)
        {
            for (int x = 0; x < OutputTexture.width; x++)
            {
                data[index++] = 128;
                data[index++] = 0;
                data[index++] = 0;
                data[index++] = 128;
            }
        }*/

        OutputTexture.LoadRawTextureData(texData);
        OutputTexture.Apply();
    }
}
