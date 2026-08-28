# ESP32-C3 电子琴固件

基于网表 `ESP32C3电子琴2026-08-28.tel` + 用户提供的 ESP32-C3FH4 DevKitM 16 脚模块板子照片反推出的
8 键全触摸电子琴固件。Arduino 框架, PlatformIO 构建, 单一合并 `firmware.bin` 烧录。

> **网络受限怎么办?** 工程里已经包含 `.github/workflows/build.yml` — 把代码 push 到 GitHub,
> Actions 会在 GitHub 的服务器(完整公网访问)上 build,产出 `piano-firmware` artifact 可直接下载。
> 工程根目录的 `auto_build.ps1` 脚本能**全自动完成**:粘贴 GitHub PAT → 创建仓库 → push → 监控 build → 下载 .bin。
> 详见下文 "用 auto_build.ps1 全自动"。

## 用 auto_build.ps1 全自动 (本机网络受限时)

## 项目功能

8 个触摸键(6 个 ESP32-C3 内部触摸外设 + 2 个 TTP223 硬件触摸)按 C 大调 1 阶 8 音
(C4–C5, 261.63 Hz–523.25 Hz) 触发正弦波音频输出,经外部 D 类功放 (U3) 推动扬声器。

- 单音 monophonic(同时按多键时取按键编号最大的那个)
- 启动时 LED 自检快闪 3 次
- 按下任意键:LED 常亮, 功放使能 (SD 高), 播放正弦波
- 松开:50 ms 淡出后静音,功放自动休眠

## 引脚对照 (与板子丝印 100% 对应)

| 板载位置 | GPIO | 功能 | 触摸通道 |
|---------|------|------|---------|
| 左 1 | 5V | 电源 | — |
| 左 2 | GND | 地 | — |
| 左 3 | 3V3 | 电源 | — |
| 左 4 | GPIO4 | 触摸键 3 (E4) | T4 |
| 左 5 | GPIO3 | 触摸键 2 (D4) | T3 |
| 左 6 | GPIO2 | 触摸键 1 (C4) | T2 |
| 左 7 | GPIO1 | 触摸键 7 (B4) — 经 TTP223 | T1 |
| 左 8 | GPIO0 | 触摸键 8 (C5) — 经 TTP223 | T0 |
| 右 9 | GPIO5 | 触摸键 4 (F4) | T5 |
| 右 10 | GPIO6 | 触摸键 5 (G4) | T6 |
| 右 11 | GPIO7 | 触摸键 6 (A4) | T7 |
| 右 12 | GPIO8 | (保留) | — |
| 右 13 | GPIO9 | (保留) | — |
| 右 14 | GPIO10 | 功放 SD 使能 (R5 下拉,默认关) | T8 (未用) |
| 右 15 | GPIO20 | **LEDC PWM 音频输出** (10-bit, 88.2 kHz) | — |
| 右 16 | GPIO21 | **LED1** (R1 限流) | — |

> ⚠️ **GPIO20、GPIO21 被音频和 LED 占用**,所以**串口必须用 ESP32-C3 USB-C 自带的 USB CDC**
> 而不是 UART0 (GPIO20/21)。`platformio.ini` 中已加 `ARDUINO_USB_CDC_ON_BOOT=1`。

## 目录结构

```
esp32c3_piano/
├── platformio.ini          # PlatformIO 配置 (USB CDC + 触摸阈值等)
├── merge_bin.py            # post: 合并 bootloader+partitions+app → dist/firmware.bin
├── flash.bat               # Windows 一键烧录脚本
├── src/
│   ├── main.cpp            # 入口: setup() + loop()
│   ├── pins.h              # 所有 GPIO / 定时常量
│   ├── notes.h             # 8 音频率表
│   ├── audio.h / audio.cpp # LEDC PWM 音频引擎 (Timer ISR + 正弦表 + 淡出)
│   └── keys.h / keys.cpp   # 6 个 ESP-IDF touch_pad + 2 个 TTP223 + 30 ms 去抖
└── README.md
```

## 编译

前置条件:
- PlatformIO Core 6.x (`pip install platformio` 或使用本地 `C:\Users\oushi\.platformio\penv\Scripts\pio.exe`)
- 网络 (首次构建会下载 RISC-V 工具链 ~200MB,大约 5–15 分钟)

```bash
cd esp32c3_piano
pio run --environment esp32c3
```

编译成功后 `dist/firmware.bin` 是合并好的单镜像,可以直接烧录。

## 用 GitHub Actions 远程 build(本机网络受限时)

如果你的本地网络无法下载 RISC-V 工具链(例如在受限网络下 `github.com` 大文件下载被掐断),
可以把这个工程 push 到 GitHub,让 GitHub Actions 在云端完成 build 并下载产物。

**步骤**:

1. 注册 GitHub 账号(已有跳过)
2. 创建一个新仓库(可以是 private)
3. 在该仓库根目录放入本工程的所有文件
4. 提交并 push:
   ```bash
   cd esp32c3_piano
   git init
   git add .
   git commit -m "Initial piano firmware"
   git branch -M main
   git remote add origin https://github.com/<你的用户名>/<仓库名>.git
   git push -u origin main
   ```
5. 打开 GitHub 仓库的 **Actions** 标签 → 选择 "Build ESP32-C3 piano firmware" → **Run workflow**
6. 等待 ~5-10 分钟,workflow 会自动:
   - 安装 PlatformIO
   - 下载 RISC-V 工具链(GitHub 服务器无网络限制)
   - 编译并 merge bootloader+app+partitions → `dist/firmware.bin`
   - 用 esptool image-info 校验产物
   - 上传 `firmware.bin` 为 `piano-firmware` artifact
