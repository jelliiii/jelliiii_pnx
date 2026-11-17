#  board 支持

`boards/` 提供 board 相关代码和公共库，由 `CMakeLists.txt` 统一接入。

## 目录

| 目录 | 作用与划分原因 |
| --- | --- |
| `cubemx/` | 按 board 保存 CubeMX 工程、生成代码和厂商依赖，便于独立配置与重新生成。 |
| `dsp/` | 公共 CMSIS-DSP 静态库，`include/` 放头文件，`src/` 放实现，供不同 board 共用。 |
| `memory/` | 按 board 保存链接脚本，集中维护 Flash、RAM 和堆栈布局。 |
| `wrapper/` | 按 board 提供 C++ 外设配置、HAL 资源查询、DMA 缓冲区和固件启动入口。 |

每个 CubeMX 工程中：

- `Core/`：硬件初始化、中断和系统代码。
- `Drivers/`：CMSIS 与对应芯片系列的 HAL/LL 驱动。
- `Middlewares/`：ThreadX、USBX 等组件实现。
- `AZURE_RTOS/`、`USBX/`：RTOS 和 USB 的应用接入与平台配置。
- `cmake/`：生成代码的构建清单和工具链配置。
- `.ioc`：CubeMX 配置源文件；启动汇编和 `.ld` 文件提供启动与内存布局。

##  board 选择

```cmake
set(BOARD "h723_mc02" CACHE STRING "Target board")
```

支持 `h723_mc02`（默认）和 `f407_cboard`。外部通过 CMake 参数
`-DBOARD=f407_cboard`，或 Presets 的 `cacheVariables.BOARD` 选择。
选择会保存在构建目录的缓存中，无效名称会报错。

`BOARD` 是 CMake 使用的板卡目录名，不是 C/C++ 预处理宏。各板实际导出的芯片宏和 HAL
头文件如下：

| `BOARD` | 芯片宏 | HAL 头文件 |
| --- | --- | --- |
| `h723_mc02` | `STM32H723xx` | `stm32h7xx_hal.h` |
| `f407_cboard` | `STM32F407xx` | `stm32f4xx_hal.h` |

宏在 `boards/cubemx/<board>/cmake/stm32cubemx/CMakeLists.txt` 的
`MX_Defines_Syms` 中定义，并通过 `stm32cubemx` target 传递给应用。写芯片条件编译时使用
芯片宏（如 `#if defined(STM32H723xx)`），不要使用 `BOARD` 名称，也不要用 HAL 头文件的
include guard 判断芯片。可以从 CMake 配置输出的 `Target board` 确认当前选板；排查实际
编译参数时，查看对应构建目录的 `compile_commands.json` 中 `-DSTM32...` 参数。

## wrapper 接口与使用

`wrapper/${BOARD}/include/` 中的头文件通过 `board` CMake INTERFACE target
导出，要求 C++17。选择 board 后，使用相同的头文件名即可访问对应实现：

| 文件 | 接口与职责 |
| --- | --- |
| `config.hpp` | 外设类型、`bsp::peripherals` 资源常量和 `board` 板载器件接线，不依赖 HAL/BSP 实现。CAN 配置由 BSP 定义。 |
| `peripherals.hpp` | `bsp::peripherals` 下的 HAL 资源查询，包含 `config.hpp`。 |
| `memory.hpp` | `memory::buffer<N>`、`memory::valid()`、`memory::cache_line_size` 和 `RAM_D1_BSS`，用于声明及检查 DMA 缓冲区。 |
| `../src/init.cpp` | 提供 `main()`，完成 HAL、时钟及 CubeMX 外设初始化，然后调用 `tx_kernel_enter()`；H723 还启用 I/D-cache。 |

配置常量描述当前 CubeMX 配置，查询函数只解析资源；二者都不会初始化或启动外设。
CAN BSP 直接使用 CubeMX 的 `hcan*` / `hfdcan*` 句柄，wrapper 不提供 CAN 映射层。
依赖方向为应用 → BSP → board → HAL；board 不引用 BSP 或 `pnxtypes`。
应用应在启动初始化完成后（例如 ThreadX 线程中）调用 HAL 或上层驱动。
修改 `.ioc` 并重新生成后，也需要同步更新 wrapper 的配置常量和初始化顺序。

