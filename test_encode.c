#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "encode.h"

#define INPUTSZ 5

char *hex2bin[] = {
    "0000", "0001", "0010", "0011",
    "0100", "0101", "0110", "0111",
    "1000", "1001", "1010", "1011",
    "1100", "1101", "1110", "1111"
};

int main ()
{
    unsigned int *out;
    unsigned long outsize;
    unsigned char *decoded;
    unsigned char *ge, *gd;
    unsigned long ge_size, gd_size;
    unsigned int golomb_param;
    unsigned long decoded_size;
    unsigned char input[] = { 1, 5, 4, 5 };
    //unsigned char str[] = {0, 0, 0, 0, 8, 2, 20, 128, 30};
    //unsigned char input[INPUTSZ];
    int i;
    int inputsz = INPUTSZ;

    srand (time(NULL));

    /*
       for (i = 0; i < inputsz; ++i) 
       input[i] = rand() % 256;
     */

    if (get_run_length_encoding (input, inputsz, &out, &outsize)) {
        printf ("returned false\n");
    } else {

        printf ("encoded size: %lu\nrun-lengths: ", outsize);
        for (i = 0; i < outsize; ++i) {
            //printf ("%s ", hex2bin[out[i]]);
            printf ("%d ", out[i]);
        }
        printf ("\n");

    }

    if (get_run_length_decoding (out, outsize, &decoded, &decoded_size)) {
        printf ("decoding failure\n");
    } else {
        /*
           printf ("decoded size: %lu\nunsigned chars: ", decoded_size);
           for (i = 0; i < decoded_size; ++i) 
           printf ("%d ", decoded[i]);
           printf ("\n");
         */
    }

    //printf ("testing golomb encoding\n");
    if (golomb_encode (input, inputsz, (void*)&ge, &ge_size, &golomb_param) ) {
        printf ("golomb encoding failed\n");
    } else {

        printf ("ge param: %d, ge chars: ", golomb_param);

        for (i = 0; i < ge_size; ++i) 
            printf ("%s %s ",  hex2bin[ge[i] >> 4], hex2bin[ge[i] & 0x0f]);
        printf ("\n");


        if (golomb_decode (ge, ge_size, golomb_param, (void**)&gd, &gd_size)) {
            printf ("golomb decoding failed\n");
        } else {
            if (gd_size != inputsz) goto print_on_error;

            for (i = 0; i < inputsz; ++i) {
                if (gd[i] != input[i]) {
                    printf ("entry %d mismatches\n", i);
                    goto print_on_error;
                }
            }

        }
    }
    return 0;

print_on_error:
    printf ("input unsigned chars: ");
    for (i = 0; i < inputsz; ++i)
        printf ("%d ", input[i]);
    printf ("\n");

    printf ("gd chars: ");
    for (i = 0; i < gd_size; ++i) 
        printf ("%d ", gd[i]);
    printf ("\n");
    return 0;
}


