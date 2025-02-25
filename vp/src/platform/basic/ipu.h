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

#define VIDEONAME "jku"
#define IMG_IN "video/" VIDEONAME "%ux%u_%03u.pgm"
#define AVAIL_IMG 3 /* number of different image frames (1 or more) */
#define VERBOSE true

#define OUTPUT_BUFFER_SIZE (IPU_MAX_WIDTH * IPU_MAX_HEIGHT)

struct IPU : public sc_core::sc_module {
	tlm_utils::simple_target_socket<IPU> tsock;

	interrupt_gateway *plic = 0;
	uint32_t irq_number = 0;
	sc_core::sc_event capture_event;

	uint32_t enable = 0;  // TODO should probably be boolean flag, or event based

	// memory mapped frame buffer at offset 0x0 (i.e. 0x51000000)
	unsigned char *frame_buffer = 0;
	unsigned char *output_buffer = 0;

	// Configuration registers
	uint32_t input_width = 640;   // default width
	uint32_t input_height = 480;  // default height
	uint32_t scale_factor = 1;    // default scale factor
	uint32_t rotation_angle = 0;  // rotation angle in degrees (0, 90, 180, 270)

	std::unordered_map<uint64_t, uint32_t *> addr_to_reg;

	enum {
		INPUT_WIDTH_ADDR = 0xff0000,
		INPUT_HEIGHT_ADDR = 0xff0004,
		SCALE_FACTOR_ADDR = 0xff0008,
		ROTATION_ANGLE_ADDR = 0xff00c,
		ENABLE_REG_ADDR = 0xff0010,
	};

	SC_HAS_PROCESS(IPU);

	IPU(sc_core::sc_module_name, uint32_t irq_number) : irq_number(irq_number) {
		tsock.register_b_transport(this, &IPU::transport);

		frame_buffer = new unsigned char[IPU_FRAME_BUFFER_SIZE];

		addr_to_reg = {
		    {INPUT_WIDTH_ADDR, &input_width},
		    {INPUT_HEIGHT_ADDR, &input_height},
		    {SCALE_FACTOR_ADDR, &scale_factor},
		    {ROTATION_ANGLE_ADDR, &rotation_angle},
		    {ENABLE_REG_ADDR, &enable}
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
			assert(cmd == tlm::TLM_READ_COMMAND);
			assert((addr + len) <= IPU_FRAME_BUFFER_SIZE);
			memcpy(ptr, &frame_buffer[addr], len);
		} else {
			assert(len == 4);  // NOTE: only allow to read/write whole register

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
				capture_event.cancel();
				if (enable) {
					capture_event.notify(sc_core::sc_time(1e7, sc_core::SC_US)); //TODO remove time, just a placeholder for now
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
				capture_event.notify(sc_core::sc_time(1e7, sc_core::SC_US)); //TODO remove time, just a placeholder for now
			}
			sc_core::wait(capture_event);

			/*// capture an image into the frame buffer
			sprintf(infilename, IMG_IN, input_width, input_height, (n % AVAIL_IMG) + 1);
			if (read_pgm_image(infilename, frame_buffer, input_height, capture_width) == 0) {
				//	// no suitable image file found, make a monotone one
				//      memset(frame_buffer, (n*32)%256, capture_height*capture_width);
				// no suitable image file found, make a diagonally striped one
				fprintf(stderr, "No suitable image file found, making a diagonally striped one.\n");
				for (unsigned h = 0; h < input_height; h++) {
					for (unsigned w = 0; w < capture_width; w++) {
						frame_buffer[h * capture_width + w] = ((w + h + n) * 32) % 256;
					}
				}
				if (VERBOSE)
					fprintf(stderr, "%s: %s captured image %u [%ux%u].\n", sc_time_stamp().to_string().c_str(), name(),
					        n, capture_width, input_height);
			} else {
				if (VERBOSE)
					fprintf(stderr, "%s: %s captured image %u [%ux%u] (%s).\n", sc_time_stamp().to_string().c_str(),
					        name(), n, capture_width, input_height, infilename);
			}*/
			output_buffer = frame_buffer;
			plic->gateway_trigger_interrupt(irq_number);
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
				// Do nothing for 0 degrees
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
				    output_buffer[y * output_width + x];
			}
		}
		memcpy(output_buffer, temp, OUTPUT_BUFFER_SIZE);
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
		uint32_t output_width = input_width * scale_factor;
		uint32_t output_height = input_height * scale_factor;
		unsigned char *temp = new unsigned char[OUTPUT_BUFFER_SIZE];
		
