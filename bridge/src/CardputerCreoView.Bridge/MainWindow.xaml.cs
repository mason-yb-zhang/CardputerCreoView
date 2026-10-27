using System.ComponentModel;
using System.Net.Sockets;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using CardputerCreoView.Bridge.Core;

namespace CardputerCreoView.Bridge;

public partial class MainWindow : Window
{
    private readonly object _gate = new();
    private readonly MotionEngine _engine = new();
    private readonly MotionUdpSender _sender = new();
    private readonly DispatcherTimer _timer;
    private readonly CancellationTokenSource _connectCancellation = new();
    private BleTelemetryClient? _client;
    private bool _connecting;
    private bool _closing;
    private string _message = string.Empty;
    private long _invalidPackets;

    public MainWindow()
    {
        InitializeComponent();
        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(50) };
        _timer.Tick += (_, _) => UpdateInterface();
        _timer.Start();
        UpdateInterface();
    }

    private async void ConnectButton_Click(object sender, RoutedEventArgs e)
    {
        if (_client is not null || _connecting)
        {
            return;
        }

        _connecting = true;
        _message = "正在搜索 CardputerCreo，最长等待 15 秒。";
        var client = new BleTelemetryClient();
        client.PacketReceived += OnBlePacket;
        client.ConnectionChanged += OnBleConnectionChanged;
        _client = client;
        UpdateInterface();
        try
        {
            await client.ConnectAsync(_connectCancellation.Token);
            if (client.IsConnected)
            {
                _message = "已订阅遥测，请先保持静止并校准。";
            }
        }
        catch (Exception ex) when (ex is not OperationCanceledException || !_closing)
        {
            _message = ex is TimeoutException ? "未找到 CardputerCreo。" : ex.Message;
            DetachClient(client);
        }
        finally
        {
            _connecting = false;
            if (_closing)
            {
                lock (_gate)
                {
                    DetachClient(client);
                }
            }
            else
            {
                UpdateInterface();
            }
        }
    }

    private void DisconnectButton_Click(object sender, RoutedEventArgs e)
    {
        lock (_gate)
        {
            SendFrameLocked(_engine.SetConnected(false, Environment.TickCount64));
            DetachClient(_client);
            _message = "已断开。";
        }

        UpdateInterface();
    }

    private void CalibrateButton_Click(object sender, RoutedEventArgs e)
    {
        lock (_gate)
        {
            SendFrameLocked(_engine.StartCalibration(Environment.TickCount64));
            _message = "校准约 2 秒，请保持设备静止；移动会重新计时。";
        }

        UpdateInterface();
    }

    private void RecenterButton_Click(object sender, RoutedEventArgs e)
    {
        Recenter();
    }

    private void OutputCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        lock (_gate)
        {
            var requested = OutputCheckBox.IsChecked == true;
            if (requested == _engine.IsOutputRequested)
            {
                return;
            }

            SendFrameLocked(_engine.SetOutputRequested(requested, Environment.TickCount64));
        }

        UpdateInterface();
    }

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.R && !e.IsRepeat)
        {
            Recenter();
            e.Handled = true;
        }
    }

    private void Recenter()
    {
        lock (_gate)
        {
            SendFrameLocked(_engine.Recenter(Environment.TickCount64));
        }

        UpdateInterface();
    }

    private void OnBlePacket(byte[] bytes)
    {
        try
        {
            if (!TelemetryDecoder.TryDecode(bytes, out var packet))
            {
                lock (_gate)
                {
                    _invalidPackets++;
                }

                return;
            }

            lock (_gate)
            {
                if (!_closing)
                {
                    SendFrameLocked(_engine.ProcessPacket(packet, Environment.TickCount64));
                }
            }
        }
        catch (Exception ex)
        {
            lock (_gate)
            {
                _message = $"遥测处理失败：{ex.Message}";
            }
        }
    }

    private void OnBleConnectionChanged(bool connected)
    {
        lock (_gate)
        {
            if (!_closing)
            {
                SendFrameLocked(_engine.SetConnected(connected, Environment.TickCount64));
                _message = connected ? "BLE 已连接。" : "BLE 已断开，运动输出已失效。";
            }
        }
    }

    private void UpdateInterface()
    {
        MotionFrame? timeoutFrame = null;
        bool connected;
        bool calibrated;
        bool calibrating;
        bool outputRequested;
        bool outputActive;
        bool grab;
        bool translate;
        float calibrationProgress;
        long dropped;
        long received;
        long invalid;
        double rate;
        uint grabId;
        string message;
        string sensorText;
        string rotationText;
        lock (_gate)
        {
            var now = Environment.TickCount64;
            timeoutFrame = _engine.Tick(now);
            SendFrameLocked(timeoutFrame);
            connected = _engine.IsConnected;
            calibrated = _engine.IsCalibrated;
            calibrating = _engine.IsCalibrating;
            outputRequested = _engine.IsOutputRequested;
            outputActive = _engine.IsOutputActive(now);
            grab = _engine.IsGrabActive;
            translate = _engine.IsTranslateActive;
            calibrationProgress = _engine.CalibrationProgress;
            dropped = _engine.DroppedPackets;
            received = _engine.ReceivedPackets;
            invalid = _invalidPackets;
            rate = outputActive || connected ? _engine.PacketRateHz : 0;
            grabId = _engine.GrabId;
            var gyro = _engine.RawGyroscopeDps;
            var accel = _engine.RawAccelerationG;
            var rotation = _engine.RelativeRotationDegrees;
            sensorText = $"陀螺 raw  X {gyro.X:F1}  Y {gyro.Y:F1}  Z {gyro.Z:F1} °/s\n" +
                         $"加速度 raw X {accel.X:F2}  Y {accel.Y:F2}  Z {accel.Z:F2} g";
            rotationText = $"相对旋转 X {rotation.X:F1}°  Y {rotation.Y:F1}°  Z {rotation.Z:F1}°";
            message = _message;
        }

        StatusText.Text = calibrating ? "校准中，请保持静止" : connected ? calibrated ? "已连接，可以操作" : "已连接，等待校准" : "未连接";
        BleText.Text = _connecting ? "搜索 / 连接中" : _client is null ? "未连接" : connected ? "已连接并订阅" : "连接中断，等待恢复";
        CalibrationText.Text = calibrating ? $"{calibrationProgress:P0}，请保持静止" : calibrated ? "已校准" : "未校准";
        OutputText.Text = outputActive ? "启用" : outputRequested ? "等待有效数据" : "关闭";
        GrabText.Text = $"#{grabId} · {(grab ? translate ? "平移" : "旋转" : "松手")}";
        DroppedText.Text = invalid == 0 ? $"{dropped}" : $"{dropped}（无效包 {invalid}）";
        RateText.Text = $"{rate:F1} Hz · {received} 包";
        SensorText.Text = sensorText;
        RotationText.Text = rotationText;
        MessageText.Text = message;
        ConnectButton.IsEnabled = !_connecting && _client is null && !_closing;
        DisconnectButton.IsEnabled = _client is not null && !_connecting && !_closing;
        CalibrateButton.IsEnabled = connected && !calibrating && !_closing;
        RecenterButton.IsEnabled = connected && calibrated && !calibrating && !_closing;
        OutputCheckBox.IsChecked = outputRequested;
        OutputCheckBox.IsEnabled = connected && calibrated && !calibrating && !_closing;
    }

    private void SendFrameLocked(MotionFrame? frame)
    {
        if (frame is null)
        {
            return;
        }

        try
        {
            _sender.Send(frame.Value);
        }
        catch (Exception ex) when (ex is SocketException or ObjectDisposedException)
        {
            _message = $"UDP 发送失败：{ex.Message}";
        }
    }

    private void DetachClient(BleTelemetryClient? client)
    {
        if (client is null)
        {
            return;
        }

        client.PacketReceived -= OnBlePacket;
        client.ConnectionChanged -= OnBleConnectionChanged;
        client.Dispose();
        if (ReferenceEquals(_client, client))
        {
            _client = null;
        }
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        _closing = true;
        _connectCancellation.Cancel();
        _timer.Stop();
        lock (_gate)
        {
            SendFrameLocked(_engine.Disable(Environment.TickCount64));
            if (!_connecting)
            {
                DetachClient(_client);
            }

            _sender.Dispose();
        }

        _connectCancellation.Dispose();
        base.OnClosing(e);
    }
}
