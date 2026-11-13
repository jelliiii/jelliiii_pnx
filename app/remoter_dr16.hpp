#include "bsp_usart.hpp"
#include "memory.hpp"
#include "remoter.hpp"
#include "usart.h"
#include "user_config.hpp"

remotes::reading remote_data;
static memory::buffer<18> rx_buffer RAM_D1_BSS; // （默认放到 DTCM，DMA 不可访问）把缓冲区放到
                                                // RAM_D1

pnxtypes::status usart_init;
pnxtypes::status usart_config;
pnxtypes::status usart_rx_start;
pnxtypes::status usart_decode;

void remote_rx_callback(bsp::usart::handle usart, uint8_t* data, uint32_t len)
{
    if (usart != bsp::usart::handle::uart5 || data == nullptr)
    {
        return;
    }

    constexpr uint8_t frame_length = remotes::dr16::frame_size;
    static uint8_t frame[frame_length]{};
    static uint8_t buffered = 0;

    for (uint32_t i = 0; i < len; ++i)
    {
        frame[buffered++] = data[i];
        if (buffered != frame_length)
        {
            continue;
        }

        remotes::reading decoded{};
        usart_decode = remotes::dr16::decode(frame, frame_length, decoded);
        if (usart_decode == pnxtypes::status::ok)
        {
            remote_data = decoded;
            buffered = 0;
            continue;
        }

        for (uint8_t j = 1; j < frame_length; ++j)
        {
            frame[j - 1] = frame[j];
        }
        --buffered;
    }
}

void dr16_remoter_init()
{
    // 先配置再初始化
    usart_config = bsp::usart::configure(bsp::usart::handle::uart5, user_config::dr16_line);
    // init函数先检查是否是 dma 模式，若是，则检查 tx 和 rx 是否都配了 dma
    usart_init = bsp::usart::init(bsp::peripherals::uart5, bsp::usart::mode::block);

    usart_rx_start = bsp::usart::start_rx_to_idle(bsp::usart::handle::uart5, rx_buffer,
        bsp::usart::rx_callback::bind<remote_rx_callback>());
}