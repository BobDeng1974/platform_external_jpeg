#include "opencl_package.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

opencl_context* opencl_create_context(int usegpu)
{
    opencl_context* con = (opencl_context*)malloc(sizeof(opencl_context));
    assert(NULL!=con);
    int flags = usegpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU;
    con->context = clCreateContextFromType(NULL, flags, NULL, NULL, NULL);
    assert(NULL!=con->context);
    size_t deviceListSize;
    clGetContextInfo(con->context,CL_CONTEXT_DEVICES, 0,NULL,&deviceListSize);
    cl_device_id* devices = (cl_device_id*)malloc(sizeof(cl_device_id)*deviceListSize);
    assert(NULL!=devices);
    clGetContextInfo(con->context, CL_CONTEXT_DEVICES, deviceListSize, devices, NULL);
    con->device_id = devices[0];
    con->queue = clCreateCommandQueue(con->context, con->device_id, 0, NULL);
    free(devices);
    return con;
}

void opencl_destroy_context(opencl_context* c)
{
    assert(NULL!=c);
    clReleaseCommandQueue(c->queue);
    clReleaseDevice(c->device_id);
    clReleaseContext(c->context);
}

cl_kernel opencl_compile_create_kernel(opencl_context* c, const char* sourcecode, const char* kernalname)
{
    assert(NULL!=sourcecode);
    assert(NULL!=kernalname);
    size_t sourcesize[] = {strlen(sourcecode)};
    cl_int errercode = CL_SUCCESS;
    cl_program program = clCreateProgramWithSource(c->context, 1, &sourcecode, sourcesize, &errercode);
    assert(errercode == CL_SUCCESS);
    errercode = clBuildProgram(program, 1, &(c->device_id), NULL, NULL, NULL);
    if (CL_SUCCESS != errercode)
    {
        size_t len;
        clGetProgramBuildInfo(program, c->device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
        char* buffer = (char*)malloc(sizeof(char)*(len+1));
        clGetProgramBuildInfo(program, c->device_id, CL_PROGRAM_BUILD_LOG, len, buffer, &len);
        printf("Build log is %s\n", buffer);//TODO use FUNC_PRINT_ALL
        assert(CL_SUCCESS == errercode);
        free(buffer);
    }
    cl_kernel kernel = clCreateKernel(program, kernalname, &errercode);
    assert(errercode == CL_SUCCESS);
    errercode = clReleaseProgram(program);
    assert(errercode == CL_SUCCESS);
    return kernel;
}

opencl_mem* opencl_create_mem(opencl_context* con, int size)
{
    opencl_mem* res = (opencl_mem*)malloc(sizeof(opencl_mem));
    res->size = size;
    res->base = clCreateBuffer(con->context, CL_MEM_ALLOC_HOST_PTR, size, NULL, NULL);
    assert(NULL!=res->base);
    res->map = clEnqueueMapBuffer(con->queue, res->base, CL_FALSE, CL_MAP_READ|CL_MAP_WRITE, 0, size, 0, NULL, NULL, NULL);
    res->queue = con->queue;
    assert(NULL!=res->map);
    return res;
}
void opencl_destroy_mem(opencl_mem* mem)
{
    assert(NULL!=mem);
    clEnqueueUnmapMemObject(mem->queue, mem->base, mem->map, 0, NULL, NULL);
}

void opencl_sync_mem(opencl_mem* mem)
{
    assert(NULL!=mem);
    clEnqueueUnmapMemObject(mem->queue, mem->base, mem->map, 0, NULL, NULL);
    mem->map = clEnqueueMapBuffer(mem->queue, mem->base, CL_FALSE, CL_MAP_READ|CL_MAP_WRITE, 0, mem->size, 0, NULL, NULL, NULL);
    assert(NULL!=mem->map);
}