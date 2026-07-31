# 开发踩坑笔记

记录本项目排查过、且容易再次误判的问题。每条包含**现象 / 根因 / 正确做法 / 验证依据**，便于后续遇到相似症状时直接对照。

---

## 1. Accessibility 的 `doAction(Press)` 会静默失效

### 现象

对 QML `Button` 执行点击，`InputSimulator::click()` 返回 `true`、注入侧日志 `status=ok`，但界面上按钮的 `onClicked` 根本没触发，关联的 Label 文本不变。即"报告成功但实际没点到"。

### 根因

`QAccessibleQuickItem::actionNames()` 只要 item 声明了 `Accessible.role: Accessible.Button` 就会把 `pressAction` 列为可用动作，而 `doAction(pressAction)` 的实现仅仅是 emit `QQuickAccessibleAttached::pressAction()` 信号。

如果 QML 侧没有声明 `Accessible.onPressAction` 处理器，这个信号**没有接收者**，于是：

- 动作查询显示"可用" → 代码认为这条轨道能走；
- `doAction()` 不返回失败 → 代码认为执行成功；
- 实际什么也没发生。

两个条件叠加，就构成了一个既不报错、又不生效的静默陷阱。

此外，直接把 `QMouseEvent` 用 `sendEvent` 投给 `QQuickItem` 也不可靠——它绕过了 Qt Quick 的场景事件分发与命中测试，Item 内部的 `MouseArea`、Controls 的私有事件处理都可能收不到。

### 正确做法

把事件投递给 **`QQuickWindow`**，坐标用 **scene 坐标**，让 Qt Quick 自己做命中测试和分发。这与 `QTest::mouseClick` 的做法一致，行为最接近真实用户操作：

```cpp
// 目标 item 的中心点分别换算到 scene 坐标与全局坐标
const QPointF center    = itemCenter(target);
const QPointF scenePos  = target->mapToScene(center);
const QPointF screenPos = target->mapToGlobal(center);

QMouseEvent event(type, scenePos, scenePos, screenPos,
                  button, buttons, Qt::NoModifier);
m_eventTimestamp += 10;      // 时间戳必须递增，否则双击等连续事件判定会错乱
event.setTimestamp(m_eventTimestamp);
event.setAccepted(false);

QCoreApplication::sendEvent(target->window(), &event);
return event.isAccepted();   // 以是否被接受作为命中判据
```

注意两点：

- **press 的 `isAccepted()` 才是命中判据**，但 release 无论 press 结果如何都必须补发，否则控件会停留在按下态；
- 事件时间戳需自行递增，双击等依赖时序的判定才正确。

因此轨道优先级必须是：

| 优先级 | 轨道 | 说明 |
|:---|:---|:---|
| Track 1 | 窗口场景投递 | 主轨道，`QQuickWindow` + scene 坐标 |
| Track 2 | MouseArea 直接寻址 | 适用于 Item + MouseArea 组合 |
| Track 3 | item `sendEvent` | 直接向 item 发事件 |
| Track 4 | Accessibility | **最后回退**，因为可能静默失效 |

**不能**把 Accessibility 当主轨道。

同理，读取文本要以**内容属性优先**（`text` / `displayText`），Accessibility 的 `ai->text(QAccessible::Name)` 仅作回退——Label 的 accessible Name 并不等于它的 `text` 属性值，优先用 Name 会取到错误内容。

### 验证依据

E2E 日志中每条用例带 `[method]` 字段，可确认实际走了哪条轨道：

```
click btnClickMe PASS [Window::sendEvent]
getText clickResultLabel PASS (text="按钮被点击了 1 次") [property::text]
doubleClick PASS [Window::doubleClick]
setFocus PASS [Accessibility::setFocus]
```

修复前这 3 条中的 click / doubleClick / getText 均为 FAIL（或误报 PASS 但断言不符），修复后 9/9 全通。

相关代码：`src/inject/InputSimulator.cpp`、`src/inject/InputSimulator.h`

---

## 2. `QLocalSocket::flush()` 返回 `false` 不代表写入失败

### 现象

注入侧日志里，小响应 `flush=OK`、大响应（5304 字节的 `dumpTree`）`flush=FAIL`，看上去像"大响应写入失败"。但 E2E 侧同一时刻 `dumpTree PASS`，数据完整收到——日志与事实矛盾。

### 根因

Windows 上 `QLocalSocket::flush()` 内部等价于 `pipeWriter->waitForWrite(0)`，**超时为 0 毫秒**。它返回 `false` 的语义只是"异步写未在 0ms 内排空"：

