/* Date Last Modified: Feb. 20, 2020
 * 
 * Brian Savage and Eric Duanmu
 * bsavag01         eduanm01
 * 
 * HW3 - Locality
 * uarray2b.c
 * Function: UArray2 implementation that uses blocking to store the contents
 *           of the array, where each blocks elements exhibit spacial locality
 */


#include <stdlib.h>                                       
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "uarray.h"
#include "uarray2b.h"
#include "uarray2.h"                   
#include "assert.h"

#define T UArray2b_T

/* UArray2b_T (uses the defined macro T)
 * Purpose: An array that uses blocking for spacial locality 
 *          and block major accessing 
 * Data Structure: the two dimensional blocked array will contain 
 *                 data members to store the dimensions of the array,
 *                 the array itself will have a block as each element,
 *                 each block will be a UArray_T type which is a one
 *                 dimensional array
 * Math for Indexing: to get an element at col and row of the UArray2b_T
 *                    first, locate block with col and row
 *                    second, compute [blocksize * (row % blocksize) + 
 *                    (col % blocksize)] to get the 1D index using row and col
 */
struct T {
        int       height; /* number of elements in column of array */
        int        width; /* number of elements in row of array */
        int         size; /* size of a single element of array */
        int    blocksize; /* height and width of single block 
                           * (sqrt of the number of elements in a block) */
        UArray2_T  array; /* a UArray2_T of blocks, each block is a UArray_T 
                           * of elements */

};

static void apply_on_block(T array2b, 
                           int block_col, int block_row, 
                           int end_small_col, int end_small_row, 
                           void apply(int col, int row, 
                                      T array2b, void *elem, void *cl), 
                           void *cl);


/* T UArray2b_new(int width, int height, int size, int blocksize)
 * Parameters: int width - desired total number of elements in row
 *             int height - desired total number of elements in column
 *             int size - the memory allocation size of a single element
 *             int blocksize - number of elements in row or column of a block
 *    Returns: UArray2b_T, with initialized data members and array of blocks
 *       Does: Initializes new UArray2b type based on the passed parameters
 *   if Error: raises assertions 
 *             if height or width is less than zero
 *             if size or blocksize is less than or equal to zero 
 *             if failed mallocing
 */
T UArray2b_new(int width, int height, int size, int blocksize)
{
       /* Validating conditions to create a new UArray2b (T) 
        * type as specified */
        assert(height >= 0);
        assert(width >= 0);
        assert(size > 0);
        assert(blocksize > 0);
        
        T blocked = NULL;
        blocked = malloc(sizeof(*blocked)); 
        assert(blocked != NULL);      
        
        /* Initialize data members of a UArray2b (T) */
        blocked->size = size; 
        blocked->blocksize = blocksize; 
        blocked->width = width; 
        blocked->height = height; 
        
        /* Calculates number of blocks needed along the width of the 
         * image and the height of the image, rounds up using ceil to 
         * account for cut blocks on the right and bottom edges of 
         * the image */
        int blocked_width = ceil((double) width / blocksize); 
        int blocked_height = ceil((double) height / blocksize);
        
        /* Initialization of UArray2b->array to a UArray2 type with each 
         * element of the array being a block with the dimensions 
         * blocksize x blocksize */
        blocked->array = UArray2_new(blocked_width,  
                                     blocked_height, 
                                     sizeof(UArray_T));

        /* Initializes each element of UArray2b->array to a UArray_T type 
         * which will store the elements of each block in the UArray_T */
        for (int block_col = 0; block_col < blocked_width; block_col++) {
                for (int block_row = 0; block_row < blocked_height; 
                     block_row++) {

                         /* Get the block at the current block indexes and 
                          * initialize it to a new UArray_T, the elements of 
                          * each block will be located next  to each other in 
                          * memory to exhibit spacial locality */
                        *(UArray_T *) UArray2_at(blocked->array, 
                                                 block_col, block_row) 
                        = UArray_new(blocksize * blocksize, size);

                }
        }
        return blocked;
}

/* T UArray2b_new_64K_block(int width, int height, int size)
 * Parameters: int width - desired total number of elements in row
 *                         int height - desired total number of elements 
 *                         in column
 *             int size - the memory allocation size of a single element
 *    Returns: UArray2b_T, with initialized data members and array of blocks
 *       Does: Initializes new UArray2b type based on the passed parameters
 *                         and uses a blocksize that occupies a max size 
 *                         of 64K per block
 *   if Error: raises assertions 
 *             if height or width is less than zero
 *             if size is less than or equal to zero 
 */
