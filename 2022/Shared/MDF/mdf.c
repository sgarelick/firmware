#include "mdf.h"

#ifndef FEATURE_TEST
#define FEATURE_TEST 1
#endif


#if FEATURE_TEST
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("Test passed\r\n");
    return 0;
}

#endif