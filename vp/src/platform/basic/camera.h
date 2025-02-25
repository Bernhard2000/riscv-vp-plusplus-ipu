// src/platform/basic/camera.h: hack for EECS 226 (06/07/21, RD)
// -> new "camera" device derived from "sensor" and connected in place of "sensor2"

#ifndef RISCV_ISA_CAMERA_H
#define RISCV_ISA_CAMERA_H

#include <cstdlib>
#include <cstring>

#include <systemc>
#include <tlm_utils/simple_target_socket.h>

#include "core/common/irq_if.h"

#define CAM_MAX_WIDTH		1920
#define CAM_MAX_HEIGHT		1080
#define CAM_FRAME_BUFFER_SIZE	(CAM_MAX_WIDTH*CAM_MAX_HEIGHT)

#define VIDEONAME "jku"
#define IMG_IN    "video/" VIDEONAME "%ux%u_%03u.pgm"
#define AVAIL_IMG 3 /* number of different image frames (1 or more) */
#define VERBOSE   true

#define ENABLE_HARDWARE_BLUR_BUFFER	// new feature for final exam

#ifdef ENABLE_HARDWARE_BLUR_BUFFER
#define CAM_BLUR_BUFFER_ADDR	0x400000
#define CAM_BLUR_BUFFER_SIZE	(2*CAM_MAX_WIDTH*CAM_MAX_HEIGHT)
#endif

struct CannyCamera : public sc_core::sc_module {
  tlm_utils::simple_target_socket<CannyCamera> tsock;

  interrupt_gateway *plic = 0;
  uint32_t irq_number = 0;
  sc_core::sc_event capture_event;

  // memory mapped frame buffer at offset 0x0 (i.e. 0x51000000)
  unsigned char *frame_buffer = 0;

#ifdef ENABLE_HARDWARE_BLUR_BUFFER
  short int *blur_buffer = 0;
#endif

  // memory mapped configuration registers
  uint32_t capture_interval = 0; // 0 = off, 33333us = 30 FPS, 1min max
  uint32_t capture_width  = 640; // 640 default, CAM_MAX_WIDTH max
  uint32_t capture_height = 360; // 360 default, CAM_MAX_HEIGHT max
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
  uint32_t blur_sigma100    = 0; // 0 = off, range 10-400 (sigma=[0.1-4.0], 0.6 typ.)
  uint32_t blur_boostfactor = 1; // 1 default, range 0-1000 (90 typical)
#endif
  std::unordered_map<uint64_t, uint32_t *> addr_to_reg;

  enum { CAPTURE_INTERVAL_REG_ADDR = 0xff0000,
	 CAPTURE_WIDTH_ADDR        = 0xff0004,
	 CAPTURE_HEIGHT_ADDR       = 0xff0008,
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
	 HW_BLUR_SIGMA100_ADDR     = 0xff000c,
	 HW_BLUR_BOOSTFACTOR_ADDR  = 0xff0010,
#endif
  };

  SC_HAS_PROCESS(CannyCamera);

  CannyCamera(sc_core::sc_module_name, uint32_t irq_number)
		: irq_number(irq_number) {
    tsock.register_b_transport(this, &CannyCamera::transport);

    assert(CAM_FRAME_BUFFER_SIZE < CAPTURE_INTERVAL_REG_ADDR);
    frame_buffer = new unsigned char[CAM_FRAME_BUFFER_SIZE];
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
    assert(CAM_FRAME_BUFFER_SIZE <= CAM_BLUR_BUFFER_ADDR);
    assert(CAM_BLUR_BUFFER_ADDR+CAM_BLUR_BUFFER_SIZE <= CAPTURE_INTERVAL_REG_ADDR);
    blur_buffer = new short int[CAM_FRAME_BUFFER_SIZE];
#endif
    addr_to_reg = {
        {CAPTURE_INTERVAL_REG_ADDR, &capture_interval},
        {CAPTURE_WIDTH_ADDR,  &capture_width},
        {CAPTURE_HEIGHT_ADDR, &capture_height},
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
        {HW_BLUR_SIGMA100_ADDR, &blur_sigma100},
        {HW_BLUR_BOOSTFACTOR_ADDR, &blur_boostfactor},
#endif
    };
    SC_THREAD(run);
  }