### CMake 接入

顶层创建固件 target 后接入 `boards`，并链接 `board`：

```cmake
# 在已配置工具链、启用 C/CXX/ASM 的顶层工程中：
add_executable(${CMAKE_PROJECT_NAME})
add_subdirectory(boards)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE board)
```

本仓库的 `bsp` target 已传递链接 `board`，固件链接 `bsp` 即可获得 wrapper。
`init.cpp` 由 wrapper 的 CMake 直接加入固件 target，因此应用无需再定义 `main()`。
CubeMX 已配置为不生成 `main()`，实际入口位于 wrapper 的 `src/init.cpp`。
根工程直接引用 `cubemx/<board>/cmake/stm32cubemx`，不依赖生成工程的顶层 CMake。
ThreadX 和 USBX 的应用入口由 BSP 替代，具体源文件选择见 [BSP 文档](../bsp/README.md)。
重新生成后无需向 `AZURE_RTOS/App` 或 `USBX/App` 插入应用代码。

### 配置类型与板级资源

配置类型均位于 `bsp` 命名空间：

| 类型 | 字段含义 |
| --- | --- |
| `active_level` | `low` / `high`，表示逻辑有效电平。 |
| `dma_config` | `rx`、`tx` 表示收发 DMA 配置，默认均为 `false`。 |
| `spi::config` | `instance`、`dma` 和外设中断使能标志 `irq`。 |
| `usart::config` | `instance` 和 `dma`。UART 与 USART 均使用此类型。 |
| `gpio::config` | 端口 `instance`、引脚编号 `pin`（0～15，非 HAL 位掩码）和 `active`（默认 `high`）。 |
| `pwm::config` | 预分频前的定时器内核时钟 `timer_clock_hz`、`instance`、通道掩码 `channel_mask`（位 0～3 对应 CH1～CH4）。时钟值不是 PWM 输出频率。 |
| `adc::config`（仅 H723） | ADC 通道编号 `channel`、序列位置 `rank`、采样时间枚举 `sampling` 和 `instance`。 |

下表 CAN 名称指 `bsp::can::handle` 枚举，其余指 `bsp::peripherals` 中的配置常量；DMA 顺序为 RX / TX：

| 资源 | `h723_mc02` | `f407_cboard` |
| --- | --- | --- |
| CAN | `fdcan1`：FD+BRS；`fdcan2`、`fdcan3`：classic | `can1`、`can2`：classic |
| SPI | `spi1`：DMA 关/关，IRQ 开；`spi2`：DMA 开/开，IRQ 开；`spi6`：DMA 关/关，IRQ 关 | `spi1`：DMA 开/开，IRQ 关 |
| 串口 | `uart5`、`usart10`：DMA 开/关；`uart7`、`usart1`：DMA 开/开 | `usart3`：DMA 开/关；`usart1`、`usart2`、`usart6`：DMA 开/开 |
| PWM | `tim3`：260 MHz、CH4；`tim12`：260 MHz、CH2 | `tim1`：168 MHz、CH1～4；`tim4`：84 MHz、CH3；`tim8`：168 MHz、CH1～3；`tim10`：168 MHz、CH1 |
| ADC | `adc1`：通道 4、rank 1、1.5 cycles | 未提供 ADC 接口 |
| 通用 GPIO | `ps2_cs`、`pc13`、`pc14`、`pc15` | `pc8`、`pg6` |

未命名用途的 GPIO 的 `active` 需按电路选择，CubeMX 初始输出电平不代表逻辑有效电平。
CAN 的经典/FD 模式、标识符类型和 TDC 由 BSP 配置；CAN 时序及 FIFO 大小仍由 CubeMX 生成代码维护。

### 板载器件接线

接线统一放在各板的 `config.hpp`，使用 `board::bmi088`、`board::led`。
设备驱动通过参数接收接线，配置不承担初始化或读写。
接线依据参考仓库 `penix/configs/boards/{h723_dmmc02,f407_cboard}/board.json`，
并与当前 IOC 核对；不依赖该参考仓库进行构建。

