using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public static class QuaternionMath
{
    public static Quaternion Multiply(Quaternion left, Quaternion right)
    {
        return new Quaternion(
            left.W * right.X + left.X * right.W + left.Y * right.Z - left.Z * right.Y,
            left.W * right.Y - left.X * right.Z + left.Y * right.W + left.Z * right.X,
            left.W * right.Z + left.X * right.Y - left.Y * right.X + left.Z * right.W,
            left.W * right.W - left.X * right.X - left.Y * right.Y - left.Z * right.Z);
    }

    public static Quaternion NormalizeOrIdentity(Quaternion value)
    {
        var lengthSquared = value.X * value.X + value.Y * value.Y + value.Z * value.Z + value.W * value.W;
        if (!float.IsFinite(lengthSquared) || lengthSquared < 1e-12f)
        {
            return Quaternion.Identity;
        }

        var scale = 1f / MathF.Sqrt(lengthSquared);
        return new Quaternion(value.X * scale, value.Y * scale, value.Z * scale, value.W * scale);
    }

    public static Quaternion RelativeToAnchor(Quaternion current, Quaternion anchor)
    {
        var normalizedAnchor = NormalizeOrIdentity(anchor);
        return NormalizeOrIdentity(Multiply(NormalizeOrIdentity(current), Quaternion.Conjugate(normalizedAnchor)));
    }

    public static Vector3 Rotate(Quaternion rotation, Vector3 vector)
    {
        var normalized = NormalizeOrIdentity(rotation);
        var rotated = Multiply(Multiply(normalized, new Quaternion(vector, 0f)), Quaternion.Conjugate(normalized));
        return new Vector3(rotated.X, rotated.Y, rotated.Z);
    }

    public static Vector3 ToRotationVector(Quaternion value)
    {
        var quaternion = NormalizeOrIdentity(value);
        if (quaternion.W < 0f)
        {
            quaternion = new Quaternion(-quaternion.X, -quaternion.Y, -quaternion.Z, -quaternion.W);
        }

        var w = Math.Clamp(quaternion.W, -1f, 1f);
        var sine = MathF.Sqrt(MathF.Max(0f, 1f - w * w));
        var vector = new Vector3(quaternion.X, quaternion.Y, quaternion.Z);
        if (sine < 1e-6f)
        {
            return vector * 2f;
        }

        return vector * (2f * MathF.Acos(w) / sine);
    }
}
