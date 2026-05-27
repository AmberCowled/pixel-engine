namespace PixelEngine {
    public class MonoBehaviour : Component {
        protected override int GetComponentType() => 9;

        public virtual void OnCreate() {}
        public virtual void OnUpdate(float dt) {}
        public virtual void OnCollisionEnter(ulong otherID) {}
    }
}
