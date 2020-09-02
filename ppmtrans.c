/* Date Last Modified: Feb. 20, 2020
 * 
 * Brian Savage and Eric Duanmu
 * bsavag01         eduanm01
 * 
 * HW3 - Locality
 * ppmtrans.c
 * Function: Ppmtrans takes a ppm file as a parameter and applies a rotation
 *           of 0, 90, 180, or 270 degree rotations using a specifed mapping
 *           method of row-major, col-major, or block major mapping. Ppmtrans
 *           also supports timing these rotations using the -time command
 *           line argument and stores this timing data in a specified file
 *           which will be the next argument of the command line after -time.        
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "assert.h"
#include "a2methods.h"
#include "a2plain.h"
#include "a2blocked.h"
#include "pnm.h"
#include "cputiming.h"

#define A2 A2Methods_UArray2

/* Prints to stderr the correct formatting of ppmtrans' command line 
 * arguments if user formatting is invalid
 */ 
static void
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-rotate <angle>] "
                    "[-{row,col,block}-major] [filename]\n",
                    progname);
    exit(1);
}



FILE *create_file(int i, int argc, char *argv[]);

Pnm_ppm rotate_img(int rotation, Pnm_ppm input_img, A2Methods_mapfun *map, 
                   A2Methods_T methods, char *time_file_name);
      
/* apply functions to be mapped */
void rotate_0(int input_col, int input_row, A2 input_img, void *elem, 
               void *new_img);
void rotate_90(int input_col, int input_row, A2 input_img, void *elem, 
               void *new_img);
void rotate_180(int input_col, int input_row, A2 input_img, void *elem, 
                void *new_img);

void print_time(char *time_file_name, double time_elapsed, int num_pixels,
                int rotation);

int main(int argc, char *argv[]) 
{
    char *time_file_name = NULL;
    int   rotation       = 0;
    int   i;

    /* default to UArray2 methods */
    A2Methods_T methods = uarray2_methods_plain; 
    assert(methods);

    /* default to best map */
    A2Methods_mapfun *map = methods->map_default; 
    assert(map);

    #define SET_METHODS(METHODS, MAP, WHAT) do {      \
        methods = (METHODS);                          \
        assert(methods != NULL);                      \
        map = methods->MAP;                           \
        if (map == NULL) {                            \
            fprintf(stderr, "%s does not support "    \
            WHAT "mapping\n",                         \
            argv[0]);                                 \
            exit(1);                                  \
        }                                             \
    } while (0)

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-row-major") == 0) {
            SET_METHODS(uarray2_methods_plain, map_row_major, 
                        "row-major");
        } else if (strcmp(argv[i], "-col-major") == 0) {
            SET_METHODS(uarray2_methods_plain, map_col_major, 
                        "column-major");
        } else if (strcmp(argv[i], "-block-major") == 0) {
            SET_METHODS(uarray2_methods_blocked, map_block_major,
                        "block-major");
        } else if (strcmp(argv[i], "-rotate") == 0) {
            if (!(i + 1 < argc)) {      /* no rotate value */
                usage(argv[0]);
            }
            char *endptr;
            rotation = strtol(argv[++i], &endptr, 10);
            if (!(rotation == 0 || rotation == 90 ||
                rotation == 180)) {
                    fprintf(stderr, "Rotation must be 0, 90, or 180\n");
                    usage(argv[0]);
            }
            if (!(*endptr == '\0')) {    /* Not a number */
                    usage(argv[0]);
            }
        } else if (strcmp(argv[i], "-time") == 0) {
            time_file_name = argv[++i];             /* TIME FILE */
        } else if (*argv[i] == '-') {
            fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[i]);
        } else if (argc - i > 1) {
            fprintf(stderr, "Too many arguments\n");
            usage(argv[0]);
        } else {
            break;
        }
    }

    /* initializes file pointer to the ppm file passed as a command line arg */
    FILE *file = create_file(i, argc, argv);

    /* initializes input_img from the file contents of the file pointer */
    Pnm_ppm input_img = Pnm_ppmread(file, methods);
    /* applies rotation on input_img and initializes new rotated version */
    Pnm_ppm rotated_img = rotate_img(rotation, input_img, map, methods, 
                                     time_file_name);

    /* Writes to terminal by default, but supports piping to file */
    Pnm_ppmwrite(stdout, rotated_img);

    /* Freeing allocated memory of input_img and rotated_img and closes file */
    Pnm_ppmfree(&input_img);
    Pnm_ppmfree(&rotated_img);
    fclose(file);
    
    /* Exits with EXIT_SUCCESS after successful completion  */
    exit(EXIT_SUCCESS);
}

/* FILE *create_file(int i, int argc, char *argv[])
 * Parameters: int i - integer expressing the current command line argument
 *                     being collected, this determines if input is collected
 *                     from stdin or from argv
 *             int argc - integer of command line arguments
 *             char *argv[] - list of command line arguments
 *    Returns: a pointer to the opened file
 *       Does: initializes file pointer with command line argument if passed,
 *             if no file argument is passed, then the file to be opened will
 *             be collected from the users command line
 */
