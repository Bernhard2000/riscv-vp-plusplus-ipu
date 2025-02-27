#include "ipu.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Function prototypes */
void rotate_image(unsigned char *input, unsigned char *output, int rows, int cols, float angle_degrees);
void scale_image(unsigned char *input, unsigned char *output, int rows, int cols, float scale_factor);
void rgb_to_yuv(unsigned char *rgb, unsigned char *yuv, int rows, int cols);
void yuv_to_rgb(unsigned char *yuv, unsigned char *rgb, int rows, int cols);
int read_pgm_image(const char *infilename, unsigned char *image, int rows, int cols);
int write_pgm_image(const char *outfilename, unsigned char *image, int rows,
                   int cols, const char *comment, int maxval);

void rotate_image(input, output, rows, cols, angle_degrees)
   unsigned char *input;
   unsigned char *output;
   int rows;
   int cols;
   float angle_degrees;
{
   float angle_rad;
   float cos_angle;
   float sin_angle;
   float center_x;
   float center_y;
   int x, y;
   float dx, dy;
   int src_x, src_y;

   angle_rad = angle_degrees * M_PI / 180.0f;
   cos_angle = cos(angle_rad);
   sin_angle = sin(angle_rad);
   center_x = cols / 2.0f;
   center_y = rows / 2.0f;

   /* Clear output buffer */
   memset(output, 0, rows * cols);

   for(y = 0; y < rows; y++) {
      for(x = 0; x < cols; x++) {
         dx = x - center_x;
         dy = y - center_y;
         src_x = round(dx * cos_angle - dy * sin_angle + center_x);
         src_y = round(dx * sin_angle + dy * cos_angle + center_y);
         
         if(src_x >= 0 && src_x < cols && src_y >= 0 && src_y < rows) {
            output[y * cols + x] = input[src_y * cols + src_x];
         }
      }
   }
}

void scale_image(input, output, rows, cols, scale_factor)
   unsigned char *input;
   unsigned char *output;
   int rows;
   int cols;
   float scale_factor;
{
   int new_rows, new_cols;
   int x, y;
   int src_x, src_y;

   new_rows = rows * scale_factor;
   new_cols = cols * scale_factor;

   for(y = 0; y < new_rows; y++) {
      for(x = 0; x < new_cols; x++) {
         src_x = x / scale_factor;
         src_y = y / scale_factor;
         
         if(src_x < cols && src_y < rows) {
            output[y * new_cols + x] = input[src_y * cols + src_x];
         }
      }
   }
}

void rgb_to_yuv(rgb, yuv, rows, cols)
   unsigned char *rgb;
   unsigned char *yuv;
   int rows;
   int cols;
{
   int i, r, g, b;

   for(i = 0; i < rows * cols; i++) {
      r = rgb[i * 3];
      g = rgb[i * 3 + 1];
      b = rgb[i * 3 + 2];

      yuv[i * 3] = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
      yuv[i * 3 + 1] = (unsigned char)(-0.169f * r - 0.331f * g + 0.5f * b + 128);
      yuv[i * 3 + 2] = (unsigned char)(0.5f * r - 0.419f * g - 0.081f * b + 128);
   }
}

void yuv_to_rgb(yuv, rgb, rows, cols)
   unsigned char *yuv;
   unsigned char *rgb;
   int rows;
   int cols;
{
   int i, r, g, b;
   float y, u, v;

   for(i = 0; i < rows * cols; i++) {
      y = yuv[i * 3];
      u = yuv[i * 3 + 1] - 128;
      v = yuv[i * 3 + 2] - 128;

      r = y + 1.4075f * v;
      g = y - 0.3455f * u - 0.7169f * v;
      b = y + 1.7790f * u;

      rgb[i * 3] = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
      rgb[i * 3 + 1] = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
      rgb[i * 3 + 2] = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
   }
}

