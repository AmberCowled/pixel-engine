using System;
using System.Runtime.InteropServices;

namespace PixelEngine {
    internal static class InternalCalls {
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void LogCallback(int level, IntPtr message);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate bool GetTransformCallback(ulong entityID, out Vector3 translation, out Vector3 rotation, out Vector3 scale);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void SetTransformCallback(ulong entityID, ref Vector3 translation, ref Vector3 rotation, ref Vector3 scale);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate bool HasComponentCallback(ulong entityID, int componentType);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void AddComponentCallback(ulong entityID, int componentType);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate bool GetVelocityCallback(ulong entityID, out Vector3 linear, out Vector3 angular);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void SetVelocityCallback(ulong entityID, ref Vector3 linear, ref Vector3 angular);

        public static LogCallback Log = null!;
        public static GetTransformCallback GetTransform = null!;
        public static SetTransformCallback SetTransform = null!;
        public static HasComponentCallback HasComponent = null!;
        public static AddComponentCallback AddComponent = null!;
        public static GetVelocityCallback GetVelocity = null!;
        public static SetVelocityCallback SetVelocity = null!;
    }
}
