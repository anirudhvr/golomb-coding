/*
 * Golomb encoding and decoding. The implementation is based on
 * pseudocode taken from 'Compression and Coding Algorithms' by Alistair
 * Moffat & Andrew Turping, Kluwer Academic Publishers, 2002.
 * Copyright Anirudh Ramachandran <anirudhvr@gmail.com>, 2008
 * Released under GPLv2
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <math.h>

#include "encode.h"

#define CHUNK 8192
#define CHUNK_2 16384
#define BUF_REALLOC_PENALTY 3
#define MAX_INPUT_SIZE 131072 /* 128 KB */


/*
 * XXX
 * these two are static buffers used to write encoded output, both
 * during run-length encoding/decoding and golomb encoding/decoding.
 * We need them to be greater than the max. input size we expect as some
 * run-length encodings will actually be longer than the input. 10 times
 * the max.input length seems reasonable (about 4 MB each, i think..)
 *
 */
unsigned int rle_buf[10*MAX_INPUT_SIZE];
unsigned int golomb_buf[10*MAX_INPUT_SIZE];

/*
 *
 * Encoding functions: run-length encoding. Run-length encoding returns
 * the number of trials required before a 1 is found. For example, the
 * RLE according to the following impl. of this sequence of bits:
 *
 * 001 1 00001 1 001 0
 * is 
 * 3 1 5 1 3 1 0 <--- the last '0' is used as a marker indicating that a
 * string of 1 '0' terminated the input as opposed to a single '1'.
 *
 * 
 */



/*
 * The main RLE function. Takes as input an unsigned char buffer, and
 * outputs the RLE as an unsigned int buf. This function automatically
 * adds a last unsigned char of all 1s (ie., value 255) so that we don't
 * come to the problem of having a hanging ends-in-zero marker, if the
 * last char of the input ends in a 0. This char is automatically
 * removed after you do run-length decode
 */

int
get_run_length_encoding (unsigned char *in, 
    unsigned long size,
    unsigned int **out,
    unsigned long *outsize)
{
  unsigned int *rle;
  int i, j;
  int rle_index = 0;
  int need_to_splice = 0;
  unsigned char allones = 255;

  rle = rle_buf;

  for (i = 0; i < size; ++i) {
    //printf ("got char %d\n", in[i]);
    for (j = 0; j < rle_lookup_sizes[in[i]]; ++j) {
      if (!j && need_to_splice) {
        //printf ("splicing..");
        if (rle_lookup[in[i]][0] > 1) {
          /* this means that the first bit is a run of one or more
           * zeros.  so we can successfully splice with the previous set
           * */
          //assert (rle_index > 7);
          //printf ("increased %d with %d\n", 
              //rle[rle_index - 2], rle_lookup[in[i]][0]);
          rle[rle_index - 2] += rle_lookup[in[i]][0];
          rle_index--;
        } else { 
          /* darn, the first entry is a 1 (ie, the first bit is a 1), so
           * we cant splice, so move increase the value of the last
           * non-zero int in the rle, and move index back one so as to
           * overwrite the last 0 */
          rle[rle_index - 2]++;
          --rle_index; 
        }
        need_to_splice = 0;
      } else {
        rle[rle_index++] = rle_lookup[in[i]][j];
        //printf ("writing %d\n", rle_lookup[in[i]][j]);
      }
    }

    if (rle[rle_index - 1] == 0)
      need_to_splice = 1;
  }

  //printf ("got char %d\n", allones);
  for (j = 0; j < rle_lookup_sizes[allones]; ++j) {
    if (!j && need_to_splice) {
      //printf ("splicing..");
      if (rle_lookup[allones][0] > 1) {
        /* this means that the first bit is a run of one or more
         * zeros.  so we can successfully splice with the previous set
         * */
        //assert (rle_index > 7);
        //printf ("increased %d with %d\n", 
            //rle[rle_index - 2], rle_lookup[allones][0]);
        rle[rle_index - 2] += rle_lookup[allones][0];
        rle_index--;
      } else { 
        /* darn, the first entry is a 1 (ie, the first bit is a 1), so
         * we cant splice, so move index back one so as to overwrite
         * the last 0 */
        rle[rle_index - 2]++;
        --rle_index; 
      }
      need_to_splice = 0;
    } else {
      rle[rle_index++] = rle_lookup[allones][j];
      //printf ("writing %d\n", rle_lookup[allones][j]);
    }
  }

  if ( !(*out = malloc (sizeof (unsigned int) * (rle_index))) ) {
    perror ("cant malloc: ");
    return 1;
  }

  if ( !(memcpy (*out, rle, sizeof (unsigned int) * (rle_index))) ) {
    perror ("cant memcpy: ");
    free (*out);
    return 1;
  }

  *outsize = rle_index;
  return 0;
}


/*
 * Decode the unsigned integer buffer obtained above. Output is returned
 * as an unsigned char buffer
 */
 
