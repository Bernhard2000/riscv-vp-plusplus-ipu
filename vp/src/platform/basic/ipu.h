#pragma once

#include <systemc>
#include <tlm_utils/simple_target_socket.h>
#include "core/common/irq_if.h"
#include "util/tlm_map.h"

struct IPU : public sc_core::sc_module {
    // TLM socket
    tlm_utils::simple_target_socket<IPU> tsock;

    // Interrupt output
    interrupt_gateway *plic;
    uint32_t irq_number;

    // Register map
    struct Registers {
        uint32_t control;          // 0x00: Control register
        uint32_t status;          // 0x04: Status register
        uint32_t src_addr;        // 0x08: Source framebuffer address
        uint32_t dst_addr;        // 0x0C: Destination framebuffer address
        uint32_t width;           // 0x10: Image width
        uint32_t height;          // 0x14: Image height
        uint32_t operation;       // 0x18: Operation selection
        uint32_t scale_factor;    // 0x1C: Scaling factor
        uint32_t rotation_angle;  // 0x20: Rotation angle
        uint32_t color_format;    // 0x24: Color format
    };

    // Register bit definitions
    enum ControlBits {
        CTRL_ENABLE = 1 << 0,
        CTRL_IRQ_EN = 1 << 1,
        CTRL_START  = 1 << 2
    };

    enum StatusBits {
        STATUS_BUSY = 1 << 0,
        STATUS_DONE = 1 << 1,
        STATUS_ERROR = 1 << 2
    };

    enum Operations {
        OP_SCALE = 0,
        OP_ROTATE = 1,
        OP_RGB_TO_YUV = 2,
        OP_YUV_TO_RGB = 3
    };

    enum ColorFormat {
        FMT_RGB888 = 0,
        FMT_RGB565 = 1,
        FMT_YUV420 = 2
    };

    // Internal state
    Registers regs;
    uint8_t* input_fb;
    uint8_t* output_fb;

    SC_HAS_PROCESS(IPU);
    IPU(sc_core::sc_module_name, uint32_t irq_number);
    
    void register_access_callback(const vp::map::register_access_t &r);
    void transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    
    // Operation handlers
    void process_operation();
    void scale_image();
    void rotate_image();
    void convert_rgb_to_yuv();
    void convert_yuv_to_rgb();
    
    // Interrupt handling
    void trigger_interrupt();
    
    private:
    vp::map::LocalRouter router;
};