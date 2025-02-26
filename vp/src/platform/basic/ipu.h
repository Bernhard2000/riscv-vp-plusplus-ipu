#ifndef RISCV_ISA_IPU_H
#define RISCV_ISA_IPU_H

#include <tlm_utils/simple_target_socket.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <systemc>

#include "core/common/irq_if.h"

#define IPU_MAX_WIDTH 1920
#define IPU_MAX_HEIGHT 1080
#define IPU_FRAME_BUFFER_SIZE (IPU_MAX_WIDTH * IPU_MAX_HEIGHT)

#define VERBOSE true

#define OUTPUT_BUFFER_SIZE (IPU_MAX_WIDTH * IPU_MAX_HEIGHT)

struct IPU : public sc_core::sc_module {
	tlm_utils::simple_target_socket<IPU> tsock;

	interrupt_gateway *plic = 0;
	uint32_t irq_number = 0;
	sc_core::sc_event process_event;

	uint32_t enable = 0;  // TODO should probably be boolean flag, or event based

	// memory mapped frame buffer at offset 0x0
	unsigned char *frame_buffer = 0;
	unsigned char *output_buffer = 0;


	// Configuration registers
	uint32_t input_width = 640;   // default width
	uint32_t input_height = 480;  // default height
	uint32_t scale_factor = 1;    // default scale factor
	uint32_t rotation_angle = 0;  // rotation angle in degrees (0, 90, 180, 270)

	uint32_t output_width = 0;
	uint32_t output_height = 0;

	std::unordered_map<uint64_t, uint32_t *> addr_to_reg;

	enum {
		INPUT_WIDTH_ADDR = 0xff0000,
		INPUT_HEIGHT_ADDR = 0xff0004,
		SCALE_FACTOR_ADDR = 0xff0008,
		ROTATION_ANGLE_ADDR = 0xff000c,
		ENABLE_REG_ADDR = 0xff0010,
		OUTPUT_WIDTH_ADDR = 0xff0014,
		OUTPUT_HEIGHT_ADDR = 0xff0018
	};

	SC_HAS_PROCESS(IPU);

