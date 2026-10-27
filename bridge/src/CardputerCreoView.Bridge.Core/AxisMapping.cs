using System.Numerics;

namespace CardputerCreoView.Bridge.Core;

public readonly record struct AxisSelection(int Index, float Sign);

public readonly record struct AxisMapping(AxisSelection X, AxisSelection Y, AxisSelection Z)
{
    // 待实机标定，未确认外壳方向。
    public static AxisMapping UncalibratedDefault => new(new(0, 1f), new(1, 1f), new(2, 1f));
}

public static class SensorAxisMapper
{
    public static Vector3 Map(Vector3 input, AxisMapping mapping)
    {
        Span<float> values = [input.X, input.Y, input.Z];
        return new Vector3(Select(values, mapping.X), Select(values, mapping.Y), Select(values, mapping.Z));
    }

    public static TelemetryPacket Map(TelemetryPacket packet, AxisMapping mapping)
    {
        return packet with
        {
            GyroscopeDps = Map(packet.GyroscopeDps, mapping),
            AccelerationG = Map(packet.AccelerationG, mapping)
        };
    }

    private static float Select(ReadOnlySpan<float> values, AxisSelection selection)
    {
        if ((uint)selection.Index >= (uint)values.Length)
        {
            throw new ArgumentOutOfRangeException(nameof(selection));
        }

        return values[selection.Index] * selection.Sign;
    }
}
