# Cardputer Adv → Creo 空间控制器开发记录

- 日期：2026-10-27
- 项目目录：`D:\Files\Documents\claude code\CardputerCreoView`
- 当前状态：**固件、BLE 和 Windows 桥接已实测通过；Creo 插件加载仍受阻，端到端未验收。**
- 安全事实：**从未执行任何 Creo 视图写入，未修改任何模型、特征或参数。**

## 1. 目标与关键决策

目标是用 M5Stack Cardputer Adv 的 BMI270 和键盘，经 BLE 在 Windows 中实现类似“抓住模型”的旋转、平移和缩放控制。

用户确认的方案：

- 首次目标为 **Creo 4.0 M130**，不是 Creo 10。
- 使用**独立控制器固件**，不保留出厂应用；烧录前已做完整备份。
- 第一版不依赖 3DxWare/spacenavd，而采用 **BLE → Windows 桥接 → Creo 原生 Pro/TOOLKIT DLL**。
- 旋转采用抓取后的相对姿态；平移采用倾斜产生速度，不把纯 IMU 加速度二次积分当作可靠位移。
- 不使用 mapkey，不修改模型几何、特征或参数，输出默认关闭。

后续新增的最高优先级边界：

> 多次插件加载崩溃后，用户明确表示“不要总是折腾 Creo 软件”。此后任何 Creo 启动、CDB 调试、注册、部署、视图写入或模型操作，都必须重新取得用户明确同意。当前只允许离线排查。

## 2. 系统组成

### 2.1 Cardputer 固件

- ESP32-S3FN8 + BMI270。
- 官方轴向：**X 向右、Y 向后、Z 向上**，来自 M5Stack 官方 IMU 三轴图。
- 启动即运行独立控制器，显示 BLE、抓取、平移和 IMU 状态。
- NimBLE 自定义 GATT Notify，默认目标 60 Hz。
- `fallback_board = board_M5CardputerADV`，避免显示检测失败时错误回退。
- 串口日志已启用 115200；USB 日志捕获曾受复位重枚举影响，但 BLE 已证明应用正常运行。

### 2.2 BLE 遥测协议（20 字节，小端）

| 偏移 | 字段 | 格式/量纲 |
|---|---|---|
| 0 | sequence | `uint16` |
| 2 | device_ms | `uint32` |
| 6 | gyro X/Y/Z | 3 × `int16`，0.1 °/s/LSB |
| 12 | accel X/Y/Z | 3 × `int16`，0.001 g/LSB |
| 18 | buttons | `uint16` 位图 |

按键位：Space=抓取，T=平移，`+`/`-`=缩放，C=校准，R=重新锚定。

### 2.3 Windows .NET 8/WPF 桥接

- 使用 `BluetoothLEAdvertisementWatcher` 发现广播，再用 `FromBluetoothAddressAsync(address, addressType)` 创建连接。
- 不使用存在非配对发现问题的旧设备选择器路径。
- 完成 2 秒静止陀螺零偏校准、Mahony 姿态解算、抓取/重抓/模式切换状态机。
- 新抓取、重新锚定和旋转/平移切换都会递增 `grabId`，避免复用旧锚点造成跳变。
- 150 ms 无新 BLE 包即撤销输出；断线、超时、校准和手动停用后，必须重新启用并先松开一次抓取键。
- 通过 loopback UDP 向 `127.0.0.1:49250` 发送 60 字节 Motion 包，包含相对四元数、平移/缩放速度、`grabId` 和 `sessionId`。
- 仅保留最新帧，不排队补帧；输出默认关闭。

### 2.4 Creo 插件设计（尚未通过加载验证）

- 独立 Pro/TOOLKIT DLL，UDP 接收线程只更新单份最新样本。
- 自有非模态对话框持有官方 UI Timer，提供 2/5/10 Hz 三档，初始 2 Hz。
- 设计为先空 Timer 回调和只读视图检查，再允许用户启用视图控制。
- `ProViewMatrixGet/Set` 管方向，`ProWindowPanZoomMatrixSet` 管平移/缩放，最后 `ProWindowRepaint`。
- 300 ms 数据超时、窗口/模型变化或 API 错误时冻结。
- **以上仅为代码设计；插件在加载阶段失败，尚未证明任何接口可实机使用。**

## 3. 锁定的构建版本