int 
get_run_length_decoding (unsigned int *in,
    unsigned long size,
    unsigned char **out,
    unsigned long *outsize)
{
  int i;
  int currindex, bytecounter;
  unsigned char *currbyte;

  currindex = 0;
  bytecounter = 1;

  memset (rle_buf, 0, sizeof(unsigned int) * MAX_INPUT_SIZE);
  currbyte = (unsigned char*) rle_buf;

  //printf ("last at %d: %d\n", size, in[size-1]);

  for (i = 0; i < size; ++i) {
    currindex += in[i];
    while (currindex > 8) {
      currbyte++;
      currindex -= 8;
      bytecounter++;
    }
    *currbyte |= 1 << (8 - currindex);
    //printf ("setting bit %d of byte %d\n", 
        //currindex, bytecounter);
  }

  *outsize = bytecounter - 1;
  if ( !(*out= (unsigned char *) malloc (*outsize)) ) {
    perror ("RLD: cannot malloc: ");
    return 1;
  }

  if ( !(memcpy (*out, rle_buf, *outsize)) ) {
    perror ("RLD: cannot memcpy: ");
    free (*out);
    return 1;
  }

  return 0;
}


/*
 * Golomb encoding and decoding. The implementation is based on
 * pseudocode taken from 'Compression and Coding Algorithms' by Alistair
 * Moffat & Andrew Turping, Kluwer Academic Publishers, 2002.
 */


/*
 * Macros for certain functions used in the referecne above
 */

#define PUT_ONE_INTEGER(x, nbits) do { \
  int i; \
   /* printf ("x, nbits: %d %d\n", (x), (nbits)); */ \
  for (i = (nbits) - 1; i > -1; --i) { \
    /* printf ("byte %d, bit %d, putting ", (bytecounter), (currindex)); */ \
    if ( ((x) >> i) & 1 ) { \
      *(currbyte) |= 1 << (7 - currindex); \
    /*  printf ("..1"); */ \
    } else { \
      /* printf ("..0"); */ \
    } \
    /* printf ("\n"); */ \
    currindex++; \
    if (currindex >= 8) { \
      (currbyte)++; \
      bytecounter++; \
      currindex -= 8; \
    } \
  } \
} while (0);


#define GET_ONE_INTEGER(nbits) do { \
  int i; \
  x = 0; \
  /* printf ("nbits: %d\n", (nbits)); */ \
  for (i = (nbits) - 1; i > -1; --i) { \
    x = (x << 1) | \
    ((*(currbyte) & (1 << (7 - currindex))) > 0); \
    /* printf ("i: %d, currbyte: %d, currindex: %d, x: %d\n", \
        i, bytecounter, currindex, x);*/ \
    currindex++; \
    if (currindex >= 8) { \
      (currbyte)++; \
      bytecounter++; \
      currindex -= 8; \
    } \
  } \
} while (0);


/*
 * To make printing a number in binary easier 
 */

char *h2b[] = {
  "0000", "0001", "0010", "0011",
  "0100", "0101", "0110", "0111",
  "1000", "1001", "1010", "1011",
  "1100", "1101", "1110", "1111"
};

/*
 * This function finds ceil (log (base 2, n))
 */
inline int
ceil_log2 (int b) {
  int last = -1, secondlast = -1, count = 0;
  int log2_b, tmp = b;

  while (tmp) {
    if (tmp & 1) {
      secondlast = last;
      last = count;
    }
    tmp >>= 1;
    ++count;
  }
  if (secondlast == -1)
    log2_b = last;
  else
    log2_b = last+1;

  return log2_b;
}

/* an array to help with the num_set_bits function that follows.
 * Records the number of set bits for each value of an unsigned char */
const unsigned char set_bits_lookup_table[256] = 
{
  0, 1, 1, 2, 1, 2, 2, 3, /* 0 - 7 */
  1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, /* 120 - 127 */
  4, 5, 5, 6, 5, 6, 6, 7, 
  1, 2, 2, 3, 2, 3, 3, 4, 
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7,
  5, 6, 6, 7, 6, 7, 7, 8  /* 248 - 255 */
};

/* 
   This function uses a lookup table to quickly find the number of bits
   set in a char array of size 'size'
*/
static unsigned long 
num_set_bits (unsigned char *input, unsigned long size)
{
  unsigned long count = 0, i = 0;
  while (i < size)
    count += set_bits_lookup_table[input[i++]];
  return count;
}


/*
 *
 * Encoding function: golomb encoding
 *
 * Golomb encoding is done on a sequence of positive integers.
 * Typically, if you want to encode an arbitrary char buffer, you should
 * first run-length encode it (which will give you an
 * all-positive-integer buffer, which you can provide to golomb
 * encoding. 
 *
 * The input is the positive integer buffer, and the output is the
 * encoded buffer (say a char *), and a parameter for the encoding (see
 * the reference above for details), which is typically calculated as a
 * function of the number of set bits in the (original) input. The more
 * sparse the input, the better the compression (duh!).
 */