T UArray2b_new_64K_block(int width, int height, int size)
{
        /* Validating conditions to create a new UArray2b (T) type 
         * as specified */
        assert(height >= 0);
        assert(width  >= 0);
        assert(size   >  0);

        /* Computation for max blocksize */
        int blocksize = 0;
        if (size > 65536) {
                /* element size is larger than 64K so smallest 
                 * blocksize is used, (1 element per block) */
                blocksize = 1; 
        } else {
                /* blocksize is set to the maximum number of elements 
                 * in a row or column of a block that occupies 
                 * 64K of memory */
                blocksize = (int) ceil(sqrt(65536 / size));
        }

        /* Call UArrat2b_new with the computed blocksize and 
         * return result */
        return UArray2b_new(width, height, size, blocksize);
}

/* void UArray2b_free (T *array2b)
 * Parameters: T *array2b - pointer to UArray2b_T object to be freed
 *    Returns: Nothing
 *       Does: Frees all memory allocated for the passed UArray2b_T object
 *   if Error: raises assertions 
 *             if array2b is null
 *             if array2b->array is null
 */ 
void UArray2b_free (T *array2b)
{
        /* Validate that the passed array2b and the data member array 
         * are not null */
        assert(array2b != NULL);
        assert((*array2b)->array != NULL); 
        
        /* Compute the number of blocks in the 
         * width and height of the array */
        int blocked_width = ceil((double) (*array2b)->width 
                                           / (*array2b)->blocksize);
        int blocked_height = ceil((double) (*array2b)->height 
                                            / (*array2b)->blocksize);

        /* Index each block within the array using row-major indexing */
        for (int i = 0; i < blocked_height; i++) {
                for (int j = 0; j < blocked_width; j++) {

                        /* Get then free the array at each 
                         * block of the UArray2b_T */
                        UArray_T* temp = (UArray_T*) 
                                          UArray2_at((*array2b)->array, j, i);
                        UArray_free(temp);
                }
        }

        /* Free the memory allocated for the UArray2b->array object 
         * that holds each block then free the memory allocated for 
         * the structs data members like height and width */
        UArray2_free(&((*array2b)->array));
        free(*array2b);
}

/* int UArray2b_width (T array2b)
 * Parameters: T array2b - UArray2_T object to be accessed
 *    Returns: Integer expressing the width of the array
 *       Does: Gets the number of initialized elements in row of array
 *   if Error: raises assertions 
 *             if array2b is null
 */
int UArray2b_width (T array2b)
{
        assert(array2b != NULL); 
        return array2b->width;
}

/* int UArray2b_height (T array2b)
 * Parameters: T array2b - UArray2_T object to be accessed
 *    Returns: Integer expressing the height of the array
 *       Does: Gets the number of initialized elements in a column of array
 *   if Error: raises assertions 
 *             if array2b is null
 */
int UArray2b_height (T array2b)
{
        assert(array2b != NULL);
        return array2b->height;
} 

/* int UArray2b_size (T array2b)
 * Parameters: T array2b - UArray2_T object to be accessed
 *    Returns: Integer expressing the size of a singular element of the array 
 *       Does: Gets the number of bytes occupied by one element of the array
 *   if Error: raises assertions 
 *             if array2b is null
 */
int UArray2b_size (T array2b)
{
        assert(array2b != NULL);
        return array2b->size;
}

/* int UArray2b_blocksize (T array2b)
 * Parameters: T array2b - UArray2_T object to be accessed
 *    Returns: Integer expressing height and width of a block
 *       Does: Gets the number of elements in a row or column 
 *             of a single block
 *   if Error: raises assertions 
 *             if array2b is null
 */
int UArray2b_blocksize(T array2b)
{
        assert(array2b != NULL);
        return array2b->blocksize;
}

/* void *UArray2b_at(T array2b, int col, int row)
 * Parameters: T array2b - UArray2_T object to be accessed
 *                         int col - the column index for the desired element
 *                         int row - the row index for the desired element
 *    Returns: Void * to the element at the passed indexes
 *       Does: Gets the element at the desired column and row of the array
 *   if Error: raises assertions 
 *             if array2b is null
 *             if array2b->array is null
 *             if col or row index is out of bounds of array2b
 */
void *UArray2b_at(T array2b, int col, int row)
{
        /* Validate that the array2b and array2b->array are both not null */
        assert(array2b != NULL);
        assert(array2b->array != NULL);

        /* Validates indexes are within bounds */
        assert(col < array2b->width && col >= 0);
        assert(row < array2b->height && row >= 0);
        
        /* Get the block that contained the desired element */
        int blocksize = array2b->blocksize;
        UArray_T *block = UArray2_at(array2b->array, 
                                     col / blocksize, row / blocksize);

        /* Since each block is implemented as a 1 dimensional array, 
         * the calculation below finds the corresponding 1D index from 
         * the passed row and col */                                        
        int index_within_block = blocksize * (row % blocksize) 
                                 + (col % blocksize);

        /* Return the element at the 1D index of the block containing 
         * row and col */
        return UArray_at(*block, index_within_block);
}

