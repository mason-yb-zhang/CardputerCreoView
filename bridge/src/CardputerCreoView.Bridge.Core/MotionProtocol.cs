using System.Buffers.Binary;
using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public static class MotionProtocol
{
    public const uint Magic = 0x4D564343;
    public const ushort Version = 1;
    public const ushort Port = 49250;
    public const int PacketSize = 60;
    public const int MagicOffset = 0;
    public const int VersionOffset = 4;
    public const int FlagsOffset = 6;
    public const int SequenceOffset = 8;
    public const int TimestampOffset = 12;
    public const int QuaternionOffset = 20;
    public const int PanOffset = 36;
    public const int ZoomOffset = 48;
    public const int GrabIdOffset = 52;
    public const int SessionIdOffset = 56;
}

[Flags]
public enum MotionFlags : ushort
{
    None = 0,
    Enable = 1 << 0,
    Grab = 1 << 1,
    Translate = 1 << 2
}

public readonly record struct MotionFrame(
    MotionFlags Flags,
    uint Sequence,
    ulong HostMilliseconds,
    Quaternion Quaternion,
    Vector3 PanVelocity,
    float ZoomVelocity,
    uint GrabId,
    uint SessionId);

public static class MotionPacketWriter
{
    public static int Write(Span<byte> destination, in MotionFrame frame)
    {
        if (destination.Length < MotionProtocol.PacketSize)
        {
            throw new ArgumentException("Motion packet requires 60 bytes.", nameof(destination));
        }

        var quaternion = QuaternionMath.NormalizeOrIdentity(frame.Quaternion);
        BinaryPrimitives.WriteUInt32LittleEndian(destination[MotionProtocol.MagicOffset..], MotionProtocol.Magic);
        BinaryPrimitives.WriteUInt16LittleEndian(destination[MotionProtocol.VersionOffset..], MotionProtocol.Version);
        BinaryPrimitives.WriteUInt16LittleEndian(destination[MotionProtocol.FlagsOffset..], (ushort)frame.Flags);
        BinaryPrimitives.WriteUInt32LittleEndian(destination[MotionProtocol.SequenceOffset..], frame.Sequence);
        BinaryPrimitives.WriteUInt64LittleEndian(destination[MotionProtocol.TimestampOffset..], frame.HostMilliseconds);
        WriteFloat(destination, MotionProtocol.QuaternionOffset, quaternion.X);
        WriteFloat(destination, MotionProtocol.QuaternionOffset + 4, quaternion.Y);
        WriteFloat(destination, MotionProtocol.QuaternionOffset + 8, quaternion.Z);
        WriteFloat(destination, MotionProtocol.QuaternionOffset + 12, quaternion.W);
        WriteFloat(destination, MotionProtocol.PanOffset, frame.PanVelocity.X);
        WriteFloat(destination, MotionProtocol.PanOffset + 4, frame.PanVelocity.Y);
        WriteFloat(destination, MotionProtocol.PanOffset + 8, frame.PanVelocity.Z);
        WriteFloat(destination, MotionProtocol.ZoomOffset, frame.ZoomVelocity);
        BinaryPrimitives.WriteUInt32LittleEndian(destination[MotionProtocol.GrabIdOffset..], frame.GrabId);
        BinaryPrimitives.WriteUInt32LittleEndian(destination[MotionProtocol.SessionIdOffset..], frame.SessionId);
        return MotionProtocol.PacketSize;
    }

    private static void WriteFloat(Span<byte> destination, int offset, float value)
    {
        BinaryPrimitives.WriteInt32LittleEndian(destination[offset..], BitConverter.SingleToInt32Bits(value));
    }
}