int
golomb_encode (void *input,
    unsigned long input_len,
    void **out,
    unsigned long *outsize,
    unsigned int *golomb_param)
{
  unsigned long setbits, currindex, size, rle_size;
  int b, i;
  int q, r;
  int d, bytecounter;
  unsigned int *rle;
  unsigned int log2_b;
  unsigned char *currbyte, *in;
  float prob;

  in = (unsigned char*) input;
  size = input_len;

  if (!in) return -1;

  setbits = size * 8 - num_set_bits (in, size); /* from bf.h */
  prob = (float)setbits / (float)(size * 8);

  //printf ("set bits in input: %lu\n", setbits);
  //b = (int) ceilf (LN2 * (float) (size * 8.0) / (float) setbits);
  b = (int) ceilf (-(LN2 / (float) (logf (prob))));
  //b = 3;

  /* b+=2; */

  //printf ("p = %f, b = %d\n",(float) setbits / (float) (size * 8.0), b);

  if (get_run_length_encoding (in, size, &rle, &rle_size) ) {
    fprintf (stderr, "golomb encode: error with RLE\n");
    return 1;
  }

  memset (golomb_buf, 0, sizeof(unsigned int) * MAX_INPUT_SIZE);
  currbyte = (unsigned char*) golomb_buf;
  currindex = 0; bytecounter = 0;

  /* this is for the ceil (log_{2} (b) ) 
   * needed in minimal binary decode
   * need to compute only once 
   */
  log2_b = ceil_log2 (b);

  //printf ("log2_b = %d\n", log2_b);

  /* main loop reading RLE input */
  for (i = 0; i < rle_size; ++i) {
    q = (rle[i] - 1) / b;
    r = rle[i] - q * b;

    //printf ("q: %d, r: %d\n", q, r);

    /* unary encode (q + 1) */
    ++q;
    while (--q) {
      *currbyte |= 1 << (7 - currindex);
      ++currindex;
      if (currindex >= 8) {
        currbyte++;
        bytecounter++;
        currindex -= 8;
      }
    }

    //printf ("after encoding quotient byte %d = %s %s\n",
        //bytecounter, h2b[*currbyte >> 4], h2b[*currbyte & 0x0f]);
    
    /* equivalent to putting the last 0 */
    currindex++;

    if (currindex >= 8) {
      currbyte++;
      bytecounter++;
      currindex -= 8;
    }

    /* minimal binary encode (r, b) */
    d = (1 << log2_b) - b;
    //printf ("d = %d\n", d);

    if (r > d) {
      /* put_one_integer (r - 1 + d, log2_b) */
      PUT_ONE_INTEGER (r - 1 + d, log2_b);
    } else {
      PUT_ONE_INTEGER (r - 1, log2_b - 1);
    }

    //printf ("after encoding remainder byte %d (%d)= %s %s\n",
        //bytecounter, currindex, h2b[*currbyte >> 4], h2b[*currbyte & 0x0f]);

  }

  *outsize = bytecounter;
  if ( !(*out = malloc (*outsize) ) ) {
    perror ("golombencode: cannot malloc output buf: ");
    return 1;
  }

  if ( !(memcpy (*out, golomb_buf, *outsize) ) ) {
    perror ("golombencode: cannot memcpy to output: ");
    free (*out);
    return 1;
  }

  //*golomb_param = log2_b;
  *golomb_param = b;

  return 0;
}

/*
 * Decode a golomb-encoded input. This function also requires the
 * parameter returned by the golomb encode funciton above
 */

int
golomb_decode (void *input, unsigned long input_len, 
    unsigned int golomb_param, void **out, 
    unsigned long *outsize) 
{
  unsigned long setbits, currindex, size, rle_size;
  int b, i;
  int x, q, r;
  int d, bytecounter;
  unsigned int *gd;
  unsigned int log2_b;
  unsigned char *currbyte, *in;
  unsigned long decoded_rle_size;

  in = (unsigned char*) input;
  size = input_len;
  b = golomb_param;

  if (!in) return -1;

  memset (golomb_buf, 0, sizeof (unsigned int) * MAX_INPUT_SIZE);

  currbyte = (unsigned char*) in;
  gd = (unsigned int*) golomb_buf;

  currindex = 0; bytecounter = 0;
  q = 0;
  x = 0;

  /* log2 hack */
  log2_b = ceil_log2 (golomb_param);

  //printf ("decode: log2_b = %d\n", log2_b);

  while (bytecounter < size) {
    //printf ("byte: %d: %s %s\n", bytecounter,
        //h2b[*currbyte >> 4], h2b[*currbyte & 0x0f]);

    /* q := unary_decode() - 1 */
    q = 0;
    while (*currbyte & (1 << (7 - currindex++))) {
      ++q;
      if (currindex >= 8) {
        currbyte++;
        bytecounter++;
        currindex -= 8;
      }
    }
    //--q;


    /* sanity check */
    if (currindex >= 8) {
      currbyte++;
      bytecounter++;
      currindex -= 8;
    }

    //printf ("got q: %d, currbyte: %d, currindex: %d\n", 
        //q, bytecounter, currindex);

    /* r = minimal_binary_decode (b) */
    d = (1 << log2_b) - b;

    //printf ("d: %d, log2_b-1: %d\n", d, log2_b-1);

    GET_ONE_INTEGER (log2_b - 1);
    //printf ("got integer: %d\n", x);

    //printf ("x+1 (%d) ~ d (%d)\n", x+1, d);
    if (x + 1 > d) {
      x = (x << 1) | ((*currbyte & (1 << (7 - currindex))) > 0);
      x -= d;
      currindex++;
      if (currindex >= 8) {
        currbyte++;
        bytecounter++;
        currindex -= 8;
      }
    }

    r = x + 1;

    //printf ("got remainder: %d, currbyte: %d, currindex: %d\n", 
        //r, bytecounter, currindex);

    /* decode integer */
    *gd++ = r + q * b;
    //printf ("got answer: %d\n", r + q * b);
  }

  decoded_rle_size =  gd - golomb_buf + 1;
  
  if (get_run_length_decoding (golomb_buf, decoded_rle_size - 1, 
        (unsigned char**)out, 
        outsize) ) {
    printf ("Golomb decoding worked; RLE decoding failed\n");
    return 1;
  }


  /*
  if ( !(*out = malloc (*outsize * sizeof (unsigned int)) ) ) {
    perror ("golombdecode: cannot malloc output buf: ");
    return 1;
  }

  if ( !(memcpy (*out, golomb_buf, *outsize * sizeof (unsigned int)) ) ) {
    perror ("golombdecode: cannot memcpy to output: ");
    free (*out);
    return 1;
  }
  */

  return 0;
}



