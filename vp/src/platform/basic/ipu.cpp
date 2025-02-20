#include "ipu.h"
#include <cstring>
#include <cmath>

IPU::IPU(sc_core::sc_module_name name, uint32_t irq_number) 
    : sc_module(name)
    , router("IPU")
    , tsock("tsock")
    , irq_number(irq_number) {

    interrupt_gateway *plic = 0;
    tsock.register_b_transport(this, &IPU::transport);
    
    router.add_register_bank({
        {offsetof(Registers, control), &regs.control},
        {offsetof(Registers, status), &regs.status},
        {offsetof(Registers, src_addr), &regs.src_addr},
        {offsetof(Registers, dst_addr), &regs.dst_addr},
        {offsetof(Registers, width), &regs.width},
        {offsetof(Registers, height), &regs.height},
        {offsetof(Registers, operation), &regs.operation},
        {offsetof(Registers, scale_factor), &regs.scale_factor},
        {offsetof(Registers, rotation_angle), &regs.rotation_angle},
        {offsetof(Registers, color_format), &regs.color_format}
    }).register_handler(this, &IPU::register_access_callback);

    input_fb = nullptr;
    output_fb = nullptr;
    memset(&regs, 0, sizeof(regs));
}

void IPU::register_access_callback(const vp::map::register_access_t &r) {
    if (r.write) {
        if (r.addr == offsetof(Registers, control)) {
            if (r.read & CTRL_START) {
                regs.status |= STATUS_BUSY;
                process_operation();
            }
        }
    }
}

void IPU::transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    router.transport(trans, delay);
}

void IPU::process_operation() {
    switch (regs.operation) {
        case OP_SCALE:
            scale_image();
            break;
        case OP_ROTATE:
            rotate_image();
            break;
        case OP_RGB_TO_YUV:
            convert_rgb_to_yuv();
            break;
        case OP_YUV_TO_RGB:
            convert_yuv_to_rgb();
            break;
    }
    
    regs.status &= ~STATUS_BUSY;
    regs.status |= STATUS_DONE;
    
    if (regs.control & CTRL_IRQ_EN) {
        trigger_interrupt();
    }
}

void IPU::trigger_interrupt() {
    plic->gateway_trigger_interrupt(irq_number);
}

// Example implementation of scaling operation
void IPU::scale_image() {
    if (!input_fb || !output_fb) return;
    
    float scale = regs.scale_factor / 100.0f;
    int new_width = regs.width * scale;
    int new_height = regs.height * scale;
    
    // Simple nearest neighbor scaling
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int src_x = x / scale;
            int src_y = y / scale;
            
            if (regs.color_format == FMT_RGB888) {
                int src_pos = (src_y * regs.width + src_x) * 3;
                int dst_pos = (y * new_width + x) * 3;
                
                output_fb[dst_pos] = input_fb[src_pos];        // R
                output_fb[dst_pos + 1] = input_fb[src_pos + 1];// G
                output_fb[dst_pos + 2] = input_fb[src_pos + 2];// B
            }
        }
    }
}