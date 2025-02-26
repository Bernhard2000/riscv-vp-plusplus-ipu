#ifndef IPU_H_
#define IPU_H_

/* Image formats */
#define FORMAT_GRAYSCALE 0
#define FORMAT_RGB       1
#define FORMAT_YUV       2

/* Operation types */
#define OP_ROTATE     0
#define OP_SCALE      1
#define OP_RGB_TO_YUV 2
#define OP_YUV_TO_RGB 3

#define COLS 2048
#define ROWS 1365
#define SIZE COLS*ROWS
#define IMG_IN    "video/jku%03u.pgm"
#define IMG_OUT   "jku%03u_%s.pgm"
/*#define IMG_IN    "video/img%03u.pgm"
#define IMG_OUT   "img%03u_%s.pgm"*/
#define IMG_NUM   3

/* Function declarations */
void rotate_image(unsigned char *input, unsigned char *output, 
                 int rows, int cols, float angle);
void scale_image(unsigned char *input, unsigned char *output, 
                int rows, int cols, float scale_factor);
void rgb_to_yuv(unsigned char *rgb, unsigned char *yuv, 
                int rows, int cols);
void yuv_to_rgb(unsigned char *yuv, unsigned char *rgb, 
                int rows, int cols);

/* Image I/O function declarations */
int read_pgm_image(const char *infilename, unsigned char *image, int rows, int cols);
int write_pgm_image(const char *outfilename, unsigned char *image, int rows,
                   int cols, const char *comment, int maxval);


#endif /* IPU_H_ */