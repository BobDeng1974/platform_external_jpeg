#ifndef JPEG_OPENCL_H
#define JPEG_OPENCL_H
#include "opencl_package.h"
#include <stdio.h>
#include "jpeglib.h"
#include "jpegint.h"

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