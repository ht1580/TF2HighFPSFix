# 受控实机验证

目前没有运行游戏或双客户端实验。测试阶段不更改已有画面、灵敏度、按键CFG；用于实验的临时FPS限制应记录原值并在实验后恢复，不写入现有CFG。插件本身不设置FPS。

## 首次启用

1. 关闭游戏，把release的DLL与INI放进R2Vanilla/plugins，使用既有R2Vanilla启动方式。
2. 检查日志出现hash/signature验证和ACTIVE。若DISABLED，按日志解决版本/冲突/接口问题；不要取消校验或猜地址。
3. 加入测试会话，检查cl_move_use_dt读值和fallback计数。fallback持续增加意味着未走修复路径，不能以“DLL加载了”当作成功。
4. 保持Debug=1、Trace=1，稳定移动30秒，确认render_submit、cmd、extra等数据。BeginFrame不是显示器扫描帧率。

## 固定FPS矩阵

测试144、180、190、195、200、205、210、240、300。每一档独立重启/保存CSV，避免每次启动覆盖前一份轨迹；记录实际FPS、GPU负载、服务器、网络状况和窗口焦点，不用uncapped。

分析真实轨迹：

```powershell
py tools\analyze_trace.py PATH_TO_240_TRACE.csv --skip-seconds 5 --expected-render-fps 240 --output actual-240.json
```

如果前5秒之后仍含菜单/加载，延长skip或单独保存稳定测试区间。查看真实usercmd时间戳间隔、实际command_number连续性、frameTime平均/最大值、低于5ms的数量、extra_sample和fallback。先确认日志没有trace_lost；诊断缺失不等于游戏命令缺失。

不能直接用离线模拟输出宣称200FPS转折。必须对195/200/205的真实渲染提交与usercmd时间戳比较，记录门限附近帧抖动下的混合节奏，再对240确认约8.33ms命令间隔。实际测量不符合预期就记录事实并回查。

## 输入与双客户端

比较A=未打补丁144、B=未打补丁240、C=补丁240。客户端B作为观察者。使用同一服务器、地图、网络条件和轨迹：AD短按/交替、slidehop、wallrun、air strafe、跳跃快速转视角、蹲起、抓钩、自动与单发武器。

逐项检查本地视角连续性、单帧短按是否丢失、远端抖动/瞬移、移动速度和命中。CSV的buttons只能辅助定位输入差异，不能取代物理输入时刻与实际动作对照。NetChan的datagram调用日志只提供调用节奏；需要网络抓包判断choke/ack/真实报文。

成功标准：本地保持稳定原生240、命令frameTime表示完整累计时间、输入不丢失、无速度改变，远端表现接近原144基线，命中不劣化。单独通过编译、单元测试或ACTIVE日志不算成功。

## 停用

退出游戏后Enabled=0或移走DLL/INI，然后正常启动。插件没有修改游戏文件、CFG、画面设置或FPS，所以停用不需要重写这些文件。
