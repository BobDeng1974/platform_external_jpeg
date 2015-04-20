#ifndef OPENC_PACKAGE_H
#define OPENC_PACKAGE_H

#ifdef __APPLE__
#include <OpenCl/cl.h>
#endif

typedef struct opencl_context
{
    cl_context context;
    cl_command_queue queue;
    cl_device_id device_id;
}opencl_context;

typedef struct opencl_mem
{
    cl_mem base;
    void* map;
    size_t size;
    cl_command_queue queue;
}opencl_mem;

opencl_context* opencl_create_context(int usegpu);
void opencl_destroy_context(opencl_context* c);

cl_kernel opencl_compile_create_kernel(opencl_context* context, const char* sourcecode, const char* kernelname);

opencl_mem* opencl_create_mem(opencl_context* con, size_t size);
void opencl_destroy_mem(opencl_mem* mem);

enum {
    TOCPU = 0,
    TOGPU = 1
};
void opencl_sync_mem(opencl_mem* mem, int gpu);
void opencl_set_mem(cl_kernel kernel, opencl_mem* mem, int arg);

#endif
