using System.Numerics;
using CardputerCreoView.Bridge.Core;
using Xunit;

namespace CardputerCreoView.Bridge.Core.Tests;

public sealed class MotionEngineTests
{
    [Fact]
    public void OutputDefaultsOffAndCannotEnableBeforeCalibration()
    {
        var engine = new MotionEngine(sessionId: 7);
        engine.SetConnected(true, 0);

        var frame = engine.ProcessPacket(Packet(1), 0);

        Assert.False(frame.Flags.HasFlag(MotionFlags.Enable));
        Assert.Equal(7u, frame.SessionId);
        Assert.False(engine.SetOutputRequested(true, 10).Flags.HasFlag(MotionFlags.Enable));
    }

    [Fact]
    public void GrabReleaseAndRegrabChangeIdsAndEmitIdentityOnRelease()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);

        var grabbed = engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);
        var released = engine.ProcessPacket(Packet(4), 2020);
        var regrabbed = engine.ProcessPacket(Packet(5, TelemetryButtons.Grab), 2030);

        Assert.Equal(1u, grabbed.GrabId);
        Assert.True(grabbed.Flags.HasFlag(MotionFlags.Enable | MotionFlags.Grab));
        Assert.False(released.Flags.HasFlag(MotionFlags.Grab));
        Assert.Equal(Quaternion.Identity, released.Quaternion);
        Assert.Equal(2u, regrabbed.GrabId);
    }

    [Fact]
    public void RecenterIncrementsIdAndAnchorsCurrentOrientation()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);
        engine.ProcessPacket(Packet(4, TelemetryButtons.Grab, new Vector3(90f, 0f, 0f)), 2020);

        var recentered = engine.Recenter(2030);

        Assert.Equal(2u, recentered.GrabId);
        Assert.Equal(Quaternion.Identity, recentered.Quaternion);
        Assert.Equal(1f, recentered.Quaternion.Length(), 5);
    }

    [Fact]
    public void TranslateChangesDuringGrabReanchorExactlyOnce()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);

        var translating = engine.ProcessPacket(Packet(4, TelemetryButtons.Grab | TelemetryButtons.Translate), 2020);
        var rotating = engine.ProcessPacket(Packet(5, TelemetryButtons.Grab), 2030);

        Assert.Equal(2u, translating.GrabId);
        Assert.Equal(Quaternion.Identity, translating.Quaternion);
        Assert.True(translating.Flags.HasFlag(MotionFlags.Translate));
        Assert.Equal(3u, rotating.GrabId);
        Assert.Equal(Quaternion.Identity, rotating.Quaternion);
        Assert.False(rotating.Flags.HasFlag(MotionFlags.Translate));
    }

    [Fact]
    public void DeviceRecenterAndTranslateChangeShareOneIncrement()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);

        var frame = engine.ProcessPacket(
            Packet(4, TelemetryButtons.Grab | TelemetryButtons.Translate | TelemetryButtons.Recenter),
            2020);

        Assert.Equal(2u, frame.GrabId);
        Assert.Equal(Quaternion.Identity, frame.Quaternion);
    }

    [Fact]
    public void TimeoutRequiresExplicitOutputReenable()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);

        Assert.Null(engine.Tick(2159));
        var disabled = engine.Tick(2160);
        Assert.NotNull(disabled);
        Assert.False(disabled.Value.Flags.HasFlag(MotionFlags.Enable));
        Assert.False(disabled.Value.Flags.HasFlag(MotionFlags.Grab));
        Assert.Equal(Quaternion.Identity, disabled.Value.Quaternion);
        Assert.False(engine.IsOutputRequested);
        Assert.Null(engine.Tick(2200));

        var resumed = engine.ProcessPacket(Packet(4, TelemetryButtons.Grab), 2201);
        Assert.False(resumed.Flags.HasFlag(MotionFlags.Enable));
        Assert.Equal(1u, resumed.GrabId);
        Assert.False(engine.IsGrabActive);
        Assert.Equal(Quaternion.Identity, resumed.Quaternion);
        var reenabled = engine.SetOutputRequested(true, 2202);
        Assert.True(reenabled.Flags.HasFlag(MotionFlags.Enable));
        Assert.False(reenabled.Flags.HasFlag(MotionFlags.Grab));
        engine.ProcessPacket(Packet(5), 2210);
        var regrabbed = engine.ProcessPacket(Packet(6, TelemetryButtons.Grab), 2220);
        Assert.Equal(2u, regrabbed.GrabId);
        Assert.True(regrabbed.Flags.HasFlag(MotionFlags.Enable | MotionFlags.Grab));
    }

    [Fact]
    public void DisconnectRequiresExplicitOutputReenable()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);

        var disabled = engine.SetConnected(false, 2020);

        Assert.NotNull(disabled);
        Assert.Equal(MotionFlags.None, disabled.Value.Flags);
        Assert.Equal(Quaternion.Identity, disabled.Value.Quaternion);
        Assert.False(engine.IsOutputRequested);
        engine.SetConnected(true, 2030);
        var resumed = engine.ProcessPacket(Packet(4, TelemetryButtons.Grab), 2040);
        Assert.False(resumed.Flags.HasFlag(MotionFlags.Enable));
        Assert.False(engine.IsGrabActive);
        Assert.Equal(1u, resumed.GrabId);
        var reenabled = engine.SetOutputRequested(true, 2041);
        Assert.False(reenabled.Flags.HasFlag(MotionFlags.Grab));
        engine.ProcessPacket(Packet(5), 2050);
        var regrabbed = engine.ProcessPacket(Packet(6, TelemetryButtons.Grab), 2060);
        Assert.Equal(2u, regrabbed.GrabId);
        Assert.True(regrabbed.Flags.HasFlag(MotionFlags.Enable | MotionFlags.Grab));
    }

    [Fact]
    public void StartCalibrationRevokesOutputAndDoesNotRestoreIt()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3), 2010);

        var disabled = engine.StartCalibration(2020);
        Assert.False(disabled.Flags.HasFlag(MotionFlags.Enable));
        Assert.False(engine.IsOutputRequested);
        engine.ProcessPacket(Packet(4), 2020);
        engine.ProcessPacket(Packet(5), 3020);
        var calibrated = engine.ProcessPacket(Packet(6), 4020);
        Assert.True(engine.IsCalibrated);
        Assert.False(calibrated.Flags.HasFlag(MotionFlags.Enable));
    }

    [Fact]
    public void DisableRevokesOutputAndRecoveryDoesNotRestoreIt()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3), 2010);

        var disabled = engine.Disable(2020);
        Assert.False(disabled.Flags.HasFlag(MotionFlags.Enable));
        Assert.False(engine.IsOutputRequested);
        var resumed = engine.ProcessPacket(Packet(4), 2030);
        Assert.False(resumed.Flags.HasFlag(MotionFlags.Enable));
    }

    [Fact]
    public void ManualDisableRequiresReleaseBeforeNewGrab()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);
        engine.ProcessPacket(Packet(3, TelemetryButtons.Grab), 2010);
        engine.SetOutputRequested(false, 2020);

        var reenabled = engine.SetOutputRequested(true, 2030);
        Assert.True(reenabled.Flags.HasFlag(MotionFlags.Enable));
        var held = engine.ProcessPacket(Packet(4, TelemetryButtons.Grab), 2040);
        Assert.False(held.Flags.HasFlag(MotionFlags.Grab));
        Assert.Equal(1u, held.GrabId);

        engine.ProcessPacket(Packet(5), 2050);
        var regrabbed = engine.ProcessPacket(Packet(6, TelemetryButtons.Grab), 2060);
        Assert.True(regrabbed.Flags.HasFlag(MotionFlags.Grab));
        Assert.Equal(2u, regrabbed.GrabId);
    }

    [Fact]
    public void ZoomButtonsProduceSignedVelocity()
    {
        var engine = CreateCalibratedEngine();
        engine.SetOutputRequested(true, 2000);

        Assert.Equal(1f, engine.ProcessPacket(Packet(3, TelemetryButtons.ZoomIn), 2010).ZoomVelocity);
        Assert.Equal(-1f, engine.ProcessPacket(Packet(4, TelemetryButtons.ZoomOut), 2020).ZoomVelocity);
        Assert.Equal(0f, engine.ProcessPacket(Packet(5, TelemetryButtons.ZoomIn | TelemetryButtons.ZoomOut), 2030).ZoomVelocity);
    }

    private static MotionEngine CreateCalibratedEngine()
    {
        var engine = new MotionEngine(sessionId: 123);
        engine.SetConnected(true, 0);
        engine.StartCalibration(0);
        engine.ProcessPacket(Packet(0), 0);
        engine.ProcessPacket(Packet(1), 1000);
        engine.ProcessPacket(Packet(2), 2000);
        Assert.True(engine.IsCalibrated);
        return engine;
    }

    private static TelemetryPacket Packet(ushort sequence, TelemetryButtons buttons = TelemetryButtons.None, Vector3? gyroscope = null)
    {
        return new TelemetryPacket(sequence, 0, gyroscope ?? Vector3.Zero, Vector3.UnitZ, buttons);
    }
}