  void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
    auto addr = trans.get_address();
    auto cmd = trans.get_command();
    auto len = trans.get_data_length();
    auto ptr = trans.get_data_ptr();

    if (addr < CAM_FRAME_BUFFER_SIZE) {
      // access frame buffer
      assert(cmd == tlm::TLM_READ_COMMAND);
      assert((addr + len) <= CAM_FRAME_BUFFER_SIZE);

      // return pixel data at requested address
      memcpy(ptr, &frame_buffer[addr], len);
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
    } else if (  (addr >= CAM_BLUR_BUFFER_ADDR)
               &&(addr < CAM_BLUR_BUFFER_ADDR+CAM_BLUR_BUFFER_SIZE)) {
      // access blur buffer
      assert(cmd == tlm::TLM_READ_COMMAND);
      assert((addr + len) <= CAM_BLUR_BUFFER_ADDR+CAM_BLUR_BUFFER_SIZE);

      // return blurred pixel data at requested address
      memcpy(ptr, &blur_buffer[(addr-CAM_BLUR_BUFFER_ADDR)/sizeof(short int)], len);
#endif
    } else {
      assert(len == 4);  // NOTE: only allow to read/write whole register

      auto it = addr_to_reg.find(addr);
      assert(it != addr_to_reg.end());  // access to non-mapped address

      // trigger pre read/write actions
      if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == CAPTURE_INTERVAL_REG_ADDR)) {
        uint32_t value = *((uint32_t *)ptr);
        if (value > 60*1000000) // greater than 1 minute?
          return;  // ignore invalid values
      }
      if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == CAPTURE_WIDTH_ADDR)) {
        uint32_t value = *((uint32_t *)ptr);
        if (value > CAM_MAX_WIDTH) // greater than max?
          return;  // ignore invalid values
      }
      if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == CAPTURE_HEIGHT_ADDR)) {
        uint32_t value = *((uint32_t *)ptr);
        if (value > CAM_MAX_HEIGHT) // greater than max?
          return;  // ignore invalid values
      }
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
      if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == HW_BLUR_SIGMA100_ADDR)) {
        uint32_t value = *((uint32_t *)ptr);
        if (value > 400) // greater than max?
          return;  // ignore invalid values
      }
      if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == HW_BLUR_BOOSTFACTOR_ADDR)) {
        uint32_t value = *((uint32_t *)ptr);
        if (value > 1000) // greater than max?
          return;  // ignore invalid values
      }
#endif

      // actual read/write
      if (cmd == tlm::TLM_READ_COMMAND) {
        *((uint32_t *)ptr) = *it->second; // read the register
      } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        *it->second = *((uint32_t *)ptr); // write the register
      } else {
        assert(false && "unsupported tlm command for camera access");
      }

      // trigger post read/write actions
      if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == CAPTURE_INTERVAL_REG_ADDR)) {
        capture_event.cancel();
        if (capture_interval>0) {
	  capture_event.notify(sc_core::sc_time(capture_interval, sc_core::SC_US));
	}
      }
    }
  }

  void run() {
    unsigned int n = 0;
    char infilename[70];
    while (true) {
      if (capture_interval>0) {
        capture_event.notify(sc_core::sc_time(capture_interval, sc_core::SC_US));
      }
      sc_core::wait(capture_event);

      // capture an image into the frame buffer
      sprintf(infilename, IMG_IN, capture_width, capture_height, (n%AVAIL_IMG)+1);
      if (read_pgm_image(infilename, frame_buffer, capture_height, capture_width) == 0){
//	// no suitable image file found, make a monotone one
//      memset(frame_buffer, (n*32)%256, capture_height*capture_width);
	// no suitable image file found, make a diagonally striped one
	for(unsigned h=0; h<capture_height; h++) {
	    for(unsigned w=0; w<capture_width; w++) {
		frame_buffer[h*capture_width+w] = ((w+h+n)*32)%256;
	    }
	}
        if (VERBOSE) fprintf(stderr, "%s: %s captured image %u [%ux%u].\n",
			sc_time_stamp().to_string().c_str(), name(), n,
			capture_width, capture_height);
      } else {
        if (VERBOSE) fprintf(stderr, "%s: %s captured image %u [%ux%u] (%s).\n",
			sc_time_stamp().to_string().c_str(), name(), n,
			capture_width, capture_height, infilename);
      }
#ifdef ENABLE_HARDWARE_BLUR_BUFFER
      if (blur_sigma100 > 0) {
	gaussian_smooth(frame_buffer, capture_height, capture_width,
		(float)blur_sigma100/(float)100.0,	// sigma
		(float)blur_boostfactor,		// boostblurfactor
		blur_buffer);
      }
#endif

      plic->gateway_trigger_interrupt(irq_number);
      n++;
    }
  }

