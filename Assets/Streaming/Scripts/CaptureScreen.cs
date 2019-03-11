using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// If SourceTexture isn't null, capture the source texture.
/// Otherwise, if it's a component of a camera, capture camera at OnPostRender().
/// Otherwise, capture final entire game screen at OnGUI().
/// </summary>
[ExecuteInEditMode]
public class CaptureScreen : MonoBehaviour
{
    public RenderTexture SourceTexture = null;

    public delegate void CaptureFunc(Texture2D texture);
    public delegate void TextureOutputFunc(CaptureFunc captureFunc);

    public event TextureOutputFunc OnCapturingTime;

    private bool hasCamera;

    // Use this for initialization
    void Start () {
        hasCamera = (GetComponent<Camera>() != null);
    }

    // Update is called once per frame
    /*void Update () {
		
	}*/

    public int width { get { return (SourceTexture == null) ? Screen.width : SourceTexture.width; } }
    public int height { get { return (SourceTexture == null) ? Screen.height : SourceTexture.height; } }

    private void Capture(Texture2D textureToDraw)
    {
        /*if (OutputStream == null || OutputStream.TextureToDraw == null || !OutputStream.ShouldCapture())
            return;
        // otherwise
        */

        if (textureToDraw == null)
            return;
        // otherwise

        int width = this.width;
        int height = this.height;

        if (textureToDraw.width != width || textureToDraw.height != height)
            textureToDraw.Resize(width, height);

        RenderTexture.active = SourceTexture;

        textureToDraw.ReadPixels(new Rect(0, 0, width, height),
            0, 0, false);
        textureToDraw.Apply();

        //OutputStream.Process();
    }

    private void CallOnCapturingTime()
    {
        if (OnCapturingTime != null)
            OnCapturingTime(Capture);
    }

    private void OnPostRender()
    {
        if (hasCamera)
            CallOnCapturingTime();
    }

    private void OnGUI()
    {
        if (!hasCamera)
            CallOnCapturingTime();
    }
}
