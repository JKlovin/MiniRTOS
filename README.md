# MiniRTOS

> 基于 STM32F103C8T6 / ARM Cortex-M3 的自研轻量级实时操作系统（学习与实验项目）

[![Platform](https://img.shields.io/badge/Platform-STM32F103C8T6-blue)](https://www.st.com/)
![Language](https://img.shields.io/badge/Language-C-green)
![Status](https://img.shields.io/badge/Status-Development-orange)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 项目简介

MiniRTOS 是一个从零学习并实现的轻量级实时操作系统内核，运行平台为 STM32F103C8T6（ARM Cortex-M3）。项目以理解 RTOS 底层原理为主要目标，围绕任务创建、优先级调度、上下文切换、任务同步和任务间通信等核心功能展开。

与直接调用现成 RTOS API 不同，本项目重点实现并验证 Cortex-M 异常机制、任务栈组织、就绪/阻塞链表与调度过程。在后续开发中，将利用真实应用场景反馈内核问题，再进行迭代与测试。

> **注意**：本项目处于持续开发和验证阶段，面向学习、实验和原型开发，目前不宣称达到工业级实时性、功能安全或生产环境可靠性要求。

## 开发环境

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F103C8T6 |
| CPU | ARM Cortex-M3 |
| 开发语言 | C、少量 ARM 汇编 |
| 外设初始化 | STM32CubeMX / STM32 HAL |
| 编译环境 | Keil MDK-ARM（ARM Compiler 5 开发环境） |
| 调试方式 | SWD、串口输出 |
| 内核目录 | `MiniRTOS/` |

## 内核架构

```text
应用任务 / 内核测试
        |
        v
任务管理与调度   <---->   队列 / 信号量 / 互斥锁 / 任务通知
        |
        v
就绪链表 / 阻塞管理 / 临界区
        |
        v
Cortex-M3：SVC / PendSV / SysTick（及工程配置的时间基准）
        |
        v
STM32F103 硬件与 HAL
```

任务运行在 PSP 所管理的任务栈上，利用 SVC 启动首个任务、PendSV 完成上下文切换，结合系统时钟节拍与内核链表实现任务调度。具体中断时间基准以工程配置和源码为准。

## 功能与开发状态

以下状态区分“已编写/初步运行”和“已完成全面验证”，不能将基础测试通过视为边界条件全部覆盖。

| 模块 | 状态 | 说明 |
| --- | --- | --- |
| 任务创建与任务栈初始化 | ✅ 已实现 | TCB、任务栈及初始上下文 |
| 优先级调度与上下文切换 | ✅ 已实现 | SVC、PendSV 与优先级就绪列表 |
| 任务延时和阻塞唤醒 | ✅ 已实现基础功能 | 后续补充统一超时与竞态测试 |
| 临界区保护 | ✅ 已实现 | 需要进一步覆盖嵌套/中断场景 |
| 互斥锁与优先级继承 | 🧪 已实现并初测 | 多锁、超时及优先级恢复待强化 |
| 计数信号量 | 🧪 已实现并初测 | 边界条件及 ISR 访问待验证 |
| 消息队列 | 🧪 已实现并初测 | 并发、满/空队列和超时测试待完善 |
| 任务通知 | 🧪 已实现并初测 | 通知模式及竞争场景待完善 |
| ISR 专用通信接口 | 📌 规划中 | 明确中断上下文使用限制 |
| 内存池、栈水位与运行统计 | 📌 规划中 | 提高资源可观测性 |
| 完整的自动化测试与性能基准 | 📌 规划中 | 调度延迟、IPC 延迟和压力测试 |

> 表格是当前阶段的项目说明，具体 API、测试覆盖范围请以仓库最新源码及后续测试报告为准。

## 仓库结构

```text
MiniRTOS/
├── Core/                 # CubeMX 生成的核心程序与中断入口
├── Drivers/              # STM32 HAL / CMSIS
├── MDK-ARM/              # Keil 工程文件
├── MiniRTOS/             # 自研内核源码
│   ├── minirtos.h
│   ├── minirtos_task.c/.h
│   ├── minirtos_event.c/.h
│   └── minirtos_queue.c/.h
├── F103.ioc              # CubeMX 配置
├── .gitignore
└── LICENSE
```

## 构建与运行

1. 克隆仓库：
   ```bash
   git clone https://github.com/JKlovin/MiniRTOS.git
   ```
2. 使用 Keil MDK-ARM 打开 `MDK-ARM/` 中的工程文件。
3. 确认已安装适用的 STM32F1 Device Pack 和 ARM Compiler 工具链，并检查目标器件、时钟和下载器配置。
4. 编译，通过 SWD 下载至 STM32F103C8T6。
5. 通过串口及 LED 观察当前 `main.c` 中的内核测试任务；不同提交可能具有不同测试内容。

**提示**：修改 `.ioc` 重新生成代码时，应检查 `stm32f1xx_it.c` 的 SVC/PendSV 处理函数、时钟源以及 HAL 时间基准，避免覆盖自研内核的异常入口或造成重复 Tick。

## 后续开发计划

- [ ] 统一队列、信号量、互斥锁及任务通知的阻塞超时机制
- [ ] 增加事件唤醒与超时竞争的回归测试
- [ ] 完善多把互斥锁场景下的优先级继承与恢复
- [ ] 增加中断上下文可用的通信接口
- [ ] 实现任务栈水位、运行状态和 CPU 占用率统计
- [ ] 建立上下文切换耗时及任务响应抖动测试
- [ ] 在真实综合应用中验证与改进内核

## 关联项目

**[MiniRTOS-Monitor](https://github.com/JKlovin/MiniRTOS-Monitor)**：基于瑞士军刀 STM32F103 开发板的实时运动状态监测与故障容错系统。应用项目独立维护，在实际运行中发现的通用内核问题将回到本仓库修复和测试，再同步到应用工程。

## 许可证

本项目采用 [MIT License](LICENSE)。
