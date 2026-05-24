using System.Runtime.InteropServices;

namespace PixelEngine {
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3 {
        public float X;
        public float Y;
        public float Z;

        public Vector3(float x, float y, float z) {
            X = x;
            Y = y;
            Z = z;
        }

        public override string ToString() => $"({X}, {Y}, {Z})";
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4 {
        public float X;
        public float Y;
        public float Z;
        public float W;

        public Vector4(float x, float y, float z, float w) {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public override string ToString() => $"({X}, {Y}, {Z}, {W})";
    }
}
