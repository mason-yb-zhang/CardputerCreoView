using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public sealed class GyroBiasCalibrator
{
    public const int DurationMilliseconds = 2000;
    private const float MaximumStationaryGyroDps = 8f;
    private const float MaximumAccelerationErrorG = 0.15f;
    private long _startedHostMilliseconds;
    private long _elapsedHostMilliseconds;
    private Vector3 _gyroscopeSum;
    private int _count;

    public Vector3 BiasDps { get; private set; }
    public float Progress => Math.Clamp(_elapsedHostMilliseconds / (float)DurationMilliseconds, 0f, 1f);

    public void Reset()
    {
        _startedHostMilliseconds = 0;
        _elapsedHostMilliseconds = 0;
        _gyroscopeSum = Vector3.Zero;
        _count = 0;
        BiasDps = Vector3.Zero;
    }

    public bool Add(Vector3 gyroscopeDps, Vector3 accelerationG, long hostMilliseconds)
    {
        if (!IsStationary(gyroscopeDps, accelerationG))
        {
            _gyroscopeSum = Vector3.Zero;
            _count = 0;
            _elapsedHostMilliseconds = 0;
            return false;
        }

        if (_count == 0)
        {
            _startedHostMilliseconds = hostMilliseconds;
        }

        _gyroscopeSum += gyroscopeDps;
        _count++;
        _elapsedHostMilliseconds = Math.Max(0, hostMilliseconds - _startedHostMilliseconds);
        if (_elapsedHostMilliseconds < DurationMilliseconds)
        {
            return false;
        }

        BiasDps = _gyroscopeSum / _count;
        return true;
    }

    private static bool IsStationary(Vector3 gyroscopeDps, Vector3 accelerationG)
    {
        var gyroscopeLength = gyroscopeDps.Length();
        var accelerationLength = accelerationG.Length();
        return float.IsFinite(gyroscopeLength) &&
               float.IsFinite(accelerationLength) &&
               gyroscopeLength <= MaximumStationaryGyroDps &&
               MathF.Abs(accelerationLength - 1f) <= MaximumAccelerationErrorG;
    }
}