/* 
 * This is a hack for calculating run-length encoding. This lookup table
 * tells me, for each possible unsigned char, the lengths of the runs
 * (as described above). So suppose you have two chars, you can look up
 * this table, get the run lengths, and figure out how to splice them
 * (taking care of the '0' symbol at the end of the first char's RLE if
 * the second char starts with one or more '0's.
 */
 
unsigned char rle_lookup[256][9] = {
  {8,0,0,0,0,0,0,0,0},
  {8,0,0,0,0,0,0,0,0},
  {7,1,0,0,0,0,0,0,0},
  {7,1,0,0,0,0,0,0,0},
  {6,2,0,0,0,0,0,0,0},
  {6,2,0,0,0,0,0,0,0},
  {6,1,1,0,0,0,0,0,0},
  {6,1,1,0,0,0,0,0,0},
  {5,3,0,0,0,0,0,0,0},
  {5,3,0,0,0,0,0,0,0},
  {5,2,1,0,0,0,0,0,0},
  {5,2,1,0,0,0,0,0,0},
  {5,1,2,0,0,0,0,0,0},
  {5,1,2,0,0,0,0,0,0},
  {5,1,1,1,0,0,0,0,0},
  {5,1,1,1,0,0,0,0,0},
  {4,4,0,0,0,0,0,0,0},
  {4,4,0,0,0,0,0,0,0},
  {4,3,1,0,0,0,0,0,0},
  {4,3,1,0,0,0,0,0,0},
  {4,2,2,0,0,0,0,0,0},
  {4,2,2,0,0,0,0,0,0},
  {4,2,1,1,0,0,0,0,0},
  {4,2,1,1,0,0,0,0,0},
  {4,1,3,0,0,0,0,0,0},
  {4,1,3,0,0,0,0,0,0},
  {4,1,2,1,0,0,0,0,0},
  {4,1,2,1,0,0,0,0,0},
  {4,1,1,2,0,0,0,0,0},
  {4,1,1,2,0,0,0,0,0},
  {4,1,1,1,1,0,0,0,0},
  {4,1,1,1,1,0,0,0,0},
  {3,5,0,0,0,0,0,0,0},
  {3,5,0,0,0,0,0,0,0},
  {3,4,1,0,0,0,0,0,0},
  {3,4,1,0,0,0,0,0,0},
  {3,3,2,0,0,0,0,0,0},
  {3,3,2,0,0,0,0,0,0},
  {3,3,1,1,0,0,0,0,0},
  {3,3,1,1,0,0,0,0,0},
  {3,2,3,0,0,0,0,0,0},
  {3,2,3,0,0,0,0,0,0},
  {3,2,2,1,0,0,0,0,0},
  {3,2,2,1,0,0,0,0,0},
  {3,2,1,2,0,0,0,0,0},
  {3,2,1,2,0,0,0,0,0},
  {3,2,1,1,1,0,0,0,0},
  {3,2,1,1,1,0,0,0,0},
  {3,1,4,0,0,0,0,0,0},
  {3,1,4,0,0,0,0,0,0},
  {3,1,3,1,0,0,0,0,0},
  {3,1,3,1,0,0,0,0,0},
  {3,1,2,2,0,0,0,0,0},
  {3,1,2,2,0,0,0,0,0},
  {3,1,2,1,1,0,0,0,0},
  {3,1,2,1,1,0,0,0,0},
  {3,1,1,3,0,0,0,0,0},
  {3,1,1,3,0,0,0,0,0},
  {3,1,1,2,1,0,0,0,0},
  {3,1,1,2,1,0,0,0,0},
  {3,1,1,1,2,0,0,0,0},
  {3,1,1,1,2,0,0,0,0},
  {3,1,1,1,1,1,0,0,0},
  {3,1,1,1,1,1,0,0,0},
  {2,6,0,0,0,0,0,0,0},
  {2,6,0,0,0,0,0,0,0},
  {2,5,1,0,0,0,0,0,0},
  {2,5,1,0,0,0,0,0,0},
  {2,4,2,0,0,0,0,0,0},
  {2,4,2,0,0,0,0,0,0},
  {2,4,1,1,0,0,0,0,0},
  {2,4,1,1,0,0,0,0,0},
  {2,3,3,0,0,0,0,0,0},
  {2,3,3,0,0,0,0,0,0},
  {2,3,2,1,0,0,0,0,0},
  {2,3,2,1,0,0,0,0,0},
  {2,3,1,2,0,0,0,0,0},
  {2,3,1,2,0,0,0,0,0},
  {2,3,1,1,1,0,0,0,0},
  {2,3,1,1,1,0,0,0,0},
  {2,2,4,0,0,0,0,0,0},
  {2,2,4,0,0,0,0,0,0},
  {2,2,3,1,0,0,0,0,0},
  {2,2,3,1,0,0,0,0,0},
  {2,2,2,2,0,0,0,0,0},
  {2,2,2,2,0,0,0,0,0},
  {2,2,2,1,1,0,0,0,0},
  {2,2,2,1,1,0,0,0,0},
  {2,2,1,3,0,0,0,0,0},
  {2,2,1,3,0,0,0,0,0},
  {2,2,1,2,1,0,0,0,0},
  {2,2,1,2,1,0,0,0,0},
  {2,2,1,1,2,0,0,0,0},
  {2,2,1,1,2,0,0,0,0},
  {2,2,1,1,1,1,0,0,0},
  {2,2,1,1,1,1,0,0,0},
  {2,1,5,0,0,0,0,0,0},
  {2,1,5,0,0,0,0,0,0},
  {2,1,4,1,0,0,0,0,0},
  {2,1,4,1,0,0,0,0,0},
  {2,1,3,2,0,0,0,0,0},
  {2,1,3,2,0,0,0,0,0},
  {2,1,3,1,1,0,0,0,0},
  {2,1,3,1,1,0,0,0,0},
  {2,1,2,3,0,0,0,0,0},
  {2,1,2,3,0,0,0,0,0},
  {2,1,2,2,1,0,0,0,0},
  {2,1,2,2,1,0,0,0,0},
  {2,1,2,1,2,0,0,0,0},
  {2,1,2,1,2,0,0,0,0},
  {2,1,2,1,1,1,0,0,0},
  {2,1,2,1,1,1,0,0,0},
  {2,1,1,4,0,0,0,0,0},
  {2,1,1,4,0,0,0,0,0},
  {2,1,1,3,1,0,0,0,0},
  {2,1,1,3,1,0,0,0,0},
  {2,1,1,2,2,0,0,0,0},
  {2,1,1,2,2,0,0,0,0},
  {2,1,1,2,1,1,0,0,0},
  {2,1,1,2,1,1,0,0,0},
  {2,1,1,1,3,0,0,0,0},
  {2,1,1,1,3,0,0,0,0},
  {2,1,1,1,2,1,0,0,0},
  {2,1,1,1,2,1,0,0,0},
  {2,1,1,1,1,2,0,0,0},
  {2,1,1,1,1,2,0,0,0},
  {2,1,1,1,1,1,1,0,0},
  {2,1,1,1,1,1,1,0,0},
  {1,7,0,0,0,0,0,0,0},
  {1,7,0,0,0,0,0,0,0},
  {1,6,1,0,0,0,0,0,0},
  {1,6,1,0,0,0,0,0,0},
  {1,5,2,0,0,0,0,0,0},
  {1,5,2,0,0,0,0,0,0},
  {1,5,1,1,0,0,0,0,0},
  {1,5,1,1,0,0,0,0,0},
  {1,4,3,0,0,0,0,0,0},
  {1,4,3,0,0,0,0,0,0},
  {1,4,2,1,0,0,0,0,0},
  {1,4,2,1,0,0,0,0,0},
  {1,4,1,2,0,0,0,0,0},
  {1,4,1,2,0,0,0,0,0},
  {1,4,1,1,1,0,0,0,0},
  {1,4,1,1,1,0,0,0,0},
  {1,3,4,0,0,0,0,0,0},
  {1,3,4,0,0,0,0,0,0},
  {1,3,3,1,0,0,0,0,0},
  {1,3,3,1,0,0,0,0,0},
  {1,3,2,2,0,0,0,0,0},
  {1,3,2,2,0,0,0,0,0},
  {1,3,2,1,1,0,0,0,0},
  {1,3,2,1,1,0,0,0,0},
  {1,3,1,3,0,0,0,0,0},
  {1,3,1,3,0,0,0,0,0},
  {1,3,1,2,1,0,0,0,0},
  {1,3,1,2,1,0,0,0,0},
  {1,3,1,1,2,0,0,0,0},
  {1,3,1,1,2,0,0,0,0},
  {1,3,1,1,1,1,0,0,0},
  {1,3,1,1,1,1,0,0,0},
  {1,2,5,0,0,0,0,0,0},
  {1,2,5,0,0,0,0,0,0},
  {1,2,4,1,0,0,0,0,0},
  {1,2,4,1,0,0,0,0,0},
  {1,2,3,2,0,0,0,0,0},
  {1,2,3,2,0,0,0,0,0},
  {1,2,3,1,1,0,0,0,0},
  {1,2,3,1,1,0,0,0,0},
  {1,2,2,3,0,0,0,0,0},
  {1,2,2,3,0,0,0,0,0},
  {1,2,2,2,1,0,0,0,0},
  {1,2,2,2,1,0,0,0,0},
  {1,2,2,1,2,0,0,0,0},
  {1,2,2,1,2,0,0,0,0},
  {1,2,2,1,1,1,0,0,0},
  {1,2,2,1,1,1,0,0,0},
  {1,2,1,4,0,0,0,0,0},
  {1,2,1,4,0,0,0,0,0},
  {1,2,1,3,1,0,0,0,0},
  {1,2,1,3,1,0,0,0,0},
  {1,2,1,2,2,0,0,0,0},
  {1,2,1,2,2,0,0,0,0},
  {1,2,1,2,1,1,0,0,0},
  {1,2,1,2,1,1,0,0,0},
  {1,2,1,1,3,0,0,0,0},
  {1,2,1,1,3,0,0,0,0},
  {1,2,1,1,2,1,0,0,0},
  {1,2,1,1,2,1,0,0,0},
  {1,2,1,1,1,2,0,0,0},
  {1,2,1,1,1,2,0,0,0},
  {1,2,1,1,1,1,1,0,0},
  {1,2,1,1,1,1,1,0,0},
  {1,1,6,0,0,0,0,0,0},
  {1,1,6,0,0,0,0,0,0},
  {1,1,5,1,0,0,0,0,0},
  {1,1,5,1,0,0,0,0,0},
  {1,1,4,2,0,0,0,0,0},
  {1,1,4,2,0,0,0,0,0},
  {1,1,4,1,1,0,0,0,0},
  {1,1,4,1,1,0,0,0,0},
  {1,1,3,3,0,0,0,0,0},
  {1,1,3,3,0,0,0,0,0},
  {1,1,3,2,1,0,0,0,0},
  {1,1,3,2,1,0,0,0,0},
  {1,1,3,1,2,0,0,0,0},
  {1,1,3,1,2,0,0,0,0},
  {1,1,3,1,1,1,0,0,0},
  {1,1,3,1,1,1,0,0,0},
  {1,1,2,4,0,0,0,0,0},
  {1,1,2,4,0,0,0,0,0},
  {1,1,2,3,1,0,0,0,0},
  {1,1,2,3,1,0,0,0,0},
  {1,1,2,2,2,0,0,0,0},
  {1,1,2,2,2,0,0,0,0},
  {1,1,2,2,1,1,0,0,0},
  {1,1,2,2,1,1,0,0,0},
  {1,1,2,1,3,0,0,0,0},
  {1,1,2,1,3,0,0,0,0},
  {1,1,2,1,2,1,0,0,0},
  {1,1,2,1,2,1,0,0,0},
  {1,1,2,1,1,2,0,0,0},
  {1,1,2,1,1,2,0,0,0},
  {1,1,2,1,1,1,1,0,0},
  {1,1,2,1,1,1,1,0,0},
  {1,1,1,5,0,0,0,0,0},
  {1,1,1,5,0,0,0,0,0},
  {1,1,1,4,1,0,0,0,0},
  {1,1,1,4,1,0,0,0,0},
  {1,1,1,3,2,0,0,0,0},
  {1,1,1,3,2,0,0,0,0},
  {1,1,1,3,1,1,0,0,0},
  {1,1,1,3,1,1,0,0,0},
  {1,1,1,2,3,0,0,0,0},
  {1,1,1,2,3,0,0,0,0},
  {1,1,1,2,2,1,0,0,0},
  {1,1,1,2,2,1,0,0,0},
  {1,1,1,2,1,2,0,0,0},
  {1,1,1,2,1,2,0,0,0},
  {1,1,1,2,1,1,1,0,0},
  {1,1,1,2,1,1,1,0,0},
  {1,1,1,1,4,0,0,0,0},
  {1,1,1,1,4,0,0,0,0},
  {1,1,1,1,3,1,0,0,0},
  {1,1,1,1,3,1,0,0,0},
  {1,1,1,1,2,2,0,0,0},
  {1,1,1,1,2,2,0,0,0},
  {1,1,1,1,2,1,1,0,0},
  {1,1,1,1,2,1,1,0,0},
  {1,1,1,1,1,3,0,0,0},
  {1,1,1,1,1,3,0,0,0},
  {1,1,1,1,1,2,1,0,0},
  {1,1,1,1,1,2,1,0,0},
  {1,1,1,1,1,1,2,0,0},
  {1,1,1,1,1,1,2,0,0},
  {1,1,1,1,1,1,1,1,0},
  {1,1,1,1,1,1,1,1,0},
};