7. 在 workflow run 页面底部 **Artifacts** 区域下载 `piano-firmware.zip`,
   解压后就是可以直接烧录的 `firmware.bin`

> 整个过程不需要任何 ESP32 工具链安装在本地,只需要能 push 到 GitHub。

### 更简单:用 `auto_build.ps1` 全自动

不想手动执行 `git push`?直接双击工程根目录的 `auto_build.ps1`,粘贴一次 GitHub PAT,脚本会:

1. 验证 PAT
2. 在你 GitHub 账号下创建仓库 `esp32c3-piano` (public)
3. 自动 `git push` 所有源码
4. 触发 GitHub Actions workflow
5. 每 15 秒轮询 build 状态,直到完成
6. 自动下载 `piano-firmware` artifact
7. 解压出 `firmware.bin` 放到 `dist/`

**完整前置**:
- 注册 GitHub 账号
- 去 https://github.com/settings/tokens/new 生成 PAT,勾选 `repo` + `workflow` scope,设置有效期
- 把 PAT 粘贴到 `auto_build.ps1` 提示中(脚本会自动隐藏输入)

**运行**:
```cmd
cd esp32c3_piano
auto_build.ps1
```

约 5-10 分钟后,`dist\firmware.bin` 就绪。



## 烧录

### 方法 1: PlatformIO 串口烧录

```bash
# 1. 把板子用 USB-C 接到 PC,确认在设备管理器看到 COMx
# 2. 烧录
pio run --environment esp32c3 --target upload --upload-port COMx
```

### 方法 2: 手动 esptool (推荐 — 一个 bin 文件搞定)

```bash
python -m esptool --chip esp32c3 -p COMx write_flash 0x0 dist/firmware.bin
```

或者直接双击 `flash.bat`,会自动列出可用 COM 口供选择。

### 方法 3: 串口监视 (调试用)

```bash
pio device monitor --port COMx --baud 115200
```

> USB CDC 不受波特率影响,任意 baud 都能看日志。

## 触摸键物理要求

TP1–TP8 都需要在 PCB 上有触摸电极(铜箔焊盘、弹簧、铆钉或导线延长到外壳):

- **TP1–TP6** (GPIO2–7, ESP32-C3 内部触摸):电极面积 ≥ 5×5 mm, 走线 ≤ 20 mm, 周围画 GND guard ring
- **TP7、TP8** (GPIO1、0, TTP223):电极面积 ≥ 5×5 mm, 走线 ≤ 20 mm
- 触摸电极**不能被金属外壳完全包裹**;有塑料/亚克力面板时,需 ≤ 5 mm 非金属层

## 串口日志示例

```
==========================================
ESP32-C3 electronic piano firmware booting
==========================================
[main] Waiting 1.0 s for TTP223 warm-up...
[main] audio_init done
[keys] Capturing touch baseline (16 samples per channel)...
  HAL ch 0 (GPIO2) baseline = 1240
  HAL ch 1 (GPIO3) baseline = 1180
  HAL ch 2 (GPIO4) baseline = 1210
  HAL ch 3 (GPIO5) baseline = 1195
  HAL ch 4 (GPIO6) baseline = 1230
  HAL ch 5 (GPIO7) baseline = 1205
[keys] Last pressed: (none)
[main] Ready - touch a key to play a note
[main] Note on  key 0 C4 (261.63 Hz)
[main] Note off
[main] Note on  key 7 C5 (523.25 Hz)
[main] Note off
```

## 重要风险/警告

1. **CR2032 电池 (U1) 接在 +5V 节点**:网表里电池 + 端直接连到 5V,标称 3V 电池出现在 5V 节点,可能是 (a) 隐藏升压 IC、(b) 可充电纽扣 + 充电 IC、(c) 网表简化误标。**烧录/测试时不要装入 CR2032,仅用 USB 供电**。
2. **PWM 音频高频衰减**:R2 (1kΩ) + C1 (100nF) 构成 ~1.6 kHz 的低通滤波器,> 2 kHz 频率被衰减。但单音电子琴范围 (C4–C5 = 261–523 Hz) 全部在通带内,完全够用。
3. **GPIO20、GPIO21 被占用**:串口必须用 USB CDC,不要试图用 UART0 (会与音频/LED 冲突)。
4. **首次构建需要联网下载 RISC-V 工具链** (~200MB),大约 5–15 分钟。
5. **TTP223 上电 0.5 s 稳定期**:固件 `setup()` 中已加 `delay(1000)`,所以上电后 1 s 内触摸无响应是正常。

## 修改音阶

编辑 `src/notes.h` 即可:

```cpp
static const float kNoteFreq[NUM_KEYS] = {
    261.63f,  // C4
    293.66f,  // D4
    // ...
};
```

支持任意频率(0.0f = 该键不发声),适合做五声音阶、小星星、Twinkle Twinkle 等曲子。

## 触摸灵敏度调参

如果某个触摸键不灵敏或误触发,编辑 `src/pins.h`:

```cpp
#define TOUCH_THRESHOLD_OFFSET  30   // 调大减少误触发,调小提高灵敏度
```

或者重新捕获基线:增加 `TOUCH_BASELINE_SAMPLES`。
