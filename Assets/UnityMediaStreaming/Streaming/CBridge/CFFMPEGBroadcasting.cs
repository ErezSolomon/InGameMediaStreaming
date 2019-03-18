
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

namespace UnityMediaStreaming
{
    [Serializable]
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct TransmittingOptions
    {
        public long BitRate;// e.g 40000000L
                            /* Resolution must be a multiple of two. */
        public int OutputWidth;
        public int OutputHeight;
        public int FrameUnitsInSec;// e.g. 1000
        public int MaxFramerate;// e.g. 25
        public int GopSize;// e.g. 12

        public TransmittingOptions(long BitRate = 40000000L, int OutputWidth = 256, int OutputHeight = 256, int FrameUnitsInSec = 1000, int MaxFramerate = 25,
            int GopSize = 12)
        {
            this.BitRate = BitRate;
            this.OutputWidth = OutputWidth;
            this.OutputHeight = OutputHeight;
            this.FrameUnitsInSec = FrameUnitsInSec;
            this.MaxFramerate = MaxFramerate;
            this.GopSize = GopSize;
        }
    };

    public static class CFFMPEGBroadcasting
    {
        private static bool logSet = false;
        public enum LogLevel
        {
            Info,
            Error
        };

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void LogCallback(LogLevel log_level, string message);

        public static void SetLogCallback(LogCallback log_callback)
        {
            if (!logSet)
            {
                SetLogCallbackInternal(log_callback);
                logSet = true;
            }
        }

        [DllImport("FFMPEGBroadcastDLL", CallingConvention = CallingConvention.Cdecl, EntryPoint = "SetLogCallback")]
        private extern static void SetLogCallbackInternal(LogCallback log_callback);

        [DllImport("FFMPEGBroadcastDLL", EntryPoint = "GetMicrosecondsTimeRelative")]
        public extern static Int64 GetMicrosecondsTimeRelative();

        public static class Transmitter
        {
            [DllImport("FFMPEGBroadcastDLL", CallingConvention = CallingConvention.Cdecl, EntryPoint = "Transmitter_Create")]
            public extern static IntPtr/* Transmitter* */ Create(string path, string format, string video_codec, int input_width, int input_height,
            TransmittingOptions options, int dict_count, string[] dict_keys, string[] dict_vals);

            [DllImport("FFMPEGBroadcastDLL", EntryPoint = "Transmitter_Destroy")]
            public extern static void Destroy(IntPtr/* Transmitter* */ transmitter);

            [DllImport("FFMPEGBroadcastDLL", EntryPoint = "Transmitter_ShellWriteVideoNow")]
            [return: MarshalAs(UnmanagedType.I1)]
            public extern static bool ShellWriteVideoNow(IntPtr/* Transmitter* */ transmitter, Int64 time_diff);

            [DllImport("FFMPEGBroadcastDLL", EntryPoint = "Transmitter_WriteVideoFrame")]
            [return: MarshalAs(UnmanagedType.I1)]
            public extern static bool WriteVideoFrame(IntPtr/* Transmitter* */ transmitter, Int64 time_diff, int input_width, int input_height, byte[] rgb_data);
        }

        public static class Receiver
        {
            [DllImport("FFMPEGBroadcastDLL", CallingConvention = CallingConvention.Cdecl, EntryPoint = "Receiver_Create")]
            public extern static IntPtr/* Receiver* */ Create(string path);

            [DllImport("FFMPEGBroadcastDLL", EntryPoint = "Receiver_GetUpdatedFrame")]
            [return: MarshalAs(UnmanagedType.I1)]
            public extern static bool GetUpdatedFrame(IntPtr/* Receiver* */ receiver, int output_width, int output_height, byte[] rgb_data);

            [DllImport("FFMPEGBroadcastDLL", EntryPoint = "Receiver_Destroy")]
            public extern static void Destroy(IntPtr/* Receiver* */ receiver);
        }
    }
}