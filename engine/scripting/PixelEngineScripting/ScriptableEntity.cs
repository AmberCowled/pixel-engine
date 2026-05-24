namespace PixelEngine {
    public class ScriptableEntity : Entity {
        public virtual void OnCreate() {}
        public virtual void OnUpdate(float dt) {}
        public virtual void OnCollisionEnter(ulong otherID) {}
    }
}