// inserted from Canny sources by Mike Heath (05/17/21, RD)

/* source: http://marathon.csee.usf.edu/edge/edge_detection.html */
/* URL: ftp://figment.csee.usf.edu/pub/Edge_Comparison/source_code/canny.src */

/*******************************************************************************
* --------------------------------------------
*(c) 2001 University of South Florida, Tampa
* Use, or copying without permission prohibited.
* PERMISSION TO USE
* In transmitting this software, permission to use for research and
* educational purposes is hereby granted.  This software may be copied for
* archival and backup purposes only.  This software may not be transmitted
* to a third party without prior permission of the copyright holder. This
* permission may be granted only by Mike Heath or Prof. Sudeep Sarkar of
* University of South Florida (sarkar@csee.usf.edu). Acknowledgment as
* appropriate is respectfully requested.
* 
*  Heath, M., Sarkar, S., Sanocki, T., and Bowyer, K. Comparison of edge
*    detectors: a methodology and initial study, Computer Vision and Image
*    Understanding 69 (1), 38-54, January 1998.
*  Heath, M., Sarkar, S., Sanocki, T. and Bowyer, K.W. A Robust Visual
*    Method for Assessing the Relative Performance of Edge Detection
*    Algorithms, IEEE Transactions on Pattern Analysis and Machine
*    Intelligence 19 (12),  1338-1359, December 1997.
*  ------------------------------------------------------
* [...]
* NAME: Mike Heath
*       Computer Vision Laboratory
*       University of South Floeida
*       heath@csee.usf.edu
*
* DATE: 2/15/96
* [...]
*******************************************************************************/

#ifdef ENABLE_HARDWARE_BLUR_BUFFER

#define WINSIZE 21 /* assume SIGMA < 4.0 */

/*******************************************************************************
* PROCEDURE: gaussian_smooth
* PURPOSE: Blur an image with a gaussian filter.
* NAME: Mike Heath
* DATE: 2/15/96
*******************************************************************************/
void gaussian_smooth(unsigned char *image, int rows, int cols, float sigma,
        float boostblurfactor, short int *smoothedim)
{
   int r, c, rr, cc,     /* Counter variables. */
      windowsize,        /* Dimension of the gaussian kernel. */
      center;            /* Half of the windowsize. */
   float *tempim,        /* Buffer for separable filter gaussian smoothing. */
         *kernel,        /* A one dimensional gaussian kernel. */
         dot,            /* Dot product summing variable. */
         sum;            /* Sum of the kernel weights variable. */
   static float TempImage[CAM_FRAME_BUFFER_SIZE];
   float Gaussian_kernel[WINSIZE];
   tempim = TempImage;
   kernel = Gaussian_kernel;

   /****************************************************************************
   * Create a 1-dimensional gaussian smoothing kernel.
   ****************************************************************************/
// if(VERBOSE) printf("   Computing the gaussian smoothing kernel.\n");
   make_gaussian_kernel(sigma, &kernel, &windowsize);
   center = windowsize / 2;

   /****************************************************************************
   * Allocate a temporary buffer image and the smoothed image.
   ****************************************************************************/
// if((tempim = (float *) calloc(rows*cols, sizeof(float))) == NULL){
//    fprintf(stderr, "Error allocating the buffer image.\n");
//    exit(1);
// }
// if(((*smoothedim) = (short int *) calloc(rows*cols,
//       sizeof(short int))) == NULL){
//    fprintf(stderr, "Error allocating the smoothed image.\n");
//    exit(1);
// }

   /****************************************************************************
   * Blur in the x - direction.
   ****************************************************************************/
// if(VERBOSE) printf("   Bluring the image in the X-direction.\n");
   for(r=0;r<rows;r++){
      for(c=0;c<cols;c++){
         dot = 0.0;
         sum = 0.0;
         for(cc=(-center);cc<=center;cc++){
            if(((c+cc) >= 0) && ((c+cc) < cols)){
               dot += (float)image[r*cols+(c+cc)] * kernel[center+cc];
               sum += kernel[center+cc];
            }
         }
         tempim[r*cols+c] = dot/sum;
      }
   }

   /****************************************************************************
   * Blur in the y - direction.
   ****************************************************************************/
// if(VERBOSE) printf("   Bluring the image in the Y-direction.\n");
   for(c=0;c<cols;c++){
      for(r=0;r<rows;r++){
         sum = 0.0;
         dot = 0.0;
         for(rr=(-center);rr<=center;rr++){
            if(((r+rr) >= 0) && ((r+rr) < rows)){
               dot += tempim[(r+rr)*cols+c] * kernel[center+rr];
               sum += kernel[center+rr];
            }
         }
         (smoothedim)[r*cols+c] = (short int)(dot*boostblurfactor/sum + 0.5);
      }
   }

// free(tempim);
// free(kernel);
}