| 项目 | 版本/配置 |
|---|---|
| PlatformIO | 6.1.19 |
| `espressif32` | 6.7.0 |
| Arduino ESP32 | 2.0.16 |
| M5Cardputer | 1.1.1 |
| M5Unified | 0.2.10 |
| M5GFX | 0.2.15（GitHub 标签 `c6f92dc03226cdc04d67c705a2020f62ad21ad01`） |
| NimBLE-Arduino | 2.1.0 |
| Windows 桥接 | .NET 8 / WPF |
| Creo 目标 | 4.0 M130 Pro/TOOLKIT |

C++17 配置已固定：移除默认 `-std=gnu++11`，使用 `-std=gnu++17`。

大型官方依赖均通过注册表或 M5Stack/Espressif 官方来源取得并核对 SHA-256；未使用来源不明的镜像。

## 4. 已完成验证

### 4.1 离线测试

| 测试对象 | 结果 |
|---|---|
| Windows 桥接协议、姿态和状态机 | **24/24 通过** |
| 固件遥测序列化 | **12/12 通过** |
| Creo 插件矩阵、UDP 解码、序号回绕 | **26/26 通过** |
| WPF 完整构建 | 0 警告、0 错误 |
| WPF 启动与布局烟测 | 窗口创建、截图检查、正常关闭均通过 |

这些测试**不能证明 Creo 插件可加载或可实机控制视图**。

### 4.2 固件构建与烧录

- RAM 使用约 **10.0%**（32628 字节）。
- Flash 使用约 **19.1%**（636789 字节）。
- 最终固件 SHA-256：`f69f8c894a6050e138760f614f7702365b4ebb9e03fe3bd1f3efaa899be83cfb`。
- 最后一次上传所有区块均通过 esptool 哈希校验。

烧录前完整备份：

- 文件：`firmware/backups/factory-com6-20261027-172307.bin`
- 大小：8388608 字节（完整 8 MB）
- SHA-256：`f82304b2c7594562d96e5188b61de0c6405b704c4ae89515cdd77b8c2a943683`
- 备份仅保存在本机，不上传、不公开。

### 4.3 真实 BLE 与桥接

- 设备广播名：`CardputerCreo`。
- 直接订阅 10 秒：555 个有效 20 字节包，约 **55.24 Hz**，未发现错误包长。
- 实际 WPF 连接与校准：约 **54.6 Hz、0 丢包、输出关闭**。
- 校准完成，IMU 数据与按键状态正常。
- 证据截图：`bridge/wpf-connected.png`。

## 5. Creo 加载故障记录

### 5.1 第一次启动崩溃

- 现象：加载插件时发生 `C0000005`，未执行视图写入。
- `traceback + MAP` 定位到 `LogInit()` 的 `std::filesystem` 调用路径。
- 处理：移除该调用，改用 Win32 `CreateDirectoryA` 与 `std::string` 路径。
- 结果：修复版可进入 Creo 主窗口，但**不能据此认定初始化或手动 Start 已安全**。
- 现场：`D:\PTC\CardputerCreoView\traceback-filesystem.log`。

### 5.2 手动 Start 再次闪退

- 用户手动启动后再次发生闪退，未执行视图写入。
- CDB 捕获到先出现 C++ EH，随后访问冲突：
  - `MSVCP140!Mtx_destroy+0xa4`
  - 指令为 `mov rbx,qword ptr [rax]`，当时 `rax=0`。
- 现场：`D:\PTC\CardputerCreoView\cdb-crash.log`。
- 这只能证明原生互斥锁/对象生命周期/CRT 路径存在问题，**尚未完成根因修复**。
- 已禁用自动启动，最新禁用文件为：
  `D:\PTC\CardputerCreoView\protk.dat.disabled-20261027-191612`。
- 当前部署 DLL 与源码构建一致，SHA-256：
  `d7e050134bdf154e1299739dc073aea4387d9d67bd4abf4b505244adc67f278d`。

### 5.3 根因定位与修复（2026-10-27 晚）

通过 traceback 地址减 DLL 首选基址（0x180000000）+ map 文件解析，锁定崩溃链：

```
user_initialize+0x24 (LogInit)
  → std::ofstream / basic_filebuf
    → MSVCP140!Mtx_destroy 空指针（rax=0）
```

**根因**：插件以 `/MD` 动态运行时编译，而 Creo 进程内先加载了自带的老运行时
`D:\PTC\Creo 4.0\M130\Common Files\x86e_win64\obj\MSVCP140.dll`（版本 **14.00.24210**，2017 年）。
同名 DLL 已在进程中，新编译的 DLL 被动绑定它；新版 STL 的 `std::mutex`/`std::ofstream`
布局与老 `_Mtx_*` 不兼容，`user_initialize` 第一行即崩（插件日志文件均未创建）。
这与 5.2/5.3（原编号顺延）的 2026-09-24 先例为同一根因。