/* void UArray2b_map(T array2b, 
 *                   void apply(int col, int row, T array2b, void *elem, 
 *                              void *cl),
 *                   void *cl)
 * Parameters: T array2b - UArray2_T object to be accessed
 *          void apply() - pointer to the function that should 
 *                         be applied to each element during the block 
 *                         major mapping
 *              void *cl - void pointer to closure function or data point
 *                         hat should be incremented
 * Apply Function Parameters:
 *               int col - column index for the desired element
 *               int row - row index for the desired element
 *             T array2b - UArray2_T object to be accessed
 *            void *elem - void pointer to element at col and row indexes
 *              void *cl - void pointer to closure function or data point
 *                        that should be incremented
 *    Returns: Nothing
 *       Does: Uses block-major accessing to map onto the array and call the 
 *             apply function on each element of the passed UArray2b_T object
 *   if Error: raises assertions 
 *             if array2b is null
 *             if array2b->array is null
 */
void UArray2b_map(T array2b, 
                  void apply(int col, int row, T array2b, void *elem, 
                             void *cl), 
                  void *cl)
{
        /* Validate that the array2b and array2b->array are both not null */
        assert(array2b != NULL);
        assert(apply != NULL);
        
        /* Create local variables to be used multiple times */
        int blocksize = array2b->blocksize;
        int width = array2b->width;
        int height = array2b->height;
        /* Number of blocks in a row and number of blocks in a column */
        int blocked_width = ceil((double) width / blocksize);
        int blocked_height = ceil((double) height / blocksize);
                
        /* Use row major accessing to index the blocked array */
        for (int block_row = 0; block_row < blocked_height; block_row++) {
                for (int block_col = 0; block_col < blocked_width; 
                     block_col++) {

                        /* Default end small column and row indices for if 
                         * there is no wasted memory in any block */
                        int end_small_col = blocksize; 
                        int end_small_row = blocksize; 
                        
                        /* Updates end column index if the current block 
                         * has wasted memory beyond the width */
                        if ((block_col + 1) * blocksize > width) {
                                end_small_col = (width % blocksize); 
                        } 
                    
                        /* Updates end row index if the current block has 
                         * wasted memory beyond the height */
                        if ((block_row + 1) * blocksize > height) {
                                end_small_row = (height % blocksize);
                        }

                        /* Helper func that applies the apply function to 
                         * every element of the current block */
                        apply_on_block(array2b, 
                                       block_col, block_row, 
                                       end_small_col, end_small_row, 
                                       apply, cl);
                }
        }
}                  

/* static void apply_on_block(T array2b, UArray_T block, 
 *                            int block_col, int block_row, 
 *                            int end_small_col, int end_small_row,
 *                            void apply(int col, int row, T array2b, 
 *                                       void *elem, void *cl), 
 *                            void *cl)
 * Parameters: T array2b - UArray2_T object to be accessed
 *         int block_col - column index of the current block being indexed
 *         int block_row - row index of the current block being indexed
 *     int end_small_col - column index for the end of the 
 *                         initialized elements
 *     int end_small_row - row index for the end of the initialized 
 *                         elements
 *          void apply() - pointer to the function that should be applied to
 *                         each element during the block major mapping
 *              void *cl - void pointer to closure function or data point
 *                         that should be incremented
 * Apply Function Parameters:
 *               int col - column index for the desired element
 *               int row - row index for the desired element
 *             T array2b - UArray2_T object to be accessed
 *            void *elem - void pointer to element at col and row indexes
 *              void *cl - void pointer to closure function or data point
 *                        that should be incremented
 *  Returns: Nothing
 *     Does: Helper function to prevent out of bounds indexing during mapping
 * if Error: raises assertion 
 *           if array2b is null
 */
static void apply_on_block(T array2b, 
                           int block_col, int block_row, 
                           int end_small_col, int end_small_row, 
                           void apply(int col, int row, 
                                       T array2b, void *elem, void *cl), 
                           void *cl)
{       
        assert(array2b != NULL);
        int blocksize = array2b->blocksize;

        /* Use row major accessing to index the current block */
        for (int small_row = 0; small_row < end_small_row; small_row++) {
                for (int small_col = 0; small_col < end_small_col; 
                     small_col++) {

                        /* Computes the element's macro indexes within the 
                         * entire array then uses the UArray2b_at function 
                         * to get the element at those indexes */
                        int large_col = block_col * blocksize + small_col;
                        int large_row = block_row * blocksize + small_row;
                        void *elem = UArray2b_at(array2b, large_col, 
                                                 large_row);
                        apply(large_col, large_row, array2b, elem, cl);
                }
        }
}
