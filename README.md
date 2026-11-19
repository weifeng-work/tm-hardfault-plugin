# HardFaultPlugin for TrafficMonitor

给 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 加一个「硬缺页」监控项，可在**主悬浮窗**和**任务栏窗口**显示，并在换页压力持续超标时用 TM 自带的气泡通知报警。

- 数据源：PDH `\Memory\Pages Input/sec`（即资源监视器里那个被误译为「硬中断/秒」的指标）
- 显示：`硬缺页: 123/s`
- 报警：窗口均值 ≥ 阈值且持续 N 秒 → TM 气泡通知 + 提示音，冷却期防刷屏
- 阈值可在右键菜单里调（±50 / 恢复默认），即时生效并自动写回配置

## 右键菜单

在悬浮窗上右键点击「硬缺页」那一格，或在托盘图标右键 → 插件 → HardFault：

| 命令 | 作用 |
|---|---|
| 阈值 +50（当前 N/s） | 抬高阈值 |
| 阈值 -50 | 降低阈值 |
| 阈值 恢复默认 400 | 回到 400 |
| 报警提示音（开/关） | 切换报警时的提示音 |
| 暂停报警（已暂停） | 临时静音，仍继续采集和显示 |

改完立即生效并写回 `hardfault.ini`，不需要重启。

## 构建（GitHub Actions，无需本机装 VS）

push 到 `main` 或在 Actions 页面手动 `Run workflow`，产物在 Artifacts → `HardFaultPlugin-x64`。

本地要构建也可以：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## 安装

1. 下载 `HardFaultPlugin_x64.dll`
2. 放到 TrafficMonitor 安装目录下的 `plugins\` 子目录（没有就新建）
3. 重启 TrafficMonitor（需要管理员权限，读硬件传感器）
4. `选项 → 常规设置 → 插件管理` 应能看到 `HardFault`

## 在主悬浮窗显示

插件项只能通过皮肤指定位置。在皮肤 `skin.xml` 的 `<root>` 下加 `plugin_map`，键名随便起，值是项目 ID `hardfault`：

```xml
<plugin_map>
  <hardfault>hardfault</hardfault>
</plugin_map>
```

然后在 `layout_l` 里用同名节点放位置：

```xml
<hardfault x="14" y="50" width="200" align="0" show="1"/>
```

## 在任务栏窗口显示

任务栏窗口右键 → 显示设置 → 勾选「硬缺页」。

## 配置

`%APPDATA%\TrafficMonitor\hardfault.ini`，首次运行自动生成：

```ini
[config]
threshold=400   ; 均值阈值（硬缺页/秒）
window=10       ; 滑动窗口秒数
sustain=8       ; 连续超标多少秒才报警
cooldown=600    ; 报警冷却秒数
beep=1          ; 1=报警时提示音
paused=0        ; 1=暂停报警（菜单里可切换）
```

参考值：普通桌面静息 10–60/s，开大型程序瞬时 200–1500/s，持续上千才是真换页压力。

## 说明

- `plugin/PluginInterface.h` 取自 TrafficMonitor 官方仓库 `include/PluginInterface.h`，仅用于编译接口。
- 只支持 x64（TM 1.86 x64）。