| 接口 | H723 MC02 | F407 C 板 |
| --- | --- | --- |
| `board::bmi088::spi` | SPI2 | SPI1 |
| `board::bmi088::acc_cs` | PC0，低有效 | PA4，低有效 |
| `board::bmi088::gyro_cs` | PC3，低有效 | PB0，低有效 |
| `board::bmi088::acc_drdy` | PE10，高有效 | 未导出：PC4 未在 IOC 配置 |
| `board::bmi088::gyro_drdy` | PE12，高有效 | PC5，高有效 |
| `board::bmi088::heater_timer` / `heater_channel` | TIM3 / CH4 | TIM10 / CH1 |
| `board::led::spi` | SPI6，WS2812 数据输出 | 不提供 |
| `board::led::red/green/blue` | 不提供 | PH12/PH11/PH10，高有效 |

H723 的 DRDY 使用上升沿 EXTI。C 板参考接线为高有效 DRDY，但当前 IOC 配置 PC5
下降沿 EXTI；启用中断采样前需要统一传感器中断极性与 IOC 触发沿，再重新生成。
这里仅描述接线，不修改生成代码或默认为中断采样已经可用。
两种 LED 的控制方式不同，驱动应按板卡选择实现，不把 WS2812 当作三路 GPIO。

原来的 H723 `bsp::peripherals::acc_cs/gyro_cs/acc_int/gyro_int` 和 C 板
`pa4/pb0/pc5/ph10/ph11/ph12` 已由上述器件名称替代，避免重复维护接线。

C 板在 GPIO 初始化完成后点亮红灯：

```cpp
#include "config.hpp"
#include "bsp_gpio.hpp"

void show_error()
{
    (void)bsp::gpio::set_active(board::led::red, true);
}
```

### HAL 资源查询

以下函数均位于 `bsp::peripherals` 命名空间：

| 函数 | 返回值与无效值约定 |
| --- | --- |
| `spi_handle(spi::handle)` | `SPI_HandleTypeDef*`；无效实例返回 `nullptr`。 |
| `usart_handle(usart::handle)` | `UART_HandleTypeDef*`；无效实例返回 `nullptr`。 |
| `tim_handle(pwm::handle)` | `TIM_HandleTypeDef*`；无效实例返回 `nullptr`。 |
| `pwm_channel(pwm::handle, pwm::channel)` | 已配置通道对应的 `TIM_CHANNEL_x`；实例、通道无效或通道未启用时返回 `UINT32_MAX`。`TIM_CHANNEL_1` 为 0，不能用 0 判断失败。 |
| `gpio_port(gpio::port)` | `GPIO_TypeDef*`；无效端口返回 `nullptr`。 |
| `gpio_pin(uint8_t)` | 引脚编号对应的 HAL 位掩码；编号大于 15 返回 0。 |
| `adc_handle(adc::handle)`（H723） | `ADC_HandleTypeDef*`；无效实例返回 `nullptr`。 |
| `adc_channel(uint8_t)` / `adc_rank(uint8_t)`（H723） | 当前仅支持通道 4 / rank 1，返回对应 HAL 常量；其他编号返回 `UINT32_MAX`。 |
| `adc_sampling_time(adc::sampling_time)`（H723） | 返回 ADC1/2 的 HAL 采样时间常量；无效枚举返回 `UINT32_MAX`，不可直接用于 ADC3 或 F407。 |

CAN 的实例映射、原生帧头、DLC、过滤器、FIFO 和 HAL 调用
分别在 `bsp/can/src/bxcan.cpp` 和 `fdcan.cpp` 中实现，由 CMake 按板选择一份。
配置和帧类型统一定义在 `bsp_can.hpp`。
经典 CAN 与 CAN FD 的配置和使用示例见 [BSP 文档](../bsp/README.md)。
H723 当前 RX/TX 元素均为 8 字节；使用更大 FD 帧需要同步更新 IOC 的容量和消息 RAM 布局。

