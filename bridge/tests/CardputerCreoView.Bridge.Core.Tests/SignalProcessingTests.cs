using System.Numerics;
using CardputerCreoView.Bridge.Core;
using Xunit;

namespace CardputerCreoView.Bridge.Core.Tests;

public sealed class SignalProcessingTests
{
    [Fact]
    public void SequenceTrackerHandlesWrapAndGap()
    {
        var tracker = new SequenceLossTracker();

        Assert.Equal(0, tracker.Observe(65534));
        Assert.Equal(0, tracker.Observe(65535));
        Assert.Equal(0, tracker.Observe(0));
        Assert.Equal(1, tracker.Observe(2));
    }

    [Fact]
    public void QuaternionRelativeToAnchorIsNormalized()
    {
        var anchor = Quaternion.CreateFromAxisAngle(Vector3.UnitX, MathF.PI / 2f);
        var current = Quaternion.CreateFromAxisAngle(Vector3.UnitX, MathF.PI);

        var relative = QuaternionMath.RelativeToAnchor(current, anchor);

        Assert.Equal(1f, relative.Length(), 5);
        Assert.Equal(MathF.PI / 2f, QuaternionMath.ToRotationVector(relative).X, 4);
    }

    [Fact]
    public void MahonyIntegratesGyroAndKeepsUnitQuaternion()
    {
        var filter = new MahonyFilter();
        for (var i = 0; i < 100; i++)
        {
            filter.Update(new Vector3(MathF.PI / 2f, 0f, 0f), Vector3.Zero, 0.01f);
        }

        Assert.Equal(1f, filter.Orientation.Length(), 5);
        Assert.Equal(MathF.PI / 2f, QuaternionMath.ToRotationVector(filter.Orientation).X, 3);
    }

    [Fact]
    public void SmallTiltInsideDeadZoneProducesZeroVelocity()
    {
        var orientation = Quaternion.CreateFromAxisAngle(Vector3.UnitX, MathF.PI / 180f);

        var velocity = MotionMapping.PanVelocityFromTilt(orientation);

        Assert.Equal(Vector3.Zero, velocity);
    }

    [Fact]
    public void TiltVelocityIsClampedToUnitRange()
    {
        var orientation = Quaternion.CreateFromAxisAngle(Vector3.UnitY, -MathF.PI / 2f);

        var velocity = MotionMapping.PanVelocityFromTilt(orientation);

        Assert.Equal(-1f, velocity.Y);
        Assert.InRange(velocity.X, -1f, 1f);
        Assert.InRange(velocity.Z, -1f, 1f);
    }

    [Fact]
    public void AxisMappingIsIndependentAndDefaultIsIdentity()
    {
        var input = new Vector3(1f, 2f, 3f);
        var mapping = new AxisMapping(new AxisSelection(2, -1f), new AxisSelection(0, 1f), new AxisSelection(1, -1f));

        Assert.Equal(new Vector3(-3f, 1f, -2f), SensorAxisMapper.Map(input, mapping));
        Assert.Equal(input, SensorAxisMapper.Map(input, AxisMapping.UncalibratedDefault));
    }

    [Fact]
    public void CalibrationRestartsAfterMovement()
    {
        var calibrator = new GyroBiasCalibrator();
        calibrator.Reset();

        Assert.False(calibrator.Add(Vector3.Zero, Vector3.UnitZ, 0));
        Assert.False(calibrator.Add(Vector3.Zero, Vector3.UnitZ, 1000));
        Assert.False(calibrator.Add(new Vector3(30f, 0f, 0f), Vector3.UnitZ, 1500));
        Assert.False(calibrator.Add(Vector3.Zero, Vector3.UnitZ, 2000));
        Assert.False(calibrator.Add(Vector3.Zero, Vector3.UnitZ, 3999));
        Assert.True(calibrator.Add(Vector3.Zero, Vector3.UnitZ, 4000));
    }
}
