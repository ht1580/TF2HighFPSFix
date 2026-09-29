# TF2HighFPSFix 0.1.0

基于 Ion 的独立、客户端原生高帧率时序插件，面向build_profile.json所列的 Titanfall 2 零售版和R2Vanilla加载器。**实验性版本：已编译并完成离线/隔离模块验证；尚未在游戏中验证高帧率移动或双客户端观测。**

Experimental client-side timing compatibility patch. Native hooks for the existing R2Vanilla loader only; no injector, anti-cheat bypass, stealth loading, or gameplay changes. In-game effectiveness has NOT been verified. This project is not affiliated with Respawn, EA, Ion, or Northstar.

选择DLL是因为现有加载器支持原生插件，CL_Move、输入采样与客户端时间函数需要原生挂钩；脚本mod无法覆盖这些路径。没有独立注入器。

## 文件

- [Releases](https://github.com/ht1580/TF2HighFPSFix/releases)：DLL与INI预发布下载包；二进制不放在源码树中。
- `src/`、`CMakeLists.txt`：完整源码和构建工程。
- `third_party/minhook`：固定v1.3.4，提交c3fcafdc10146beb5919319d0683e44e3c30d537。
- `upstream/`：四个Ion提交、关键原文件和许可证。
- `docs/analysis.md`：移植范围、版本绑定及与Ion的差异。
- `docs/testing.md`：测试与日志判读；`tools/analyze_trace.py`分析真实命令时间戳。
- `build_profile.json`：3个模块的哈希和20个唯一函数签名。

## 放置方式

插件不会自动修改CFG、画面、灵敏度、宏、启动参数或fps_max。先备份现有加载器配置，只在你获准使用客户端mod的环境中测试；本项目不保证平台或服务器允许使用插件。

如果要试用，关闭游戏后将下载包中的DLL与INI一起放到已有的 `<Titanfall2>\R2Vanilla\plugins`，通过现有 `-profile=R2Vanilla` 加载。不要用普通DLL注入工具加载。该版本会拒绝没有明确R2Vanilla参数的进程。

插件启动后先检查3个游戏模块的磁盘SHA-256、加载后的PE身份、20个唯一函数签名及预期入口、输入接口的槽位和ConVar身份，然后统一启用11个hook。版本未知、已有hook改写相同入口、接口不符都会拒绝启用，并写明原因。保留现有文件，不要覆盖其他插件。

重启游戏后检查插件旁的 `TF2HighFPSFix.log`：

```text
SHA-256 / loaded PE identity / 20 unique function signatures validated.
ACTIVE: 11 hooks installed; Vanilla floor=5ms; ...
```

这些消息只说明安装hook成功。还需看到游戏进行时的usercmd日志和实际多人对照，才能判定修复生效。`cl_move_use_dt=0`会回退原CL_Move；插件不会擅自修改这个游戏ConVar。当前缺实机回读，不能保证你的运行环境满足此条件。

## 功能与日志

固定Vanilla的5ms最低命令间隔，跳过命令生成的帧继续采样鼠标、保留按键边沿和鼠标生成的移动，下一条命令恢复完整累计frameTime。额外实现Ion的client-time double余数、调用位置判断和server-uptime上限。

不更改渲染帧率。若外部受控设置为240FPS，理想稳定帧时间下约每两帧生成一次命令，即约120Hz；最低间隔5ms不代表固定200Hz。190FPS可约每帧一条命令。实际FPS边界必须从日志判断。

`Debug=1`记录约每2秒的提交帧率、命令率、额外采样率、平均/最大命令frameTime、datagram调用率、fallback和丢失的诊断事件。`Trace=1`还写QPC逐事件CSV，供受控测试使用；日常可设Trace=0，修改INI后重启。

BeginFrame计数表示渲染提交，不是显示器扫描输出；datagram调用计数也不等于网卡抓包数。游戏层输入损失、远端移动和命中情况必须另行验证。诊断记录若出现trace_lost，不应把缺失样本作为真实命令丢失；该诊断队列只接收首个生产线程的事件。

退出游戏后可将Enabled设0或把这两个文件移出plugins停用。不要在运行中卸载DLL；模块以正常Windows接口固定至进程退出，防止hook指向已卸载代码。

## 构建

本机已使用现有64位MinGW GCC16.1、CMake和Ninja构建，未安装新编译器：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
build\ModuleValidationTests.exe "<Titanfall2>\bin\x64_retail"
```

也提供MSVC构建配置，但本次未验证MSVC构建。DLL没有额外libstdc++/libgcc/MinHook运行时DLL依赖，依赖Windows系统组件。

`tools/make_build_profile.py`需要Capstone与pefile，只接受预设的三个SHA-256。不要绕过哈希或为未知版本生成签名；版本升级要重新核对结构、函数语义与调用点。

## 已完成验证

1. C++时间核心测试：9档固定FPS、累计时间守恒、双轴鼠标移动合并、短按边沿保留、重连清理、100ms长帧上限和大uptime的余数精度。
2. 在独立测试进程中映射3个真实PE模块，不运行游戏代码：哈希/入口/20签名通过；篡改CL_Move入口的内存副本后验证拒绝。
3. 11个真实函数入口的MinHook跳板，在独立映射中启用、停用、移除，恢复后再校验通过；不执行任何游戏函数。
4. DLL正常加载依赖、导出initializePlugin和嵌入resource101/API1通过。非R2Vanilla宿主调用入口后正常拒绝安装hook。

上述不能证明游戏内ABI调用、240FPS移动、单帧输入或官服命中已通过。当前交付是待实机验证的插件版本，不是已经证实消除卡顿的结论。

基于 [r2ion/IonLauncher](https://github.com/r2ion/IonLauncher)，使用MIT许可；MinHook使用其BSD许可，完整许可证随包保留。来源和版本见[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。不分发游戏DLL、资源或凭据；build_profile中仅保存版本哈希和必要的短入口签名。
