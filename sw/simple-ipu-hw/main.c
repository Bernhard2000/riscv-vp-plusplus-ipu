#include "irq.h"
#include "filter.h"

void camera_irq_handler(void);
void ipu_irq_handler(void);

void StartCamera(void);
void StopCamera(void);

void StartIPU(void);

void data_in(unsigned char *image, unsigned int i);
void data_out(unsigned char *edge, unsigned int i);

void ipu_data_in(unsigned char *image, unsigned int i);


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
const uint32_t IPU_IRQ_NUMBER = 8;

volatile unsigned int frames_captured = 0;
volatile unsigned int ipu_frames_processed = 0;

unsigned int height = ROWS;
unsigned int width = COLS;


static volatile char *const IPU_START_ADDR = (char *const)0x80000000;
static volatile char *const IPU_END_ADDR   = (char *const)0x80ffffff; 
static volatile unsigned char *const IPU_INPUT_BUFFER_ADDR =
         (unsigned char *const)(IPU_START_ADDR + 0x000000);
static volatile uint32_t *const IPU_WIDTH_REG_ADDR  =
         (uint32_t *const)(IPU_START_ADDR + 0xff0000);
static volatile uint32_t *const IPU_HEIGHT_REG_ADDR =
         (uint32_t *const)(IPU_START_ADDR + 0xff0004);
static volatile uint32_t *const IPU_SCALE_FACTOR_REG_ADDR =
         (uint32_t *const)(IPU_START_ADDR + 0xff0008);
static volatile uint32_t *const IPU_ROTATION_ANGLE_REG_ADDR =
         (uint32_t *const)(IPU_START_ADDR + 0xff000c);
static volatile uint32_t *const IPU_ENABLE_REG_ADDR =
			(uint32_t *const)(IPU_START_ADDR + 0xff0010);
static volatile uint32_t *const IPU_OUTPUT_WIDTH_REG_ADDR =
         (uint32_t *const)(IPU_START_ADDR + 0xff0014);
static volatile uint32_t *const IPU_OUTPUT_HEIGHT_REG_ADDR =
         (uint32_t *const)(IPU_START_ADDR + 0xff0018);

/* Gaussian kernel (computed at beginning, then constant) */
float Gaussian_kernel[WINSIZE] = {0.0};
int Gaussian_kernel_center = 0;

int main(void)
{
   unsigned char image[1920*1080]; //TODO make whatever max size is
   unsigned char edge[1920*1080];
   unsigned int i;
   int windowsize; /* Dimension of the gaussian kernel. */

   printf("Start camera");
   //StartCamera(); /* turn on the camera device */



   for(i=0; i<1; i++)
   {
      StartCamera(); /* turn on the camera device */
      /*************************************************************************
      * Input a frame.
      *************************************************************************/
      printf("Input image frame %u.\n", i+1);
      data_in(image, i);
      //ipu_data_in(image, i);
      //data_out(image, i);
      StopCamera(); /* turn on the camera device */
      StartIPU(); /* turn on the IPU device */
      *IPU_ROTATION_ANGLE_REG_ADDR = 180;
      memcpy(edge, image, 1920*1080);
      ipu_data_in(edge, i);
          data_out(edge, i); // Use angle in filename to save each rotation
      for (int angle = 0; angle < 360; angle++) {
         memcpy(edge, image, 1920*1080);
          *IPU_ROTATION_ANGLE_REG_ADDR = angle;
          ipu_data_in(edge, i);
          data_out(edge, i + angle); // Use angle in filename to save each rotation
      }

      /*************************************************************************
      * Output a frame.
      *************************************************************************/
   }
   //StartIPU(); /* turn on the IPU device */
   //ipu_data_in(image, 0);
   //data_out(image, 0);
   return(0); /* exit cleanly */
}

void camera_irq_handler(void)
{
    frames_captured++;
    printf("Capture frame %u.\n", frames_captured);
}

void ipu_irq_handler(void)
{
    ipu_frames_processed++;
    printf("IPU Capture frame %u.\n", frames_captured);
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

void StartIPU(void) 
{
      register_interrupt_handler(IPU_IRQ_NUMBER, ipu_irq_handler);
      *IPU_WIDTH_REG_ADDR = COLS;
      *IPU_HEIGHT_REG_ADDR = ROWS;
      *IPU_SCALE_FACTOR_REG_ADDR  = 1;   /* set requested frame dimensions */
      *IPU_ENABLE_REG_ADDR = 1;
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

void ipu_data_in(unsigned char *image, unsigned int i)
{
   printf("Read IPU data");

   /*************************************************************************
   * Grab an image from the camera
   *************************************************************************/
  copy_image(IPU_INPUT_BUFFER_ADDR, image, sizeof(unsigned char));
  *IPU_ENABLE_REG_ADDR = 1;
  while (!ipu_frames_processed) {
      asm volatile ("wfi");
   }
   printf("IPU data");
   if (ipu_frames_processed>1) {
      printf("Warning: %u frames skipped! Processing too slow.\n", ipu_frames_processed);
   }
   ipu_frames_processed = 0;

   height = *IPU_OUTPUT_HEIGHT_REG_ADDR;
   width = *IPU_OUTPUT_WIDTH_REG_ADDR;
   printf("Height: %u, Width: %u\n", height, width);
   copy_image_size(image, (const void*)IPU_INPUT_BUFFER_ADDR, sizeof(unsigned char), height*width);
}

void data_out(unsigned char *edge, unsigned int i)
{
   char outfilename[128]; /* Name of the output "edge" image */
   unsigned int n;


   /*************************************************************************
   * Write out an edge image to a file.
   *************************************************************************/
   n = 0;//i % AVAIL_IMG;
   sprintf(outfilename, IMG_OUT, n+1);
   //copy_image(edge, (void*)CAMERA_FRAME_BUFFER_ADDR, sizeof(unsigned char));
   if(VERBOSE) printf("Writing the edge image %s.\n", outfilename);
   if(write_pgm_image(outfilename, edge, height, width, "", 255) == 0){
      fprintf(stderr, "Error writing the edge image, %s.\n", outfilename);
      exit(1);
   }
}
