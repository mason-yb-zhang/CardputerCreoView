namespace CardputerCreoView.Bridge.Core;

public sealed class SequenceLossTracker
{
    private bool _hasLast;
    private ushort _last;

    public int Observe(ushort sequence)
    {
        if (!_hasLast)
        {
            _hasLast = true;
            _last = sequence;
            return 0;
        }

        var delta = unchecked((ushort)(sequence - _last));
        if (delta == 0 || delta >= 0x8000)
        {
            return 0;
        }

        _last = sequence;
        return delta - 1;
    }

    public void Reset()
    {
        _hasLast = false;
        _last = 0;
    }
}

public sealed class PacketRateMeter
{
    private long? _windowStarted;
    private int _intervals;

    public double RateHz { get; private set; }

    public void Observe(long hostMilliseconds)
    {
        if (_windowStarted is not long started)
        {
            _windowStarted = hostMilliseconds;
            _intervals = 0;
            return;
        }

        _intervals++;
        var elapsed = hostMilliseconds - started;
        if (elapsed >= 500)
        {
            RateHz = _intervals * 1000d / elapsed;
            _windowStarted = hostMilliseconds;
            _intervals = 0;
        }
    }

    public void Reset()
    {
        _windowStarted = null;
        _intervals = 0;
        RateHz = 0;
    }
}