例如，两块板都提供 `usart1`，可以在初始化完成后发送数据：

```cpp
#include "peripherals.hpp"

HAL_StatusTypeDef send_byte(uint8_t value)
{
    const auto& config = bsp::peripherals::usart1;
    auto* uart = bsp::peripherals::usart_handle(config.instance);
    if (uart == nullptr)
        return HAL_ERROR;
    return HAL_UART_Transmit(uart, &value, 1, 100); // 阻塞发送，超时 100 ms
}
```

PWM 示例使用 H723 的 TIM3 CH4；F407 可改为 `tim4` 和 `ch3`：

```cpp
#include "peripherals.hpp"

HAL_StatusTypeDef start_pwm()
{
    const auto& config = bsp::peripherals::tim3;
    auto* timer = bsp::peripherals::tim_handle(config.instance);
    const auto channel = bsp::peripherals::pwm_channel(
        config.instance, bsp::pwm::channel::ch4);
    if (timer == nullptr || channel == UINT32_MAX)
        return HAL_ERROR;
    return HAL_TIM_PWM_Start(timer, channel);
}
```

### DMA 缓冲区

`memory::buffer<N>` 中 `N` 为有效载荷字节数，必须大于 0。`data()` 返回数据指针，
`operator[]` 访问字节且不检查边界；`logical_size()` 返回 `N`，`capacity()` 返回含
对齐填充的实际存储字节数。H723 按 32 字节对齐并向上取整，F407 按 1 字节对齐。

`memory::valid(buffer)` 检查整个存储区的地址范围和对齐：H723 要求位于
`[0x24000000, 0x24050000)`，F407 要求位于 `[0x20000000, 0x20020000)`。
它不会检查 DMA 是否空闲或维护缓存。

```cpp
#include "memory.hpp"

// 文件作用域，确保异步 DMA 期间对象持续存在。
RAM_D1_BSS static memory::buffer<100> rx_buffer;

bool prepare_rx_buffer()
{
    if (!memory::valid(rx_buffer))
        return false;
    // H723 的该段不会在启动时清零，使用前显式初始化。
    for (uint32_t i = 0; i < rx_buffer.capacity(); ++i)
        rx_buffer[i] = 0;
    return true;
}
```

H723 的 `RAM_D1_BSS` 将对象放入链接脚本的 `.ram_d1_bss`（NOLOAD）段，
该段不会在启动时清零；普通静态变量和栈默认位于 DTCM，DMA1/2 无法访问。
F407 的宏为空，静态缓冲区使用普通 RAM，不应放入 DMA 无法访问的 CCM。
异步传输期间必须保持缓冲区存活，避免 CPU 与 DMA 同时修改同一缓冲区。
H723 已启用 D-cache，驱动还需按传输方向维护缓存：TX 前 clean，RX 前妥善
clean/invalidate、完成后 invalidate；使用对齐的 `data()` 和 `capacity()` 作为
缓存维护范围，传输长度通常使用 `logical_size()`。wrapper 不会自动执行这些操作。

## 构建与维护

- 顶层工程先指定匹配的工具链、设置 C11 或更高标准，并创建与
  `${CMAKE_PROJECT_NAME}` 同名的固件 target，再调用 `add_subdirectory(boards)`。
- 使用公共 DSP 时，将 `dsp` 链接到固件 target；新增函数需加入其源码清单。
- `BOARD` 自动选择 board 目录及 `memory/${BOARD}.ld` 链接脚本；脚本修改后会触发重新链接。
- 工具链仍由顶层指定，需与 board 匹配；切换 board 建议使用独立构建目录。已有缓存不会随默认值改变，需显式传入 `-DBOARD=h723_mc02` 切换。
- 修改引脚、时钟和外设时，优先修改 `.ioc`；手写代码尽量保留在 `USER CODE` 区域。
- 新增 board 时，添加 `cubemx/<board>/`、`memory/<board>.ld` 和 `wrapper/<board>/`（包含 `include/` 与 `src/init.cpp`），更新 `_supported_boards`，并配置对应工具链。