		double angle = deg * M_PI / 180.0;
		double cos_angle = cos(angle);
		double sin_angle = sin(angle);

		for (uint32_t y = 0; y < output_height; y++) {
			for (uint32_t x = 0; x < output_width; x++) {
				int new_x = static_cast<int>(cos_angle * (x - output_width / 2) - sin_angle * (y - output_height / 2) + output_width / 2);
				int new_y = static_cast<int>(sin_angle * (x - output_width / 2) + cos_angle * (y - output_height / 2) + output_height / 2);

				if (new_x >= 0 && new_x < output_width && new_y >= 0 && new_y < output_height) {
					temp[new_y * output_width + new_x] = output_buffer[y * output_width + x];
				}
			}
		}
		memcpy(output_buffer, temp, OUTPUT_BUFFER_SIZE);
		delete[] temp;
	} 

	/******************************************************************************
	 * Function: read_pgm_image
	 * Purpose: This function reads in an image in PGM format. The image can be
	 * read in from either a file or from standard input. The image is only read
	 * from standard input when infilename = NULL. Because the PGM format includes
	 * the number of columns and the number of rows in the image, these are read
	 * from the file. Memory to store the image is allocated OUTSIDE this function.
	 * The found image size is checked against the expected rows and cols.
	 * All comments in the header are discarded in the process of reading the
	 * image. Upon failure, this function returns 0, upon sucess it returns 1.
	 ******************************************************************************/
	int read_pgm_image(const char *infilename, unsigned char *image, int rows, int cols) {
		FILE *fp;
		char buf[71];
		int r, c;

		/***************************************************************************
		 * Open the input image file for reading if a filename was given. If no
		 * filename was provided, set fp to read from standard input.
		 ***************************************************************************/
		if (infilename == NULL)
			fp = stdin;
		else {
			if ((fp = fopen(infilename, "r")) == NULL) {
				//       fprintf(stderr, "Error reading the file %s in read_pgm_image().\n",
				//          infilename);
				return (0);
			}
		}

		/***************************************************************************
		 * Verify that the image is in PGM format, read in the number of columns
		 * and rows in the image and scan past all of the header information.
		 ***************************************************************************/
		fgets(buf, 70, fp);
		if (strncmp(buf, "P5", 2) != 0) {
			//    fprintf(stderr, "The file %s is not in PGM format in ", infilename);
			//    fprintf(stderr, "read_pgm_image().\n");
			if (fp != stdin)
				fclose(fp);
			return (0);
		}
		do {
			fgets(buf, 70, fp);
		} while (buf[0] == '#'); /* skip all comment lines */
		sscanf(buf, "%d %d", &c, &r);
		if (c != cols || r != rows) {
			//    fprintf(stderr, "The file %s is not a %d by %d image in ", infilename,
			//            cols, rows);
			//    fprintf(stderr, "read_pgm_image().\n");
			if (fp != stdin)
				fclose(fp);
			return (0);
		}
		do {
			fgets(buf, 70, fp);
		} while (buf[0] == '#'); /* skip all comment lines */

		/***************************************************************************
		 * Read the image from the file.
		 ***************************************************************************/
		if ((unsigned)rows != fread(image, cols, rows, fp)) {
			//    fprintf(stderr, "Error reading the image data in read_pgm_image().\n");
			if (fp != stdin)
				fclose(fp);
			return (0);
		}

		if (fp != stdin)
			fclose(fp);
		return (1);
	}
};
#endif  // RISCV_ISA_IPU_H
