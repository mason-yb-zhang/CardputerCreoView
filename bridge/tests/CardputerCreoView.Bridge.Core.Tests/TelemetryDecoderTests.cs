using System.Buffers.Binary;
using CardputerCreoView.Bridge.Core;
using Xunit;

namespace CardputerCreoView.Bridge.Core.Tests;

public sealed class TelemetryDecoderTests
{
    [Fact]
    public void DecodeMatchesFirmwareNativePacket()
    {
        Span<byte> bytes = stackalloc byte[TelemetryProtocol.PacketSize];
        BinaryPrimitives.WriteUInt16LittleEndian(bytes, 0x1234);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes[2..], 0x89ABCDEF);
        BinaryPrimitives.WriteInt16LittleEndian(bytes[6..], 10);
        BinaryPrimitives.WriteInt16LittleEndian(bytes[8..], -20);
        BinaryPrimitives.WriteInt16LittleEndian(bytes[10..], 32767);
        BinaryPrimitives.WriteInt16LittleEndian(bytes[12..], 1000);
        BinaryPrimitives.WriteInt16LittleEndian(bytes[14..], -500);
        BinaryPrimitives.WriteInt16LittleEndian(bytes[16..], -32768);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes[18..], 0x003F);

        Assert.True(TelemetryDecoder.TryDecode(bytes, out var packet));
        Assert.Equal(0x1234, packet.Sequence);
        Assert.Equal(0x89ABCDEF, packet.Timestamp);
        Assert.Equal(1f, packet.GyroscopeDps.X);
        Assert.Equal(-2f, packet.GyroscopeDps.Y);
        Assert.Equal(3276.7f, packet.GyroscopeDps.Z);
        Assert.Equal(1f, packet.AccelerationG.X);
        Assert.Equal(-0.5f, packet.AccelerationG.Y);
        Assert.Equal(-32.768f, packet.AccelerationG.Z);
        Assert.Equal((TelemetryButtons)0x3F, packet.Buttons);
    }

    [Theory]
    [InlineData(0)]
    [InlineData(19)]
    [InlineData(21)]
    public void DecodeRejectsAnyOtherLength(int length)
    {
        Assert.False(TelemetryDecoder.TryDecode(new byte[length], out _));
    }
}