FILE *create_file(int i, int argc, char *argv[])
{
    FILE *fp = NULL;
    /* initialization of the file pointer from argv or stdin */
    if (i < argc) {
        fp = fopen(argv[i], "rb");
    } else {
        fp = stdin;
    }
    /* Raised if the file pointer was not initialized from argv or stdin */
    assert(fp != NULL);
    return fp;
}

Pnm_ppm rotate_img(int rotation, Pnm_ppm input_img, A2Methods_mapfun *map, 
                   A2Methods_T methods, char *time_file_name)
{
    int num_pixels = input_img->width * input_img->height;
    int rgb_pixel_size = sizeof(struct Pnm_rgb);
   
    Pnm_ppm rotated_img = malloc(sizeof(*input_img));
    assert(rotated_img != NULL);
    
    rotated_img->denominator = input_img->denominator;
    rotated_img->methods = methods; 
    
    CPUTime_T time = CPUTime_New();
    double time_elapsed = 0;
    if (rotation == 0) {
        rotated_img->width = input_img->width;
        rotated_img->height = input_img->height;
        rotated_img->pixels = methods->new(rotated_img->width, 
                                           rotated_img->height, 
                                           rgb_pixel_size);

        CPUTime_Start(time);
        map(input_img->pixels, rotate_0, rotated_img);
        time_elapsed = CPUTime_Stop(time);
    }
    if (rotation == 90) {
        rotated_img->width = input_img->height;
        rotated_img->height = input_img->width;
        rotated_img->pixels = methods->new(rotated_img->width, 
                                           rotated_img->height, 
                                           rgb_pixel_size);
                                           
        CPUTime_Start(time);
        map(input_img->pixels, rotate_90, rotated_img);
        time_elapsed = CPUTime_Stop(time);
    }
    if (rotation == 180) {
        rotated_img->width = input_img->width;
        rotated_img->height = input_img->height;
        rotated_img->pixels = methods->new(rotated_img->width, 
                                           rotated_img->height, 
                                           rgb_pixel_size);

        CPUTime_Start(time);
        map(input_img->pixels, rotate_180, rotated_img);
        time_elapsed = CPUTime_Stop(time);
    }
    
    print_time(time_file_name, time_elapsed, num_pixels, rotation);
    
    CPUTime_Free(&time);
    
    return rotated_img;
}

void rotate_0(int input_col, int input_row, A2 input_img, void *elem, 
               void *new_img)
{
    (void) input_img;
    Pnm_ppm rotated_img = new_img;
    
    int rotated_col = input_col;
    int rotated_row = input_row;
  
    Pnm_rgb input_pixel = (Pnm_rgb) elem;

    Pnm_rgb rotated_pixel = rotated_img->methods->at(rotated_img->pixels, 
                                                     rotated_col,
                                                     rotated_row);
    *rotated_pixel = *input_pixel;
}

void rotate_90(int input_col, int input_row, A2 input_img, void *elem, 
               void *new_img)
{
    (void) input_img;
    Pnm_ppm rotated_img = (Pnm_ppm) new_img;

    int input_height = rotated_img->width;
    
    int rotated_col = input_height - input_row - 1;
    int rotated_row = input_col;
    
    Pnm_rgb input_pixel = (Pnm_rgb) elem;

    Pnm_rgb rotated_pixel = rotated_img->methods->at(rotated_img->pixels, 
                                                     rotated_col,
                                                     rotated_row);
    *rotated_pixel = *input_pixel;
}

void rotate_180(int input_col, int input_row, A2 input_img, void *elem, 
                void *new_img)
{
    (void) input_img;
    
    Pnm_ppm rotated_img = new_img;
    
    int input_width = rotated_img->width;
    int input_height = rotated_img->height;
    
    int rotated_col = input_width - input_col - 1;
    int rotated_row = input_height - input_row - 1;
    
    Pnm_rgb input_pixel = (Pnm_rgb) elem;

    Pnm_rgb rotated_pixel = rotated_img->methods->at(rotated_img->pixels, 
                                                     rotated_col,
                                                     rotated_row);
    *rotated_pixel = *input_pixel;
}

void print_time(char *time_file_name, double time_elapsed, int num_pixels, 
                int rotation)
{
    if (time_file_name != NULL) {
        FILE *time_file = fopen(time_file_name, "w+");
        assert(time_file != NULL);
        
        double time_per_pixel = time_elapsed / num_pixels;
        
        fprintf(time_file, "Number of pixels: %u\n", num_pixels);
        fprintf(time_file, 
                "Rotation of %u degrees was computed in %.0f nanoseconds\n", 
                rotation, time_elapsed);
        fprintf(time_file, "Time per pixel: %.0f nanoseconds\n", 
                time_per_pixel);
                
        fclose(time_file);   
    }
}