/*******************************************************************************
* PROCEDURE: make_gaussian_kernel
* PURPOSE: Create a one dimensional gaussian kernel.
* NAME: Mike Heath
* DATE: 2/15/96
*******************************************************************************/
void make_gaussian_kernel(float sigma, float **kernel, int *windowsize)
{
   int i, center;
   float x, fx, sum=0.0;

   *windowsize = 1 + 2 * ceil(2.5 * sigma);
   center = (*windowsize) / 2;

// if(VERBOSE) printf("      The kernel has %d elements.\n", *windowsize);
// if((*kernel = (float *) calloc((*windowsize), sizeof(float))) == NULL){
//    fprintf(stderr, "Error callocing the gaussian kernel array.\n");
//    exit(1);
// }

   for(i=0;i<(*windowsize);i++){
      x = (float)(i - center);
      fx = pow(2.71828, -0.5*x*x/(sigma*sigma)) / (sigma * sqrt(6.2831853));
      (*kernel)[i] = fx;
      sum += fx;
   }

   for(i=0;i<(*windowsize);i++) (*kernel)[i] /= sum;

// if(VERBOSE){
//    printf("The filter coefficients are:\n");
//    for(i=0;i<(*windowsize);i++)
//       printf("kernel[%d] = %f\n", i, (*kernel)[i]);
// }
}

#endif // ENABLE_HARDWARE_BLUR_BUFFER

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
int read_pgm_image(const char *infilename, unsigned char *image, int rows,
    int cols)
{
   FILE *fp;
   char buf[71];
   int r, c;

   /***************************************************************************
    * Open the input image file for reading if a filename was given. If no
    * filename was provided, set fp to read from standard input.
    ***************************************************************************/
   if(infilename == NULL) fp = stdin;
   else{
      if((fp = fopen(infilename, "r")) == NULL){
//       fprintf(stderr, "Error reading the file %s in read_pgm_image().\n",
//          infilename);
         return(0);
      }
   }

   /***************************************************************************
    * Verify that the image is in PGM format, read in the number of columns
    * and rows in the image and scan past all of the header information.
    ***************************************************************************/
   fgets(buf, 70, fp);
   if(strncmp(buf,"P5",2) != 0){
//    fprintf(stderr, "The file %s is not in PGM format in ", infilename);
//    fprintf(stderr, "read_pgm_image().\n");
      if(fp != stdin) fclose(fp);
      return(0);
   }
   do{ fgets(buf, 70, fp); }while(buf[0] == '#');  /* skip all comment lines */
   sscanf(buf, "%d %d", &c, &r);
   if(c != cols || r != rows){
//    fprintf(stderr, "The file %s is not a %d by %d image in ", infilename,
//            cols, rows);
//    fprintf(stderr, "read_pgm_image().\n");
      if(fp != stdin) fclose(fp);
      return(0);
   }
   do{ fgets(buf, 70, fp); }while(buf[0] == '#');  /* skip all comment lines */

   /***************************************************************************
    * Read the image from the file.
    ***************************************************************************/
   if((unsigned)rows != fread(image, cols, rows, fp)){
//    fprintf(stderr, "Error reading the image data in read_pgm_image().\n");
      if(fp != stdin) fclose(fp);
      return(0);
   }

   if(fp != stdin) fclose(fp);
   return(1);
}

};

#endif  // RISCV_ISA_CAMERA_H
