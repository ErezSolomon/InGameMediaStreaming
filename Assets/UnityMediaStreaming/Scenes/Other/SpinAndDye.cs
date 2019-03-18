using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class SpinAndDye : MonoBehaviour
{
    public Transform Transform;
    public Renderer rend;

    void Start()
    {
        rend=GetComponent<Renderer>();
    }
    // Update is called once per frame
    void Update()
    {
        Transform.Rotate(Vector3.up);
        float lerp = Mathf.PingPong(Time.time, 3f);

        rend.material.color = Color.Lerp(Color.red, Color.blue, lerp);
    }
}
