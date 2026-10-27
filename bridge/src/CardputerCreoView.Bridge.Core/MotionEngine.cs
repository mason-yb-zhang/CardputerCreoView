using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public sealed class MotionEngine
{
    public const int PacketTimeoutMilliseconds = 150;
    private readonly AxisMapping _axisMapping;
    private readonly MahonyFilter _filter = new();
    private readonly GyroBiasCalibrator _calibrator = new();
    private readonly SequenceLossTracker _sequenceLoss = new();
    private readonly PacketRateMeter _rate = new();
    private Vector3 _gyroscopeBiasDps;
    private Quaternion _anchor = Quaternion.Identity;
    private Quaternion _relativeOrientation = Quaternion.Identity;
    private Vector3 _panVelocity;
    private float _zoomVelocity;
    private TelemetryButtons _previousButtons;
    private long? _lastPacketHostMilliseconds;
    private long? _lastFilterHostMilliseconds;
    private uint _sequence;
    private bool _outputRequested;
    private bool _connected;
    private bool _calibrated;
    private bool _calibrating;
    private bool _motionValid;
    private bool _grabActive;
    private bool _translateActive;
    private bool _requireRelease;

    public MotionEngine(AxisMapping? axisMapping = null, uint? sessionId = null)
    {
        _axisMapping = axisMapping ?? AxisMapping.UncalibratedDefault;
        SessionId = sessionId ?? (uint)Random.Shared.NextInt64(1, (long)uint.MaxValue + 1);
    }

    public uint SessionId { get; }
    public uint GrabId { get; private set; }
    public long ReceivedPackets { get; private set; }
    public long DroppedPackets { get; private set; }
    public bool IsConnected => _connected;
    public bool IsCalibrated => _calibrated;
    public bool IsCalibrating => _calibrating;
    public bool IsOutputRequested => _outputRequested;
    public bool IsGrabActive => _grabActive;
    public bool IsTranslateActive => _translateActive;
    public float CalibrationProgress => _calibrating ? _calibrator.Progress : _calibrated ? 1f : 0f;
    public double PacketRateHz => _rate.RateHz;
    public Vector3 RawGyroscopeDps { get; private set; }
    public Vector3 RawAccelerationG { get; private set; }
    public Vector3 RelativeRotationDegrees =>
        QuaternionMath.ToRotationVector(_relativeOrientation) * (180f / MathF.PI);

    public MotionFrame ProcessPacket(TelemetryPacket packet, long hostMilliseconds)
    {
        ReceivedPackets++;
        RawGyroscopeDps = packet.GyroscopeDps;
        RawAccelerationG = packet.AccelerationG;
        DroppedPackets += _sequenceLoss.Observe(packet.Sequence);
        _rate.Observe(hostMilliseconds);
        _lastPacketHostMilliseconds = hostMilliseconds;
        _motionValid = true;
        if (_requireRelease && !HasButton(packet.Buttons, TelemetryButtons.Grab))
        {
            _requireRelease = false;
        }

        var mapped = SensorAxisMapper.Map(packet, _axisMapping);
        var calibrateEdge = HasButton(packet.Buttons, TelemetryButtons.Calibrate) &&
                            !HasButton(_previousButtons, TelemetryButtons.Calibrate);
        if (calibrateEdge && !_calibrating)
        {
            StartCalibration(hostMilliseconds);
        }

        if (_calibrating)
        {
            if (_calibrator.Add(mapped.GyroscopeDps, mapped.AccelerationG, hostMilliseconds))
            {
                _gyroscopeBiasDps = _calibrator.BiasDps;
                _calibrated = true;
                _calibrating = false;
                _filter.Reset();
                _anchor = Quaternion.Identity;
                _lastFilterHostMilliseconds = hostMilliseconds;
            }

            _grabActive = false;
            _translateActive = false;
            _relativeOrientation = Quaternion.Identity;
            _panVelocity = Vector3.Zero;
            _zoomVelocity = 0f;
            _previousButtons = packet.Buttons;
            return BuildFrame(hostMilliseconds);
        }

        if (!_calibrated)
        {
            _previousButtons = packet.Buttons;
            _relativeOrientation = Quaternion.Identity;
            _panVelocity = Vector3.Zero;
            _zoomVelocity = 0f;
            return BuildFrame(hostMilliseconds);
        }

        UpdateOrientation(mapped, hostMilliseconds);
        UpdateGrabState(packet.Buttons);
        _previousButtons = packet.Buttons;
        return BuildFrame(hostMilliseconds);
    }

    public MotionFrame StartCalibration(long hostMilliseconds)
    {
        _calibrated = false;
        _calibrating = true;
        _calibrator.Reset();
        _gyroscopeBiasDps = Vector3.Zero;
        InvalidateMotion();
        return BuildFrame(hostMilliseconds);
    }

    public MotionFrame Recenter(long hostMilliseconds)
    {
        Reanchor();
        _panVelocity = Vector3.Zero;
        return BuildFrame(hostMilliseconds);
    }

    public MotionFrame SetOutputRequested(bool requested, long hostMilliseconds)
    {
        _outputRequested = requested;
        if (!requested)
        {
            _requireRelease = true;
            _grabActive = false;
            _translateActive = false;
            _relativeOrientation = Quaternion.Identity;
            _panVelocity = Vector3.Zero;
            _zoomVelocity = 0f;
            _previousButtons = TelemetryButtons.None;
        }
        return BuildFrame(hostMilliseconds);
    }

    public MotionFrame? SetConnected(bool connected, long hostMilliseconds)
    {
        if (_connected == connected)
        {
            return null;
        }

        _connected = connected;
        if (connected)
        {
            _lastPacketHostMilliseconds = null;
            _motionValid = false;
            ResetPacketTracking();
            return null;
        }

        InvalidateMotion();
        return BuildFrame(hostMilliseconds);
    }

    public MotionFrame? Tick(long hostMilliseconds)
    {
        if (_motionValid && (!_connected || !IsFresh(hostMilliseconds)))
        {
            InvalidateMotion();
            return BuildFrame(hostMilliseconds);
        }

        return null;
    }

    public MotionFrame Disable(long hostMilliseconds)
    {
        InvalidateMotion();
        return BuildFrame(hostMilliseconds);
    }

    public bool IsOutputActive(long hostMilliseconds)
    {
        return _outputRequested && _calibrated && !_calibrating && _connected && _motionValid && IsFresh(hostMilliseconds);
    }

    public void ResetPacketTracking()
    {
        ReceivedPackets = 0;
        DroppedPackets = 0;
        _sequenceLoss.Reset();
        _rate.Reset();
    }

    private void UpdateOrientation(TelemetryPacket packet, long hostMilliseconds)
    {
        if (_lastFilterHostMilliseconds is long last)
        {
            var deltaSeconds = Math.Clamp((hostMilliseconds - last) / 1000f, 0f, 0.1f);
            var gyroscope = (packet.GyroscopeDps - _gyroscopeBiasDps) * (MathF.PI / 180f);
            _filter.Update(gyroscope, packet.AccelerationG, deltaSeconds);
        }

        _lastFilterHostMilliseconds = hostMilliseconds;
    }

    private void UpdateGrabState(TelemetryButtons buttons)
    {
        var grabPressed = HasButton(buttons, TelemetryButtons.Grab);
        var translatePressed = HasButton(buttons, TelemetryButtons.Translate);
        if (_requireRelease)
        {
            _grabActive = false;
            _translateActive = false;
            _relativeOrientation = Quaternion.Identity;
            _panVelocity = Vector3.Zero;
            _zoomVelocity = (HasButton(buttons, TelemetryButtons.ZoomIn) ? 1f : 0f) +
                            (HasButton(buttons, TelemetryButtons.ZoomOut) ? -1f : 0f);
            return;
        }

        var recenterEdge = HasButton(buttons, TelemetryButtons.Recenter) &&
                           !HasButton(_previousButtons, TelemetryButtons.Recenter);
        var reanchor = false;

        if (!grabPressed)
        {
            _grabActive = false;
            _translateActive = false;
        }
        else
        {
            if (!_grabActive)
            {
                _grabActive = true;
                _translateActive = translatePressed;
                reanchor = true;
            }

            if (_translateActive != translatePressed)
            {
                _translateActive = translatePressed;
                reanchor = true;
            }
        }

        if (recenterEdge)
        {
            reanchor = true;
        }

        if (reanchor)
        {
            Reanchor();
        }

        if (_grabActive)
        {
            _relativeOrientation = QuaternionMath.RelativeToAnchor(_filter.Orientation, _anchor);
            _panVelocity = _translateActive ? MotionMapping.PanVelocityFromTilt(_relativeOrientation) : Vector3.Zero;
        }
        else
        {
            _relativeOrientation = Quaternion.Identity;
            _panVelocity = Vector3.Zero;
        }

        _zoomVelocity = (HasButton(buttons, TelemetryButtons.ZoomIn) ? 1f : 0f) +
                        (HasButton(buttons, TelemetryButtons.ZoomOut) ? -1f : 0f);
    }

    private void Reanchor()
    {
        if (_requireRelease)
        {
            return;
        }

        GrabId = unchecked(GrabId + 1);
        _anchor = QuaternionMath.NormalizeOrIdentity(_filter.Orientation);
        _relativeOrientation = Quaternion.Identity;
        _panVelocity = Vector3.Zero;
    }

    private void InvalidateMotion()
    {
        _outputRequested = false;
        _requireRelease = true;
        _motionValid = false;
        _grabActive = false;
        _translateActive = false;
        _previousButtons = TelemetryButtons.None;
        _anchor = Quaternion.Identity;
        _relativeOrientation = Quaternion.Identity;
        _panVelocity = Vector3.Zero;
        _zoomVelocity = 0f;
        _filter.Reset();
        _lastFilterHostMilliseconds = null;
    }

    private MotionFrame BuildFrame(long hostMilliseconds)
    {
        var enabled = IsOutputActive(hostMilliseconds);
        var flags = enabled ? MotionFlags.Enable : MotionFlags.None;
        var quaternion = Quaternion.Identity;
        var pan = Vector3.Zero;
        var zoom = 0f;

        if (enabled)
        {
            zoom = _zoomVelocity;
            if (_grabActive)
            {
                flags |= MotionFlags.Grab;
                quaternion = QuaternionMath.NormalizeOrIdentity(_relativeOrientation);
                if (_translateActive)
                {
                    flags |= MotionFlags.Translate;
                    pan = _panVelocity;
                }
            }
        }

        var frame = new MotionFrame(flags, _sequence, unchecked((ulong)hostMilliseconds), quaternion, pan, zoom, GrabId, SessionId);
        _sequence = unchecked(_sequence + 1);
        return frame;
    }

    private bool IsFresh(long hostMilliseconds)
    {
        if (_lastPacketHostMilliseconds is not long last)
        {
            return false;
        }

        var elapsed = hostMilliseconds - last;
        return elapsed >= 0 && elapsed < PacketTimeoutMilliseconds;
    }

    private static bool HasButton(TelemetryButtons buttons, TelemetryButtons button)
    {
        return (buttons & button) != 0;
    }
}
