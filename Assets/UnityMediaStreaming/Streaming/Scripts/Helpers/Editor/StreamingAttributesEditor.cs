using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using UnityEngine.UI;

namespace UnityMediaStreaming
{
    // This idea is taken from It3ration answer at
    //  https://answers.unity.com/questions/489942/how-to-make-a-readonly-property-in-inspector.html
    [CustomPropertyDrawer(typeof(ReadOnlyWhilePlayingAttribute))]
    public class StreamingReadOnlyDrawer : PropertyDrawer
    {
        public override float GetPropertyHeight(SerializedProperty property,
                                                GUIContent label)
        {
            return EditorGUI.GetPropertyHeight(property, label, true);
        }

        public override void OnGUI(Rect position,
                                   SerializedProperty property,
                                   GUIContent label)
        {
            GUI.enabled = !Application.isPlaying;
            EditorGUI.PropertyField(position, property, label, true);
            GUI.enabled = true;
        }
    }
}