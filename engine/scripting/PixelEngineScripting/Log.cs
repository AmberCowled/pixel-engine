using System;
using System.Runtime.InteropServices;

namespace PixelEngine {
    public static class Log {
        public static void Info(string message) {
            IntPtr ptr = Marshal.StringToHGlobalAnsi(message);
            InternalCalls.Log(0, ptr);
            Marshal.FreeHGlobal(ptr);
        }

        public static void Warn(string message) {
            IntPtr ptr = Marshal.StringToHGlobalAnsi(message);
            InternalCalls.Log(1, ptr);
            Marshal.FreeHGlobal(ptr);
        }

        public static void Error(string message) {
            IntPtr ptr = Marshal.StringToHGlobalAnsi(message);
            InternalCalls.Log(2, ptr);
            Marshal.FreeHGlobal(ptr);
        }
    }
}
