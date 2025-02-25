#ifndef _filter_h
#define _filter_h

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define VERBOSE 0
#define NOEDGE 255
#define POSSIBLE_EDGE 128
#define EDGE 0
#define BOOSTBLURFACTOR 90.0
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define WINSIZE 21 /* assume SIGMA < 4.0 */
#define SIGMA 0.6
#define TLOW  0.3
#define THIGH 0.8
#define COLS 100
#define ROWS 70
#define SIZE COLS*ROWS
#define VIDEONAME "jku100x70_"
#define IMG_IN    "" VIDEONAME "%03u.pgm"
#define IMG_OUT   VIDEONAME "%03u_edges.pgm"
#define IMG_NUM   3 /* number of images processed (1 or more) */
#define AVAIL_IMG 3 /* number of different image frames (1 or more) */

void canny(unsigned char *image, int rows, int cols, float sigma,
    float tlow, float thigh, unsigned char *edge);
/* void gaussian_smooth(unsigned char *image, int rows, int cols, float sigma,
    short int *smoothedim); */
void blurX(unsigned char *image, int rows, int cols, float sigma,
    float *tempim);
void blurY(float *tempim, int rows, int cols, float sigma,
    short int *smoothedim);
void make_gaussian_kernel(float sigma, float *kernel, int *windowsize);
void derivative_x_y(short int *smoothedim, int rows, int cols,
    short int *delta_x, short int *delta_y);
void magnitude_x_y(short int *delta_x, short int *delta_y, int rows, int cols,
    short int *magnitude, short int *gradx, short int *grady);
void apply_hysteresis(short int *mag, unsigned char *nms, int rows, int cols,
    float tlow, float thigh, unsigned char *edge);
void non_max_supp(short *mag, short *gradx, short *grady, int nrows, int ncols,
    short *mag_copy, unsigned char *result);
int write_pgm_image(const char *outfilename, unsigned char *image, int rows,
    int cols, const char *comment, int maxval);
void copy_image(void *dst, const void *src, size_t pixel_size);

#endif