**修复**（`creo_plugin/build.bat`）：

- `/MD` → `/MT`（静态链接 VC 运行时，插件自带 STL，不依赖进程内 MSVCP140）。
- `protk_dllmd_NU.lib` → `protk_dll_NU.lib`（/MT 配套的 Pro/TOOLKIT 库变体）。

**验证**：`dumpbin -dependents` 确认新 DLL 导入表不含 MSVCP140/VCRUNTIME140；
插件日志与加载烟测尚未实机执行（按操作边界待用户授权）。

**注册状态**：`D:\PTC\CardputerCreoView\protk.dat` 曾两次被改名禁用
（`protk.dat.disabled-20261027-185849` / `-191612`，排查期间操作，保留备份），
后已从备份恢复，并在
`D:\PTC\Creo 4.0\M130\Common Files\text\config.pro` 增加注册行
`protkdat D:\PTC\CardputerCreoView\protk.dat`。

**经验**：给 Creo 4 写 Pro/TOOLKIT DLL 一律用 `/MT` + `protk_dll_NU.lib`；
TDdesigntool 虽用 `/MD` 但避开了 MSVCP140 中的特性（mutex/ofstream/thread），
不是可照抄的安全样板。

### 5.4 重要历史先例

线上记忆 `6262410b-d850-4d79-afa5-fbe2a4d32795` 记录了 2026-09-24 的 Creo CRT 互斥锁问题：

- MSVC 14.51 下的 `std::mutex` 与 Creo 的 MSVCP140 14.0.24210 出现锁 NULL/访问冲突。
- 当时 Win32 SRW/非 RAII 路径通过离线验证，系统 CRT 路径也可行。

本次 `Mtx_destroy` 现象与该先例高度相关，应优先离线审查全局 C++ 对象、互斥锁及 CRT 生命周期；**不能直接宣称与该故障完全相同或已修复。**

## 6. 后续待执行的事项

1. 实机加载烟测（/MT 修复版）：确认插件加载、日志生成、只读检查通过。
2. 通过后：2 Hz 视图测试 → 再评估 5/10 Hz 及更高采样频率（ProUITimer 档位现为 2/5/10 Hz）。
3. 备选路线：引入 3DxWare NavLib SDK（`F:\Files\AI_FILES\3DxWare\3DxWare_SDK_v4-0-6_r22071`），
   由桥接直接向 Creo 内置的 NavLib 服务推送运动值，可能取代自研 Pro/TOOLKIT 插件
   并解决采样频率瓶颈（Creo 原生消化，推送模型）。

## 7. 操作边界

- **未经重新明确授权，不得启动、调试、注册、部署或修改 Creo。**
- 不使用 mapkey；如未来确有必要，必须单独征得用户同意。
- 不自动删除文件、不强制结束进程、不推送代码。
- 保留原始备份和崩溃现场，不覆盖、不删除。
- 不向外部服务上传备份、模型、配置或凭据。

## 8. 主要源码与证据位置

- 固件：`firmware/src/main.cpp`、`TelemetryProtocol.h`、`TelemetryCodec.h`
- 桥接核心：`bridge/src/CardputerCreoView.Bridge.Core/MotionEngine.cs`
- BLE 实现：`bridge/src/CardputerCreoView.Bridge/BleTelemetryClient.cs`
- 桥接界面：`bridge/src/CardputerCreoView.Bridge/MainWindow.xaml(.cs)`
- Creo 插件：`creo_plugin/src/`
- 插件构建：`creo_plugin/build.bat`
- 离线测试：`firmware/tests/`、`creo_plugin/tests/`、`bridge/tests/`
- CDB 现场：`D:\PTC\CardputerCreoView\cdb-crash.log`

## 9. 官方资料