- 小响应（几十字节）能即时排空 → 返回 `true`；
- 大响应（数 KB）必然来不及 → 返回 `false`，随后由事件循环正常排空，对端完整收到。

把这个返回值记成 `FAIL` 有两重危害：一是得出"大响应写入失败"的错误结论、浪费排查时间；二是**掩盖真正的故障**——短写或 `write()` 返回 `-1` 才是真问题，而它需要独立比对写入字节数才能发现。

另外，`flush()` 本身已经是非阻塞的，不存在"把它改成非阻塞"的优化空间。

### 正确做法

- 成功判据只能是 `written == data.size()`（`-1` 为失败）；
- `flush()` 保留用于催动异步写，但**不据其返回值判定成败**；
- 需要表达"尚未落盘"时记录 `bytesToWrite()` 作为 `pending` 字节数——这是事实陈述，不是成败判断；
- 仅在 `written != data.size()` 时输出告警行。

```cpp
const qint64 written = g_pipe->write(respData);
g_pipe->flush();   // 仅催动异步写，返回值与成败无关
winLog(... "bytes=%3/%4 pending=%5" ... .arg(g_pipe->bytesToWrite()));
if (written != respData.size())
{
    // 真故障：短写或 write 失败，响应必然不完整
}
```

### 验证依据

改后日志把真相说清楚了：

```
RESP -> dumptree status=ok bytes=5304/5304 pending=5304   ← 全量写入，待事件循环排空
RESP -> click    status=ok bytes=71/71     pending=0      ← 已即时排空
```

`bytes=5304/5304` 说明 `write()` 全量接收（成功），`pending=5304` 说明此刻一字节都还没落盘（所以 0ms 的 `flush()` 自然返回 false）。小响应 `pending=0` 与大响应 `pending=5304` 的对比，反过来印证了 `flush()` 返回值只反映"0ms 内是否排空"。

相关代码：`src/inject/dllmain.cpp`

---

## 3. E2E 测试在第二条命令后静默死亡

### 现象

测试进程跑完第一条命令后无任何日志、直接消失，既没有崩溃提示，也没有 `Tests Done` 汇总行。

### 根因

`inject_ready` 握手的处理器是挂在 socket `readyRead` 信号上的 lambda，而 `runTests()` 在其中被**直接调用**。于是 `sendCommand()` 里的 `waitForReadyRead()` 嵌套在同一个 socket 的 `readyRead` 槽内部，造成 `QWindowsPipeReader` 重入——响应被外层 lambda 吞掉，进而重入崩溃。

### 正确做法

握手处理器先 `QObject::disconnect()` 断开自己，再用 `QTimer::singleShot(0, ...)` 把 `runTests()` 抛到事件循环顶层执行，彻底避免阻塞读嵌套在 `readyRead` 内部。

相关代码：`tests/e2e_test.cpp`

---

## 4. 大响应被 `readLine()` 截断

### 现象

`dumpTree` 这类大响应触发同步读取死锁，一度只能把该用例注释掉规避。

### 根因

`readLine()` 在数据分片到达时会返回半包，调用方误以为收到完整一帧。

### 正确做法

双向统一为 **NDJSON**（单行 JSON + `\n`）：

- 发送侧：每帧追加 `\n`；
- 接收侧：用缓冲区累积 + `indexOf('\n')` 组帧，配合 `QElapsedTimer` 做统一超时预算，不使用 `readLine()`；
- 超时报错时带上已缓冲字节数，便于区分"没回包"和"回包不完整"。

注入侧同时按行拆包循环消费，并加重入保护：命令执行期间若嵌套触发 `readyRead`，只收数据、由外层循环继续消费。

相关代码：`tests/e2e_test.cpp`、`src/inject/dllmain.cpp`

---

## 5. 构建与运行环境约束

- **源码含中文字面量的 target 必须加 `/utf-8`**。E2E 断言的期望值是中文，MSVC 缺省会按 GBK 解读，导致断言必然失败。已在 `tests/CMakeLists.txt` 与 `src/inject/CMakeLists.txt` 中设置。
- **构建产物必须自动同步**。`QtUIAuto_E2E` 的 POST_BUILD 会把 `QtUIAuto_TestTarget.exe` 与 `QtUIAuto_Inject.dll` 拷到 E2E 输出目录。曾因依赖手工拷贝而跑到过期二进制，白白排查了一轮已修好的问题——排查前务必先核对二进制时间戳与源码修改时间。
- **E2E 需要在沙箱外运行**。注入涉及写入其他进程与 `%TEMP%`，沙箱会拒绝执行。非管理员权限即可（日志中 `Running elevated: NO`）。
