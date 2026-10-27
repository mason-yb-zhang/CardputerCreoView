using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public static class MotionMapping
{
    public const float FullScaleTiltRadians = MathF.PI / 4f;
    public const float DefaultDeadZone = 0.12f;

    public static Vector3 PanVelocityFromTilt(Quaternion relativeOrientation, float deadZone = DefaultDeadZone)
    {
        var rotation = QuaternionMath.ToRotationVector(relativeOrientation) / FullScaleTiltRadians;
        return new Vector3(
            ApplyDeadZoneAndClamp(rotation.X, deadZone),
            ApplyDeadZoneAndClamp(rotation.Y, deadZone),
            ApplyDeadZoneAndClamp(rotation.Z, deadZone));
    }

    public static float ApplyDeadZoneAndClamp(float value, float deadZone = DefaultDeadZone)
    {
        if (deadZone is < 0f or >= 1f)
        {
            throw new ArgumentOutOfRangeException(nameof(deadZone));
        }

        value = Math.Clamp(value, -1f, 1f);
        var magnitude = MathF.Abs(value);
        if (magnitude <= deadZone)
        {
            return 0f;
        }

        return MathF.CopySign((magnitude - deadZone) / (1f - deadZone), value);
    }
}
