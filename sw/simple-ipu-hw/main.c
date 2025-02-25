#include "irq.h"
#include "filter.h"

void camera_irq_handler(void);
void StartCamera(void);
void StopCamera(void);
void data_in(unsigned char *image, unsigned int i);
void data_out(unsigned char *edge, unsigned int i);

static volatile char *const CAMERA_START_ADDR = (char *const)0x51000000;
static volatile char *const CAMERA_END_ADDR   = (char *const)0x51ffffff;
static volatile unsigned char *const CAMERA_FRAME_BUFFER_ADDR =
			(unsigned char *const)(CAMERA_START_ADDR + 0x000000);
static volatile uint32_t *const CAMERA_CAPTURE_INTERVAL_REG_ADDR =
			(uint32_t *const)(CAMERA_START_ADDR + 0xff0000);
static volatile uint32_t *const CAMERA_WIDTH_REG_ADDR  =
			(uint32_t *const)(CAMERA_START_ADDR + 0xff0004);
static volatile uint32_t *const CAMERA_HEIGHT_REG_ADDR =
			(uint32_t *const)(CAMERA_START_ADDR + 0xff0008);

const uint32_t CAMERA_IRQ_NUMBER = 5;
volatile unsigned int frames_captured = 0;

static volatile char *const IPU_START_ADDR = (char *const)0x73000000;
static volatile char *const IPU_END_ADDR   = (char *const)0x73ffffff; 
static volatile unsigned char *const IPU_INPUT_BUFFER_ADDR =
         (unsigned char *const)(IPU_START_ADDR + 0x000000);
static volatile unsigned char *const IPU_OUTPUT_BUFFER_ADDR =
         (unsigned char *const)(IPU_START_ADDR + 0x100000);
static volatile uint32_t *const IPU_CONTROL_REG_ADDR =
         (uint32_t *const)(IPU_START_ADDR + 0xff0000);
   

/* Gaussian kernel (computed at beginning, then constant) */
float Gaussian_kernel[WINSIZE] = {0.0};
int Gaussian_kernel_center = 0;

int main(void)
{
   unsigned char image[SIZE];
   unsigned char edge[SIZE];
   unsigned int i;
   int windowsize; /* Dimension of the gaussian kernel. */

   printf("Start camera");
   StartCamera(); /* turn on the camera device */



   for(i=0; i<IMG_NUM; i++)
   {
      /*************************************************************************
      * Input a frame.
      *************************************************************************/
      printf("Input image frame %u.\n", i+1);
      data_in(image, i);

     
      /*************************************************************************
      * Output a frame.
      *************************************************************************/
     data_out(image, i);

   }

   StopCamera(); /* turn on the camera device */

   return(0); /* exit cleanly */
}

void camera_irq_handler(void)
{
    frames_captured++;
    printf("Capture frame %u.\n", frames_captured);
}

void StartCamera(void) /* configure and turn on the camera */
{
    register_interrupt_handler(CAMERA_IRQ_NUMBER, camera_irq_handler);
    *CAMERA_WIDTH_REG_ADDR  = COLS;   /* set requested frame dimensions */
    *CAMERA_HEIGHT_REG_ADDR = ROWS;
//  *CAMERA_CAPTURE_INTERVAL_REG_ADDR = 33333; /* capture every 33333us (30 FPS) */
//  *CAMERA_CAPTURE_INTERVAL_REG_ADDR = 1e6;   /* capture every second  ( 1 FPS) */
    *CAMERA_CAPTURE_INTERVAL_REG_ADDR = 1e7;   /* capture every 10 secs (.1 FPS) */
}

void StopCamera(void)
{
    *CAMERA_CAPTURE_INTERVAL_REG_ADDR = 0;
}


void data_in(unsigned char *image, unsigned int i)
{
   /*************************************************************************
   * Grab an image from the camera
   *************************************************************************/
   while (!frames_captured) {
      asm volatile ("wfi");
   }
   if (frames_captured>1) {
      printf("Warning: %u frames skipped! Processing too slow.\n", frames_captured);
   }
   frames_captured = 0;

   copy_image(image, (const void*)CAMERA_FRAME_BUFFER_ADDR, sizeof(unsigned char));
}

void data_out(unsigned char *edge, unsigned int i)
{
   char outfilename[128]; /* Name of the output "edge" image */
   unsigned int n;


   /*************************************************************************
   * Write out an edge image to a file.
   *************************************************************************/
   n = i % AVAIL_IMG;
   sprintf(outfilename, IMG_OUT, n+1);
   //copy_image(edge, (void*)CAMERA_FRAME_BUFFER_ADDR, sizeof(unsigned char));
   if(VERBOSE) printf("Writing the edge image %s.\n", outfilename);
   if(write_pgm_image(outfilename, edge, ROWS, COLS, "", 255) == 0){
      fprintf(stderr, "Error writing the edge image, %s.\n", outfilename);
      exit(1);
   }
}