int main()
{
   unsigned char input[SIZE];
   unsigned char output[SIZE];
   char infilename[70];
   char outfilename[70];
   int i;
   
   for(i = 0; i < 1; i++) {
      sprintf(infilename, IMG_IN, i+1);
      
      if(read_pgm_image(infilename, input, ROWS, COLS) == 0) {
         fprintf(stderr, "Error reading input image\n");
         return 1;
      }

      for(int angle = 0; angle < 360; angle++) {
        rotate_image(input, output, ROWS, COLS, (float)angle);
        sprintf(outfilename, IMG_OUT, i+1, "rotate");

        if(write_pgm_image(outfilename, output, ROWS, COLS, "Processed Image", 255) == 0) {
          fprintf(stderr, "Error writing output image\n");
          return 1;
        }
      }
      scale_image(input, output, ROWS, COLS, 0.5f);
      sprintf(outfilename, IMG_OUT, i+1, "scale");
      if(write_pgm_image(outfilename, output, ROWS*0.5f, COLS*0.5f, "Processed Image", 255) == 0) {
         fprintf(stderr, "Error writing output image\n");
         return 1;
      }
      /*rgb_to_yuv(input, output, ROWS, COLS);
      sprintf(outfilename, IMG_OUT, i+1, "rgb2yuv");
      if(write_pgm_image(outfilename, output, ROWS, COLS, "Processed Image", 255) == 0) {
         fprintf(stderr, "Error writing output image\n");
         return 1;
      }
      yuv_to_rgb(input, output, ROWS, COLS);
      sprintf(outfilename, IMG_OUT, i+1, "yuv2rgb");
      if(write_pgm_image(outfilename, output, ROWS, COLS, "Processed Image", 255) == 0) {
         fprintf(stderr, "Error writing output image\n");
         return 1;
      }*/
   }
   return 0;
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
        fprintf(stderr, "Error reading the file %s in read_pgm_image().\n",
           infilename);
        return(0);
     }
  }

  /***************************************************************************
  * Verify that the image is in PGM format, read in the number of columns
  * and rows in the image and scan past all of the header information.
  ***************************************************************************/
  fgets(buf, 70, fp);
  if(strncmp(buf,"P5",2) != 0){
     fprintf(stderr, "The file %s is not in PGM format in ", infilename);
     fprintf(stderr, "read_pgm_image().\n");
     if(fp != stdin) fclose(fp);
     return(0);
  }
  do{ fgets(buf, 70, fp); }while(buf[0] == '#');  /* skip all comment lines */
  sscanf(buf, "%d %d", &c, &r);
  if(c != cols || r != rows){
     fprintf(stderr, "The file %s is not a %d by %d image in ", infilename,
             cols, rows);
     fprintf(stderr, "read_pgm_image().\n");
     if(fp != stdin) fclose(fp);
     return(0);
  }
  do{ fgets(buf, 70, fp); }while(buf[0] == '#');  /* skip all comment lines */

  /***************************************************************************
  * Read the image from the file.
  ***************************************************************************/
  if((unsigned)rows != fread(image, cols, rows, fp)){
     fprintf(stderr, "Error reading the image data in read_pgm_image().\n");
     if(fp != stdin) fclose(fp);
     return(0);
  }

  if(fp != stdin) fclose(fp);
  return(1);
}

/******************************************************************************
* Function: write_pgm_image
* Purpose: This function writes an image in PGM format. The file is either
* written to the file specified by outfilename or to standard output if
* outfilename = NULL. A comment can be written to the header if coment != NULL.
******************************************************************************/
int write_pgm_image(const char *outfilename, unsigned char *image, int rows,
   int cols, const char *comment, int maxval)
{
  FILE *fp;
 
  /***************************************************************************
  * Open the output image file for writing if a filename was given. If no
  * filename was provided, set fp to write to standard output.
  ***************************************************************************/
  if(outfilename == NULL) fp = stdout;
  else{
     if((fp = fopen(outfilename, "w")) == NULL){
        fprintf(stderr, "Error writing the file %s in write_pgm_image().\n",
           outfilename);
        return(0);
     }
  }
  printf(outfilename);

  /***************************************************************************
  * Write the header information to the PGM file.
  ***************************************************************************/
  fprintf(fp, "P5\n%d %d\n", cols, rows);
  if(comment != NULL)
     if(strlen(comment) <= 70) fprintf(fp, "# %s\n", comment);
  fprintf(fp, "%d\n", maxval);

  /***************************************************************************
  * Write the image data to the file.
  ***************************************************************************/
  if((unsigned)rows != fwrite(image, cols, rows, fp)){
     fprintf(stderr, "Error writing the image data in write_pgm_image().\n");
     if(fp != stdout) fclose(fp);
     return(0);
  }

  if(fp != stdout) fclose(fp);
  return(1);
}

void copy_image(void *dst, const void *src, size_t pixel_size)
{
  memcpy(dst, src, SIZE*pixel_size);
}
