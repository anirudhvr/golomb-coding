/*
 * Golomb encoding and decoding. The implementation is based on
 * pseudocode taken from 'Compression and Coding Algorithms' by Alistair
 * Moffat & Andrew Turping, Kluwer Academic Publishers, 2002.
 *
 * Copyright Anirudh Ramachandran <anirudhvr@gmail.com>, 2008
 * Released under GPLv2
 */

#ifndef __ENCODE_H
#define __ENCODE_H


/* natural log of 2 */
#define LN2 .69314718055994531

int 
golomb_encode (void *input, unsigned long input_len, 
    void **output, unsigned long *output_len,
    unsigned int *golomb_param);

int
golomb_decode (void *input, unsigned long input_len, 
    unsigned int golomb_param, void **output, 
    unsigned long *output_len);


int
get_run_length_encoding (unsigned char *in, 
    unsigned long size,
    unsigned int **out,
    unsigned long *outsize);

int
get_run_length_decoding (unsigned int *in, 
    unsigned long size,
    unsigned char **out,
    unsigned long *outsize);


int 
zlib_encode (void *input, unsigned long input_len, 
    void **output, unsigned long *output_len, int level);

int
zlib_decode (void *input, unsigned long input_len, 
    void **output, unsigned long *output_len);


extern unsigned char rle_lookup[256][9];
extern int rle_lookup_sizes[256];

#endif /* __ENCODE_H */
