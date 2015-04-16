#ifndef JPEG_OPENCL_H
#define JPEG_OPENCL_H
#include "opencl_package.h"

opencl_context* jpeg_get_context();
typedef enum
{
    IDCT_FLOAT=0,
    YUV_RGB,
    
    KERNELNUMBER
}KERNELNAME;

cl_kernel jpeg_get_kernel(KERNELNAME name);

#endif