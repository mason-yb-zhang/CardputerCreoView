using System.Buffers.Binary;
using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public static class TelemetryProtocol
{
    public const string DeviceName = "CardputerCreo";
    public const int PacketSize = 20;
    public const int SequenceOffset = 0;
    public const int TimestampOffset = 2;
    public const int GyroOffset = 6;
    public const int AccelOffset = 12;
    public const int ButtonsOffset = 18;
    public const float GyroUnitsPerDps = 10f;
    public const float AccelUnitsPerG = 1000f;
    public static readonly Guid ServiceUuid = new("9e5f7c20-7e6b-4f6c-9a1e-7a1d4f4b3001");
    public static readonly Guid CharacteristicUuid = new("9e5f7c20-7e6b-4f6c-9a1e-7a1d4f4b3002");
}

[Flags]
public enum TelemetryButtons : ushort
{
    None = 0,
    Grab = 1 << 0,
    Translate = 1 << 1,
    ZoomIn = 1 << 2,
    ZoomOut = 1 << 3,
    Calibrate = 1 << 4,
    Recenter = 1 << 5
}

public readonly record struct TelemetryPacket(
    ushort Sequence,
    uint Timestamp,
    Vector3 GyroscopeDps,
    Vector3 AccelerationG,
    TelemetryButtons Buttons);

public static class TelemetryDecoder
{
    public static bool TryDecode(ReadOnlySpan<byte> data, out TelemetryPacket packet)
    {
        packet = default;
        if (data.Length != TelemetryProtocol.PacketSize)
        {
            return false;
        }

        var gyro = new Vector3(
            BinaryPrimitives.ReadInt16LittleEndian(data[(TelemetryProtocol.GyroOffset)..]) / TelemetryProtocol.GyroUnitsPerDps,
            BinaryPrimitives.ReadInt16LittleEndian(data[(TelemetryProtocol.GyroOffset + 2)..]) / TelemetryProtocol.GyroUnitsPerDps,
            BinaryPrimitives.ReadInt16LittleEndian(data[(TelemetryProtocol.GyroOffset + 4)..]) / TelemetryProtocol.GyroUnitsPerDps);
        var acceleration = new Vector3(
            BinaryPrimitives.ReadInt16LittleEndian(data[TelemetryProtocol.AccelOffset..]) / TelemetryProtocol.AccelUnitsPerG,
            BinaryPrimitives.ReadInt16LittleEndian(data[(TelemetryProtocol.AccelOffset + 2)..]) / TelemetryProtocol.AccelUnitsPerG,
            BinaryPrimitives.ReadInt16LittleEndian(data[(TelemetryProtocol.AccelOffset + 4)..]) / TelemetryProtocol.AccelUnitsPerG);

        packet = new TelemetryPacket(
            BinaryPrimitives.ReadUInt16LittleEndian(data[TelemetryProtocol.SequenceOffset..]),
            BinaryPrimitives.ReadUInt32LittleEndian(data[TelemetryProtocol.TimestampOffset..]),
            gyro,
            acceleration,
            (TelemetryButtons)BinaryPrimitives.ReadUInt16LittleEndian(data[TelemetryProtocol.ButtonsOffset..]));
        return true;
    }
}
