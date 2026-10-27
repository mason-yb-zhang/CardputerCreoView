using System.Buffers.Binary;
using System.Numerics;
using CardputerCreoView.Bridge.Core;
using Xunit;

namespace CardputerCreoView.Bridge.Core.Tests;

public sealed class MotionProtocolTests
{
    [Fact]
    public void WriterMatchesAllSixtyByteOffsets()
    {
        var frame = new MotionFrame(
            MotionFlags.Enable | MotionFlags.Grab | MotionFlags.Translate,
            0x11223344,
            0x0102030405060708,
            Quaternion.Identity,
            new Vector3(1f, -2f, 3.5f),
            -1f,
            0xAABBCCDD,
            0x10203040);
        var bytes = new byte[MotionProtocol.PacketSize];

        Assert.Equal(60, MotionPacketWriter.Write(bytes, frame));
        Assert.Equal(MotionProtocol.Magic, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(0)));
        Assert.Equal(1, BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(4)));
        Assert.Equal(7, BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(6)));
        Assert.Equal(0x11223344u, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8)));
        Assert.Equal(0x0102030405060708UL, BinaryPrimitives.ReadUInt64LittleEndian(bytes.AsSpan(12)));
        Assert.Equal(0f, ReadFloat(bytes, 20));
        Assert.Equal(0f, ReadFloat(bytes, 24));
        Assert.Equal(0f, ReadFloat(bytes, 28));
        Assert.Equal(1f, ReadFloat(bytes, 32));
        Assert.Equal(1f, ReadFloat(bytes, 36));
        Assert.Equal(-2f, ReadFloat(bytes, 40));
        Assert.Equal(3.5f, ReadFloat(bytes, 44));
        Assert.Equal(-1f, ReadFloat(bytes, 48));
        Assert.Equal(0xAABBCCDD, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(52)));
        Assert.Equal(0x10203040u, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(56)));
    }

    [Fact]
    public void WriterNormalizesQuaternion()
    {
        var frame = new MotionFrame(default, 0, 0, new Quaternion(0f, 0f, 0f, 2f), default, 0f, 0, 0);
        var bytes = new byte[60];

        MotionPacketWriter.Write(bytes, frame);

        Assert.Equal(1f, ReadFloat(bytes, 32));
    }

    private static float ReadFloat(byte[] bytes, int offset)
    {
        return BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(bytes.AsSpan(offset)));
    }
}
