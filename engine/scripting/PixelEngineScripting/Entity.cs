using System;

namespace PixelEngine {
    public class Entity {
        public ulong ID { get; }

        protected Entity() {
            ID = 0;
        }

        internal Entity(ulong id) {
            ID = id;
        }

        public bool HasComponent<T>() where T : Component, new() {
            int compType = GetComponentType<T>();
            return InternalCalls.HasComponent(ID, compType);
        }

        public T AddComponent<T>() where T : Component, new() {
            int compType = GetComponentType<T>();
            InternalCalls.AddComponent(ID, compType);
            return GetComponent<T>();
        }

        public T GetComponent<T>() where T : Component, new() {
            if (!HasComponent<T>()) {
                throw new InvalidOperationException($"Entity does not have component {typeof(T).Name}");
            }
            T component = new T { InstanceEntity = this };
            return component;
        }

        private int GetComponentType<T>() {
            if (typeof(T) == typeof(TransformComponent)) return 0;
            if (typeof(T) == typeof(SpriteRendererComponent)) return 1;
            if (typeof(T) == typeof(MeshRendererComponent)) return 2;
            if (typeof(T) == typeof(VelocityComponent)) return 3;
            throw new NotSupportedException($"Component type {typeof(T).Name} is not supported.");
        }
    }
}