	IPU(sc_core::sc_module_name, uint32_t irq_number) : irq_number(irq_number) {
		tsock.register_b_transport(this, &IPU::transport);

		frame_buffer = new unsigned char[IPU_FRAME_BUFFER_SIZE];
		output_buffer = new unsigned char[IPU_FRAME_BUFFER_SIZE];
		addr_to_reg = {
		    {INPUT_WIDTH_ADDR, &input_width},
		    {INPUT_HEIGHT_ADDR, &input_height},
		    {SCALE_FACTOR_ADDR, &scale_factor},
		    {ROTATION_ANGLE_ADDR, &rotation_angle},
		    {ENABLE_REG_ADDR, &enable},
			{OUTPUT_WIDTH_ADDR, &output_width},
			{OUTPUT_HEIGHT_ADDR, &output_height}
		};
		SC_THREAD(processing_thread);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();

		if (addr < IPU_FRAME_BUFFER_SIZE) {
			// Access frame buffer
			assert(cmd == tlm::TLM_READ_COMMAND || cmd == tlm::TLM_WRITE_COMMAND);
			assert((addr + len) <= IPU_FRAME_BUFFER_SIZE);
			if(cmd == tlm::TLM_WRITE_COMMAND)
				memcpy(&frame_buffer[addr], ptr, len);
			else
				memcpy(ptr, &frame_buffer[addr], len);
		} else {
			assert(len == 4);  // NOTE: only allow to read/write whole register
			fprintf(stderr, "Addr: %x\n", addr);

			// Register access
			auto it = addr_to_reg.find(addr);
			assert(it != addr_to_reg.end());  // access to non-mapped address

			if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == INPUT_WIDTH_ADDR)) {
				uint32_t value = *((uint32_t *)ptr);
				if (value > IPU_MAX_WIDTH)  // greater than max?
					return;                 // ignore invalid values
			}
			if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == INPUT_HEIGHT_ADDR)) {
				uint32_t value = *((uint32_t *)ptr);
				if (value > IPU_MAX_HEIGHT)  // greater than max?
					return;                  // ignore invalid values
			}
			if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == ROTATION_ANGLE_ADDR)) {
				uint32_t value = *((uint32_t *)ptr);
				if (value > 360 || value < -360)  // greater than max?
					return;                       // ignore invalid values
			}
			if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == SCALE_FACTOR_ADDR)) {
				uint32_t value = *((uint32_t *)ptr);
				if (value < 0)
					return;  // ignore invalid values
			}

			// actual read/write
			if (cmd == tlm::TLM_READ_COMMAND) {
				*((uint32_t *)ptr) = *it->second;  // read the register
			} else if (cmd == tlm::TLM_WRITE_COMMAND) {
				*it->second = *((uint32_t *)ptr);  // write the register
			} else {
				assert(false && "unsupported tlm command for ipu access");
			}

			// trigger post read/write actions
			if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == ENABLE_REG_ADDR)) {
				process_event.cancel();
				if (enable) {
					process_event.notify(sc_core::sc_time(1e7, sc_core::SC_US)); //TODO remove time, just a placeholder for now
				}
			}
		}
	}

	void processing_thread() {
		unsigned int n = 0;
		char infilename[70];
		while (true) {
			fprintf(stderr, "%s: %s waiting for capture event.\n", sc_time_stamp().to_string().c_str(), name());
			if (enable) {
				process_event.notify(sc_core::sc_time(1e7, sc_core::SC_US)); //TODO remove time, just a placeholder for now
			}
			sc_core::wait(process_event);
			output_width = input_width;
			output_height = input_height;
			
			/*//fill frame buffer with input image mirrored on the y axis (flippes 180 degrees)
			for (unsigned h = 0; h < input_height; h++) {
				for (unsigned w = 0; w < input_width; w++) {
					output_buffer[h * input_width + w] = frame_buffer[(input_height-1 - h)* input_width + w];
				}
			}*/
			rotate(25);

			//memcpy(frame_buffer, output_buffer, IPU_FRAME_BUFFER_SIZE);
			output_buffer = frame_buffer;
			plic->gateway_trigger_interrupt(irq_number);
			enable = false;
			n++;
		}
	}

   private:
	void process_image() {
		// First perform scaling
		scale_image();

		// Then perform rotation
		rotate_image();
	}

	void scale_image() {
		fprintf(stderr, "Scaling");
		// Simple nearest neighbor scaling
		uint32_t output_width = input_width * scale_factor;
		uint32_t output_height = input_height * scale_factor;
	}

	void rotate_image() {
		// Only support 90-degree rotations
		switch (rotation_angle) {
			case 0:
				// Do fuck all
				break;
			case 90:
				rotate_90();
				break;
			case 180:
				rotate_180();
				break;
			case 270:
				rotate_270();
				break;
			default:
				rotate(rotation_angle);
				break;
		}
	}

	void rotate_90() {
		uint32_t output_width = input_height * scale_factor;
		uint32_t output_height = input_width * scale_factor;
		unsigned char *temp = new unsigned char[OUTPUT_BUFFER_SIZE];
		for (uint32_t y = 0; y < output_height; y++) {
			for (uint32_t x = 0; x < output_width; x++) {
				temp[x * output_height + (output_height - 1 - y)] = output_buffer[y * output_width + x];
			}
		}
		memcpy(output_buffer, temp, OUTPUT_BUFFER_SIZE);
		delete[] temp;
	}

	void rotate_180() {
		uint32_t output_width = input_width * scale_factor;
		uint32_t output_height = input_height * scale_factor;
		unsigned char *temp = new unsigned char[OUTPUT_BUFFER_SIZE];
		for (uint32_t y = 0; y < output_height; y++) {
			for (uint32_t x = 0; x < output_width; x++) {
				temp[(output_height - 1 - y) * output_width + (output_width - 1 - x)] =
				frame_buffer[y * output_width + x];
			}
		}
		memcpy(frame_buffer, temp, OUTPUT_BUFFER_SIZE);
		delete[] temp;
	}

	void rotate_270() {
		uint32_t output_width = input_height * scale_factor;
		uint32_t output_height = input_width * scale_factor;
		unsigned char *temp = new unsigned char[OUTPUT_BUFFER_SIZE];
		for (uint32_t y = 0; y < output_height; y++) {
			for (uint32_t x = 0; x < output_width; x++) {
				temp[(output_width - 1 - x) * output_height + y] = output_buffer[y * output_width + x];
			}
		}
		memcpy(output_buffer, temp, OUTPUT_BUFFER_SIZE);
		delete[] temp;
	}

	//rotate arbitrary degrees (untested and mostly as a placeholder, might be garbage)
	void rotate(uint32_t deg) {
		//uint32_t output_width = input_width * scale_factor; //TODO allow adjusting of output width and height
		//uint32_t output_height = input_height * scale_factor;
		output_buffer = new unsigned char[OUTPUT_BUFFER_SIZE];
		unsigned char *temp = new unsigned char[OUTPUT_BUFFER_SIZE];
		
		double angle = deg * M_PI / 180.0;
		// Taylor series approximation for cos(x) up to 6th term
		double x = angle;
		while (x > 2*M_PI) x -= 2*M_PI;  // normalize angle
		while (x < -2*M_PI) x += 2*M_PI;
		double cos_angle = 1 - (x*x)/2 + (x*x*x*x)/24 - (x*x*x*x*x*x)/720;
		
		// For sin(x), we use the relation sin(x) = cos(x - PI/2)
		x = angle - M_PI/2;
		while (x > 2*M_PI) x -= 2*M_PI;
		while (x < -2*M_PI) x += 2*M_PI;
		double sin_angle = 1 - (x*x)/2 + (x*x*x*x)/24 - (x*x*x*x*x*x)/720;

		double abs_sin = sin_angle;
		if (abs_sin < 0) abs_sin = -abs_sin;
		double abs_cos = cos_angle;
		if (abs_cos < 0) abs_cos = -abs_cos;
		output_width = (uint32_t)(input_width * abs_cos + input_height * abs_sin)+3;
		output_height = (uint32_t)(input_width * abs_sin + input_height * abs_cos)+3;
		
		
		fprintf(stderr, "Rotating image by %d degrees, new dimensions: %d x %d\n", deg, output_width, output_height);

		float center_x = input_width / 2.0f;
		float center_y = input_height / 2.0f;

		float out_center_x = output_width / 2.0f;
		float out_center_y = output_height / 2.0f;

		for(int y = 0; y < output_height; y++) {
			for(int x = 0; x < output_width; x++) {
				float dx = x - out_center_x;
				float dy = y - out_center_y;
				float src_x = (dx * cos_angle + dy * sin_angle + center_x);
				float src_y = (-dx * sin_angle + dy * cos_angle + center_y);
				
				

				if(src_x >= 0 && src_x < input_width && src_y >= 0 && src_y < input_height) {
					int ix = (int)src_x;
					int iy = (int)src_y;
					temp[y * output_width + x] = frame_buffer[iy * input_width + ix];
				} else {
					temp[y * output_width + x] = 255; // Fill outside areas with black
				}
			}
		}
		memcpy(frame_buffer, temp, IPU_FRAME_BUFFER_SIZE);
		delete[] temp;
	} 
};
#endif  // RISCV_ISA_IPU_H
