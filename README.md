# CardputerCreoView

M5Stack Cardputer Adv 通过 BLE 将 BMI270 和键盘输入发送到 Windows，目标是在 Creo 4.0 M130 中实现抓取式视图控制。

## 当前状态

**固件、BLE 和 Windows 桥接已实测通过；Creo 插件加载闪退已定位根因并修复（/MT 静态运行时），修复版待实机加载烟测。** 端到端未验收，不要据此认定插件可用。

- 约 55 Hz 的真实 BLE 遥测和 WPF 校准已通过。
- 桥接 24 项、固件 12 项、插件 26 项离线测试通过。
- 未执行任何 Creo 视图写入，未修改模型。
- 插件自动启动已禁用；未经明确授权，不得启动、调试、注册、部署或修改 Creo。

完整开发记录见 [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md)。

## 目录

- `firmware/`：独立 Cardputer 固件、BLE 协议及离线测试。
- `bridge/`：.NET 8/WPF 蓝牙桥接、姿态解算和状态机测试。
- `creo_plugin/`：Pro/TOOLKIT 原生插件、矩阵/协议离线测试和对话框资源。

## 构建与测试

### 固件

需要 PlatformIO 和锁定版本的 ESP32/M5Stack 依赖，版本见开发记录。

```bash
pio run --project-dir firmware
```

仅编译，不烧录。烧录必须先备份并另行明确授权。

### Windows 桥接

```bash
dotnet test bridge/CardputerCreoView.Bridge.sln --configuration Release
dotnet build bridge/CardputerCreoView.Bridge.sln --configuration Release
```

### Creo 插件离线测试

在具备 Creo 4.0 M130 Pro/TOOLKIT 和 MSVC x64 环境的机器上运行：

```bat
creo_plugin\build.bat
creo_plugin\tests\test.bat
```

`creo_plugin/protk.dat.example` 仅为注册模板，不能直接视为已验证可用的部署文件。

## 安全与发布边界

- 原始固件备份、工具链、构建产物和崩溃现场不进入仓库。
- 不上传模型、凭据、`.env` 或本地 MCP 配置。
- 不把离线测试通过等同于 Creo 实机验证通过。
- 后续实机工作必须先离线审查原生互斥锁和 CRT 生命周期，并重新取得明确授权。
