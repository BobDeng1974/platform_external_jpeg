#ifndef JPEG_OPENCL_H
#define JPEG_OPENCL_H
#include "opencl_package.h"
#include <stdio.h>
#include "jpeglib.h"
#include "jpegint.h"
#define DEUBUG_ON
#ifdef DEUBUG_ON
#include <sys/time.h>
#define TIME_START struct timeval tv_start, tv_end;gettimeofday(&tv_start, NULL);
#define TIME_END gettimeofday(&tv_end, NULL); printf("Time = %lu us in %s, %d\n", (tv_end.tv_sec*1000000 + tv_end.tv_usec - tv_start.tv_sec*1000000 - tv_start.tv_usec), __FUNCTION__, __LINE__);gettimeofday(&tv_start, NULL);
#else
#define TIME_START
#define TIME_END
#endif
opencl_context* jpeg_get_context();
typedef enum
{
    IDCT_FLOAT=0,
    YUV_RGB,
    
    KERNELNUMBER
}KERNELNAME;

cl_kernel jpeg_get_kernel(KERNELNAME name);
int jpeg_decode_by_opencl(j_decompress_ptr cinfo, JSAMPLE* output_buf);

#endif