/*
 * This lookup table is to figure out how many of the columns of the
 * last lookup table are relevant.. C does not let me have variable
 * column lengths for arrays of arrays, remember?
 */

int rle_lookup_sizes[256] = {
  2, 1, 3, 2, 3, 2, 4, 3, 
  3, 2, 4, 3, 4, 3, 5, 4, 
  3, 2, 4, 3, 4, 3, 5, 4, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  3, 2, 4, 3, 4, 3, 5, 4, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  3, 2, 4, 3, 4, 3, 5, 4, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  6, 5, 7, 6, 7, 6, 8, 7, 
  3, 2, 4, 3, 4, 3, 5, 4, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  6, 5, 7, 6, 7, 6, 8, 7, 
  4, 3, 5, 4, 5, 4, 6, 5, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  6, 5, 7, 6, 7, 6, 8, 7, 
  5, 4, 6, 5, 6, 5, 7, 6, 
  6, 5, 7, 6, 7, 6, 8, 7, 
  6, 5, 7, 6, 7, 6, 8, 7, 
  7, 6, 8, 7, 8, 7, 9, 8
};




/*
 *
 * Encoding functions: zlib, lifted from example.c on the zlib
 * page, ported to encode and decode arbitrary-length char buffers into
 * dynamically allocated (and reallocated) output buffers. Fucking slow.
 *
 */

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int 
zlib_encode (void *input, unsigned long input_len, 
    void **output, unsigned long *output_len, int level)
{
  int ret, flush;
  unsigned have;
  z_stream strm;
  //unsigned char in[CHUNK];
  unsigned char *in;
  //unsigned char out[CHUNK];
  unsigned char *out_head, *out;
  unsigned long bytes_left, bytes_written;
  unsigned long output_bytes_left;
  int buf_realloc_penalty = BUF_REALLOC_PENALTY;

  bytes_left = input_len;
  bytes_written = 0;
  in = (unsigned char*) input;

  /* allocate exactly enough space to out as in */
  /* XXX this is probably too much space, since we're encoding
   * what are the performance hits of large malloc, i wonder... */
  if ( !(out_head = (unsigned char*) malloc (input_len)) ) {
    printf ("malloc failed\n");
    goto encode_error_save;
  }
  out = out_head;
  output_bytes_left = input_len;

  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  ret = deflateInit(&strm, level);
  if (ret != Z_OK) {
    //return ret;
    return 1;
  }

  /* compress until end of file */
  do {
    //strm.avail_in = fread(in, 1, CHUNK, source);
    if (bytes_left > CHUNK) {
      strm.avail_in = CHUNK;
      strm.next_in = in;
      in += CHUNK;
      //bytes_left -= CHUNK;
      flush = Z_NO_FLUSH;
    } else if (bytes_left == 0) {
      printf ("hmm, shouldnt have come here?\n");
      break;
    } else {
      strm.avail_in = bytes_left;
      strm.next_in = in;
      in += bytes_left;
      //bytes_left = 0;
      flush = Z_FINISH;
    }

    //printf ("going ahead with %d bytes\n", strm.avail_in);

    //if (ferror(source)) {
      //(void)deflateEnd(&strm);
      //return Z_ERRNO;
    //}

    //flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    //strm.next_in = in;

    /* run deflate() on input until output buffer not full, finish
       compression if all of source has been read in */
    do {

      /* to take care of the case when output is not big enough */
      if (bytes_left && output_bytes_left <= bytes_left) {
        unsigned extra = buf_realloc_penalty*CHUNK;
        unsigned offset = out - out_head;
        if (!(out_head = realloc (out_head, input_len + extra)) ) {
          printf ("allocing more bytes didnt work");
          goto encode_error_save;
        }
        out = out_head + offset;
        output_bytes_left += extra;
        buf_realloc_penalty *= BUF_REALLOC_PENALTY;
      }

      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = deflate(&strm, flush);    /* no bad return value */
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      have = CHUNK - strm.avail_out;
      out += have;

      bytes_written += have;

      //if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        //(void)deflateEnd(&strm);
        //return Z_ERRNO;
      //}
    } while (strm.avail_out == 0);
    assert(strm.avail_in == 0);     /* all input will be used */

    bytes_left = (bytes_left > CHUNK) ? (bytes_left - CHUNK) : 0;
    /* done when last data in file processed */
  } while (flush != Z_FINISH);
  assert(ret == Z_STREAM_END);        /* stream will be complete */

  /* clean up and return */
  (void)deflateEnd(&strm);

  /* general sanity checks and cleanup */
  if (bytes_written > input_len)
    printf ("encoding sucks: input was %lu but output is %lu\n",
        input_len, bytes_written);

  if ( !(out_head = realloc (out_head, bytes_written)) )  {
    perror ("realloc failed: ");
    goto encode_error_save;
  }
  *output = out_head;
  *output_len = bytes_written;
  return 0;

encode_error_save:
  if (out_head) free (out_head);
  *output_len = 0;
  return 1;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int
zlib_decode (void *input, unsigned long input_len, 
    void **output, unsigned long *output_len) 
{
  int ret;
  unsigned have;
  z_stream strm;
  //unsigned char in[CHUNK];
  //unsigned char out[CHUNK];
  unsigned char *in;
  unsigned char *out_head, *out;
  unsigned long bytes_left, bytes_written;
  unsigned long output_bytes, output_bytes_left;
  int buf_realloc_penalty = BUF_REALLOC_PENALTY;

  bytes_left = input_len;
  bytes_written = 0;
  in = (unsigned char*) input;

  /* allocate a reasonable amount of space */

  output_bytes = input_len > CHUNK_2 ? input_len : CHUNK_2;
  if ( !(out_head = (unsigned char*) malloc (output_bytes)) ) {
    printf ("decode: malloc failed\n");
    goto decode_error_save;
  }
  out = out_head;
  output_bytes_left = output_bytes;

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit(&strm);
  if (ret != Z_OK) {
    //return ret;
    return 1;
  }

  /* decompress until deflate stream ends or end of file */
  do {
    //strm.avail_in = fread(in, 1, CHUNK, source);
    if (bytes_left > CHUNK) {
      strm.avail_in = CHUNK;
      strm.next_in = in;
      in += CHUNK;
      //bytes_left -= CHUNK;
    } else if (bytes_left == 0) {
      printf ("hmm, shouldnt have come here?\n");
      break;
    } else {
      strm.avail_in = bytes_left;
      strm.next_in = in;
      in += bytes_left;
      //bytes_left = 0;
    }

    //printf ("decode: going ahead with %d bytes\n", strm.avail_in);
    //if (ferror(source)) {
      //(void)inflateEnd(&strm);
      //return Z_ERRNO;
    //}
    //if (strm.avail_in == 0)
      //break;
    //strm.next_in = in;

    /* run inflate() on input until output buffer not full */
    do {
      /* to make sure we have at least CHUNK bytes left in out buffer */
      //assert (bytes_written + CHUNK <= input_len);
      //printf ("curr size: %lu, in bytes left: %lu, out bytes left: %lu\n",
          //output_bytes, bytes_left, output_bytes_left);
      if (bytes_left && output_bytes_left < CHUNK_2) {
        unsigned extra = buf_realloc_penalty*CHUNK;
        unsigned offset = out - out_head;
        if (!(out_head = realloc (out_head, output_bytes + extra)) ) {
          printf ("allocing more bytes didnt work");
          goto decode_error_save;
        }
        output_bytes += extra;
        output_bytes_left += extra;
        out = out_head + offset;
        buf_realloc_penalty *= BUF_REALLOC_PENALTY;
      }
         
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch (ret) {
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR;     /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          (void)inflateEnd(&strm);
          //return ret;
          return 1;
      }
      have = CHUNK - strm.avail_out;

      out += have;
      bytes_written += have;
      output_bytes_left -= have;

      //if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        //(void)inflateEnd(&strm);
        //return Z_ERRNO;
      //}
    } while (strm.avail_out == 0);

    bytes_left = (bytes_left > CHUNK) ? (bytes_left - CHUNK) : 0;
      
    /* done when inflate() says it's done */
  } while (ret != Z_STREAM_END);

  /* clean up and return */
  (void)inflateEnd(&strm);

  /* prepare stuff to be returned */
  if (bytes_written < input_len)
    printf ("in decode: encoding sucks: input was %lu but "
        "output is %lu\n", input_len, bytes_written);

  if ( !(out_head = realloc (out_head, bytes_written)) )  {
    perror ("realloc failed: ");
    goto decode_error_save;
  }
  *output = out_head;
  *output_len = bytes_written;

  //return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
  return ret == Z_STREAM_END ? 0 : -1;

decode_error_save:
  if (out_head) free (out_head);
  *output_len = 0;
  return 1;
}

/* report a zlib or i/o error */
void zerr(int ret)
{
  fputs("zpipe: ", stderr);
  switch (ret) {
    case Z_ERRNO:
      if (ferror(stdin))
        fputs("error reading stdin\n", stderr);
      if (ferror(stdout))
        fputs("error writing stdout\n", stderr);
      break;
    case Z_STREAM_ERROR:
      fputs("invalid compression level\n", stderr);
      break;
    case Z_DATA_ERROR:
      fputs("invalid or incomplete deflate data\n", stderr);
      break;
    case Z_MEM_ERROR:
      fputs("out of memory\n", stderr);
      break;
    case Z_VERSION_ERROR:
      fputs("zlib version mismatch!\n", stderr);
  }
}



/* compress or decompress from stdin to stdout */
/*
   int main(int argc, char **argv)
{
  char *input, *output, *buf;
  unsigned long input_len, output_len, buf_len;


  input = (char*) strdup ("1234567890");
  input_len = strlen (input) + 1;

  // test compression 
  if ( encode (input, input_len, (void*)&output, 
        &output_len, Z_DEFAULT_COMPRESSION) ) {
    printf ("error with encode\n");
    return 1;
  }

  printf ("input compressed to size %lu\n", output_len);

  // test decompression
  if ( decode (output, output_len, (void*) &buf, &buf_len) ) {
    printf ("error with encode\n");
    return 1;
  }

  printf ("after decode: length: %lu, string: %s\n", buf_len, buf);
  return 0;
}

*/
