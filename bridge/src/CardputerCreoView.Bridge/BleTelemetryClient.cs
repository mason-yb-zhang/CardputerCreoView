using CardputerCreoView.Bridge.Core;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Storage.Streams;

namespace CardputerCreoView.Bridge;

public sealed class BleTelemetryClient : IDisposable
{
    private BluetoothLEDevice? _device;
    private GattSession? _session;
    private GattDeviceService? _service;
    private GattCharacteristic? _characteristic;
    private bool _disposed;

    public event Action<byte[]>? PacketReceived;
    public event Action<bool>? ConnectionChanged;
    public bool IsConnected { get; private set; }

    public async Task ConnectAsync(CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        try
        {
            var (address, addressType) = await FindDeviceAsync(cancellationToken);
            var device = await BluetoothLEDevice.FromBluetoothAddressAsync(address, addressType);
            cancellationToken.ThrowIfCancellationRequested();
            _device = device ?? throw new InvalidOperationException("无法创建 BluetoothLEDevice。");
            _device.ConnectionStatusChanged += OnConnectionStatusChanged;

            _session = await GattSession.FromDeviceIdAsync(_device.BluetoothDeviceId)
                       ?? throw new InvalidOperationException("无法创建 GATT 会话。");
            _session.MaintainConnection = true;

            var services = await _device.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            EnsureSuccess(services.Status, "GATT 服务发现");
            cancellationToken.ThrowIfCancellationRequested();
            _service = services.Services.FirstOrDefault(service => service.Uuid == TelemetryProtocol.ServiceUuid)
                       ?? throw new InvalidOperationException("未找到 Cardputer 遥测服务。");

            var characteristics = await _service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
            EnsureSuccess(characteristics.Status, "GATT 特征发现");
            _characteristic = characteristics.Characteristics.FirstOrDefault(item => item.Uuid == TelemetryProtocol.CharacteristicUuid)
                              ?? throw new InvalidOperationException("未找到 Cardputer 遥测特征。");
            if (!_characteristic.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Notify))
            {
                throw new InvalidOperationException("遥测特征不支持 Notify。");
            }

            _characteristic.ValueChanged += OnValueChanged;
            var notifyStatus = await _characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue.Notify);
            EnsureSuccess(notifyStatus, "启用 Notify");
            cancellationToken.ThrowIfCancellationRequested();
            UpdateConnection(_device.ConnectionStatus == BluetoothConnectionStatus.Connected, true);
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        if (_session is not null)
        {
            _session.MaintainConnection = false;
        }

        if (_characteristic is not null)
        {
            _characteristic.ValueChanged -= OnValueChanged;
        }

        if (_device is not null)
        {
            _device.ConnectionStatusChanged -= OnConnectionStatusChanged;
        }

        _service?.Dispose();
        _session?.Dispose();
        _device?.Dispose();
        _characteristic = null;
        _service = null;
        _session = null;
        _device = null;
        UpdateConnection(false);
    }

    private static async Task<(ulong Address, BluetoothAddressType AddressType)> FindDeviceAsync(
        CancellationToken cancellationToken)
    {
        var completion = new TaskCompletionSource<(ulong, BluetoothAddressType)>(
            TaskCreationOptions.RunContinuationsAsynchronously);
        var watcher = new BluetoothLEAdvertisementWatcher
        {
            ScanningMode = BluetoothLEScanningMode.Active
        };

        void OnReceived(BluetoothLEAdvertisementWatcher sender,
                        BluetoothLEAdvertisementReceivedEventArgs args)
        {
            if (string.Equals(args.Advertisement.LocalName, TelemetryProtocol.DeviceName,
                              StringComparison.Ordinal))
            {
                completion.TrySetResult((args.BluetoothAddress, args.BluetoothAddressType));
            }
        }

        watcher.Received += OnReceived;
        try
        {
            watcher.Start();
            return await completion.Task.WaitAsync(TimeSpan.FromSeconds(15), cancellationToken);
        }
        finally
        {
            watcher.Received -= OnReceived;
            if (watcher.Status == BluetoothLEAdvertisementWatcherStatus.Started)
            {
                watcher.Stop();
            }
        }
    }

    private void OnConnectionStatusChanged(BluetoothLEDevice sender, object args)
    {
        if (!_disposed && ReferenceEquals(sender, _device))
        {
            UpdateConnection(sender.ConnectionStatus == BluetoothConnectionStatus.Connected);
        }
    }

    private void OnValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        if (_disposed)
        {
            return;
        }

        var bytes = new byte[checked((int)args.CharacteristicValue.Length)];
        using var reader = DataReader.FromBuffer(args.CharacteristicValue);
        reader.ReadBytes(bytes);
        PacketReceived?.Invoke(bytes);
    }

    private void UpdateConnection(bool connected, bool raiseIfUnchanged = false)
    {
        if (IsConnected == connected && !raiseIfUnchanged)
        {
            return;
        }

        IsConnected = connected;
        ConnectionChanged?.Invoke(connected);
    }

    private static void EnsureSuccess(GattCommunicationStatus status, string operation)
    {
        if (status != GattCommunicationStatus.Success)
        {
            throw new InvalidOperationException($"{operation}失败：{status}。");
        }
    }
}