- [M5Stack Cardputer Adv 规格](https://docs.m5stack.com/en/core/Cardputer-Adv)
- [M5Cardputer Arduino 快速开始](https://docs.m5stack.com/en/arduino/m5cardputer/program)
- [M5Unified 0.2.10](https://github.com/m5stack/M5Unified/tree/0.2.10)
- [M5GFX 0.2.15](https://github.com/m5stack/M5GFX/tree/0.2.15)
- [Microsoft BLE Advertisement Watcher](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher)
- [Microsoft FromBluetoothAddressAsync](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothledevice.frombluetoothaddressasync)
- Creo 4.0 M130 本地官方头文件：`D:\PTC\Creo 4.0\M130\Common Files\protoolkit\includes\`

## 10. 发布与记忆

- GitLab 项目目录：`https://git.n.xiaomi.com/zhangyanbin3/ai-skills/-/tree/main/CardputerCreoView`
- 首次发布提交：`13a754a3f38ac46cb9b6a83b83b9d6bc873d2979`。
- 仓库只包含源码、构建/测试脚本和文档；原始备份、工具链、二进制、截图及崩溃现场未上传。
- mify 项目状态记忆：`69d48aab-958c-4ce9-9436-a0912919bbdc`。
- mify 长期安全边界记忆：`0a7bf95c-31ce-46a7-9ecf-4f165b9b96bc`。
- mify 开发记录与备份索引：`b31bb57e-4fef-4a55-94ea-75b7ff0bcc8b`。
- 本地记忆：`cardputer-creo-view-project.md`、`feedback-creo-no-launch-without-reapproval.md`、`cardputer-creo-view-record.md`，均已登记到 `MEMORY.md`。

## 11. 路线切换：BLE HID 模拟 SpaceMouse（2026-10-27 深夜）

官方 NavLib 对接路线（自研插件 → nlserver）因 3DxCreo 插件不加载而放弃；
改为路线 1：Cardputer 固件直接模拟 3Dconnexion SpaceMouse Wireless BT。

**关键根因与修复**：
1. PnP ID 厂商源必须 0x02（蓝牙 SIG）。此前误用 0x01（USB-IF），
   Windows 实例 ID 变为 VID&01256F，与 3dxhid.inf 的
   `HID\{00001812-...}_Dev_VID&02256f_PID&c63a` 不匹配，官方驱动不接管。
2. HOGP 报告值不含 Report ID 前缀（Report ID 由 0x2908 描述符声明）。
   运动报告 12 字节（6 轴 int16 LE），按钮报告 2 字节。
3. NimBLEHIDDevice 配对需 Just Works 安全配置（bonding + NoInputOutput IO cap），
   否则 Windows "正在连接→消失"。
4. BLE HID 输入报告无需等待订阅即可推送；nimble 的 setValue+notify() 标准写法。
5. 广播需 Appearance 0x03C0 + HID 服务 UUID 0x1812 + 快速间隔（~20ms）。

**实机验证**：Windows 设备管理器显示 "3Dconnexion SpaceMouse Wireless
Bluetooth Edition"（官方 3dxhid 驱动接管），3DxService 状态 PAIREDANDSEENDATA，
Creo 视图随设备动作响应。旧自研 Pro/TOOLKIT 插件（/MT 修复版）保留可用。

**资源**：SpaceMouse HID 描述符来自开源社区逆向（github.com/AndunHH/spacemouse,
CC BY-NC-SA，270 字节 Report Map 见 firmware/src/SpaceMouseHID.h）。

## 12. 旋转专用模式与设备配置坑（2026-10-28 凌晨）

- **旋转专用**：%APPDATA%\3Dconnexion\3DxWare\Cfg\ProE.xml 中 AxisBank 设置
  X/Y/Z Enabled=false、Rx/Ry/Rz Enabled=true（该 XML 为带缩进格式，需按 <Axis> 块正则修改）。
- **设备专属配置段必须带 AxisBank**：给 ID_ProductID_C63A 加 ButtonBank 时若不带
  AxisBank，驱动对该设备关闭全部轴（不回退 ID_Standard_3D_Mouse），症状为固件
  SENT 正常但 Windows hidapi 0 报告、Creo 无响应。已修复（同段补齐旋转轴配置）。
- **按键映射**：空格→Driver_AppFit（视图适配）；T→`AppCommand_ProCmd ~ Command \`3DxProE\`;`
  打开 3DxProE 设置对话框，其中 Rotation Center 可选 Manual/Auto/Use Selected Item。
  驱动动作表没有一键切换旋转中心的动作。
- **屏幕防闪**：drawStatus 改为 M5Canvas 离屏渲染 + 内容哈希不变不推帧，
  替代原先每 200ms 全屏清写；屏幕增加 SENT/FAIL/TX/RX 排障计数器。
- 备份：ProE.xml.before-rotation-only / .before-buttons / .before-axisbank-fix。
