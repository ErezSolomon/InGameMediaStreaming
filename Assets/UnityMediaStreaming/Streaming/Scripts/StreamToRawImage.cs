using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace UnityMediaStreaming
{
    [RequireComponent(typeof(UnityEngine.UI.RawImage))]
    public class StreamToRawImage : MonoBehaviour
    {
        private Texture2D OutputTexture = null;
        private CaptureScreen PrevSource;
        public CaptureScreen Source;
        private UnityEngine.UI.RawImage output;

        void Start()
        {
            if (Source != null)
            {
                if (OutputTexture == null)
                    OutputTexture = new Texture2D(Source.Width, Source.Height, TextureFormat.RGB24, false);

                Source.OnCapturingTime += Process;
            }

            output = GetComponent<UnityEngine.UI.RawImage>();
        }

        private void Process(CaptureScreen.CaptureFunc captureFunc)
        {
            if (enabled)
            {
                captureFunc(OutputTexture);
                output.texture = OutputTexture;
            }
        }
    }
}