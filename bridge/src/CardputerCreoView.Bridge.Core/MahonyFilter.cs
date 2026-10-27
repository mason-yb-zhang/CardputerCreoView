using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public sealed class MahonyFilter
{
    private Vector3 _integralError;

    public float ProportionalGain { get; init; } = 2f;
    public float IntegralGain { get; init; } = 0.05f;
    public Quaternion Orientation { get; private set; } = Quaternion.Identity;

    public void Reset()
    {
        Orientation = Quaternion.Identity;
        _integralError = Vector3.Zero;
    }

    public void Update(Vector3 gyroscopeRadiansPerSecond, Vector3 accelerationG, float deltaSeconds)
    {
        if (deltaSeconds <= 0f || !float.IsFinite(deltaSeconds) || !IsFinite(gyroscopeRadiansPerSecond))
        {
            return;
        }

        var correctedGyroscope = gyroscopeRadiansPerSecond;
        var accelerationLengthSquared = accelerationG.LengthSquared();
        if (float.IsFinite(accelerationLengthSquared) && accelerationLengthSquared > 1e-8f)
        {
            var measuredUp = accelerationG / MathF.Sqrt(accelerationLengthSquared);
            var estimatedUp = QuaternionMath.Rotate(Quaternion.Conjugate(Orientation), Vector3.UnitZ);
            var error = Vector3.Cross(measuredUp, estimatedUp);
            _integralError += error * deltaSeconds;
            _integralError = Vector3.Clamp(_integralError, new Vector3(-1f), new Vector3(1f));
            correctedGyroscope += ProportionalGain * error + IntegralGain * _integralError;
        }

        var derivative = QuaternionMath.Multiply(Orientation, new Quaternion(correctedGyroscope * 0.5f, 0f));
        var next = new Quaternion(
            Orientation.X + derivative.X * deltaSeconds,
            Orientation.Y + derivative.Y * deltaSeconds,
            Orientation.Z + derivative.Z * deltaSeconds,
            Orientation.W + derivative.W * deltaSeconds);
        Orientation = QuaternionMath.NormalizeOrIdentity(next);
    }

    private static bool IsFinite(Vector3 value)
    {
        return float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);
    }
}
