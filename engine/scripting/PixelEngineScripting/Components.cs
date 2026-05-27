namespace PixelEngine {
    public abstract class Component {
        public Entity Entity => InstanceEntity;
        internal Entity InstanceEntity { get; set; } = null!;

        public bool Enabled {
            get => InternalCalls.GetComponentEnabled(Entity.ID, GetComponentType());
            set => InternalCalls.SetComponentEnabled(Entity.ID, GetComponentType(), value);
        }

        protected abstract int GetComponentType();

        public bool HasComponent<T>() where T : Component, new() => Entity.HasComponent<T>();
        public T AddComponent<T>() where T : Component, new() => Entity.AddComponent<T>();
        public T GetComponent<T>() where T : Component, new() => Entity.GetComponent<T>();
    }

    public class TransformComponent : Component {
        protected override int GetComponentType() => 0;

        public Vector3 Translation {
            get {
                InternalCalls.GetTransform(Entity.ID, out Vector3 trans, out _, out _);
                return trans;
            }
            set {
                InternalCalls.GetTransform(Entity.ID, out _, out Vector3 rot, out Vector3 scale);
                InternalCalls.SetTransform(Entity.ID, ref value, ref rot, ref scale);
            }
        }

        public Vector3 Rotation {
            get {
                InternalCalls.GetTransform(Entity.ID, out _, out Vector3 rot, out _);
                return rot;
            }
            set {
                InternalCalls.GetTransform(Entity.ID, out Vector3 trans, out _, out Vector3 scale);
                InternalCalls.SetTransform(Entity.ID, ref trans, ref value, ref scale);
            }
        }

        public Vector3 Scale {
            get {
                InternalCalls.GetTransform(Entity.ID, out _, out _, out Vector3 scale);
                return scale;
            }
            set {
                InternalCalls.GetTransform(Entity.ID, out Vector3 trans, out Vector3 rot, out _);
                InternalCalls.SetTransform(Entity.ID, ref trans, ref rot, ref value);
            }
        }
    }

    public class SpriteRendererComponent : Component {
        protected override int GetComponentType() => 1;
    }

    public class MeshRendererComponent : Component {
        protected override int GetComponentType() => 2;
    }

    public class VelocityComponent : Component {
        protected override int GetComponentType() => 3;

        public Vector3 Linear {
            get {
                InternalCalls.GetVelocity(Entity.ID, out Vector3 lin, out _);
                return lin;
            }
            set {
                InternalCalls.GetVelocity(Entity.ID, out _, out Vector3 ang);
                InternalCalls.SetVelocity(Entity.ID, ref value, ref ang);
            }
        }

        public Vector3 Angular {
            get {
                InternalCalls.GetVelocity(Entity.ID, out _, out Vector3 ang);
                return ang;
            }
            set {
                InternalCalls.GetVelocity(Entity.ID, out Vector3 lin, out _);
                InternalCalls.SetVelocity(Entity.ID, ref lin, ref value);
            }
        }
    }
}
