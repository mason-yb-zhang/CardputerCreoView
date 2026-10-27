using System.Net;
using System.Net.Sockets;
using CardputerCreoView.Bridge.Core;

namespace CardputerCreoView.Bridge;

public sealed class MotionUdpSender : IDisposable
{
    private readonly UdpClient _client = new(AddressFamily.InterNetwork);
    private readonly IPEndPoint _endpoint = new(IPAddress.Loopback, MotionProtocol.Port);

    public void Send(in MotionFrame frame)
    {
        var bytes = new byte[MotionProtocol.PacketSize];
        MotionPacketWriter.Write(bytes, frame);
        _client.Send(bytes, bytes.Length, _endpoint);
    }

    public void Dispose()
    {
        _client.Dispose();
    }
}
