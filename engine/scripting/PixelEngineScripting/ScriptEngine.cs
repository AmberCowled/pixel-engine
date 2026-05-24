using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

namespace PixelEngine {
    public static class ScriptEngine {
        private static Assembly? s_GameAssembly = null;
        private static Dictionary<ulong, ScriptableEntity> s_Instances = new Dictionary<ulong, ScriptableEntity>();

        [UnmanagedCallersOnly]
        public static void Initialize(
            IntPtr logCallback,
            IntPtr getTransformCallback,
            IntPtr setTransformCallback,
            IntPtr hasComponentCallback,
            IntPtr addComponentCallback,
            IntPtr getVelocityCallback,
            IntPtr setVelocityCallback
        ) {
            InternalCalls.Log = Marshal.GetDelegateForFunctionPointer<InternalCalls.LogCallback>(logCallback);
            InternalCalls.GetTransform = Marshal.GetDelegateForFunctionPointer<InternalCalls.GetTransformCallback>(getTransformCallback);
            InternalCalls.SetTransform = Marshal.GetDelegateForFunctionPointer<InternalCalls.SetTransformCallback>(setTransformCallback);
            InternalCalls.HasComponent = Marshal.GetDelegateForFunctionPointer<InternalCalls.HasComponentCallback>(hasComponentCallback);
            InternalCalls.AddComponent = Marshal.GetDelegateForFunctionPointer<InternalCalls.AddComponentCallback>(addComponentCallback);
            InternalCalls.GetVelocity = Marshal.GetDelegateForFunctionPointer<InternalCalls.GetVelocityCallback>(getVelocityCallback);
            InternalCalls.SetVelocity = Marshal.GetDelegateForFunctionPointer<InternalCalls.SetVelocityCallback>(setVelocityCallback);

            InternalCalls.Log(0, Marshal.StringToHGlobalAnsi("ScriptEngine successfully initialized from C++!"));
        }

        [UnmanagedCallersOnly]
        public static int LoadAssembly(IntPtr assemblyPathPtr) {
            string? path = Marshal.PtrToStringAnsi(assemblyPathPtr);
            if (string.IsNullOrEmpty(path)) return 0;

            try {
                s_GameAssembly = Assembly.LoadFrom(path);
                InternalCalls.Log(0, Marshal.StringToHGlobalAnsi($"Loaded game assembly: {s_GameAssembly.FullName}"));
                return 1;
            } catch (Exception e) {
                InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Failed to load game assembly: {e.Message}"));
                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static int CreateInstance(ulong entityID, IntPtr classNamePtr) {
            string? className = Marshal.PtrToStringAnsi(classNamePtr);
            if (string.IsNullOrEmpty(className)) return 0;

            if (s_GameAssembly == null) {
                InternalCalls.Log(2, Marshal.StringToHGlobalAnsi("Cannot create script instance: No game assembly loaded."));
                return 0;
            }

            try {
                Type? type = s_GameAssembly.GetType(className);
                if (type == null) {
                    InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Could not find script class {className} in assembly."));
                    return 0;
                }

                if (!typeof(ScriptableEntity).IsAssignableFrom(type)) {
                    InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Class {className} does not inherit from ScriptableEntity."));
                    return 0;
                }

                var instance = Activator.CreateInstance(type) as ScriptableEntity;
                if (instance == null) return 0;

                var backingField = typeof(Entity).GetField("<ID>k__BackingField", BindingFlags.NonPublic | BindingFlags.Instance);
                if (backingField != null) {
                    backingField.SetValue(instance, entityID);
                }

                s_Instances[entityID] = instance;
                return 1;
            } catch (Exception e) {
                InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Failed to create instance of {className}: {e.Message}"));
                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static void DestroyInstance(ulong entityID) {
            s_Instances.Remove(entityID);
        }

        [UnmanagedCallersOnly]
        public static void OnCreate(ulong entityID) {
            if (s_Instances.TryGetValue(entityID, out var instance)) {
                try {
                    instance.OnCreate();
                } catch (Exception e) {
                    InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Error in OnCreate: {e.Message}"));
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnUpdate(ulong entityID, float dt) {
            if (s_Instances.TryGetValue(entityID, out var instance)) {
                try {
                    instance.OnUpdate(dt);
                } catch (Exception e) {
                    InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Error in OnUpdate: {e.Message}"));
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void Reset() {
            s_Instances.Clear();
        }

        [UnmanagedCallersOnly]
        public static void OnCollisionEnter(ulong entityID, ulong otherID) {
            if (s_Instances.TryGetValue(entityID, out var instance)) {
                try {
                    instance.OnCollisionEnter(otherID);
                } catch (Exception e) {
                    InternalCalls.Log(2, Marshal.StringToHGlobalAnsi($"Error in OnCollisionEnter: {e.Message}"));
                }
            }
        }
    }
}
