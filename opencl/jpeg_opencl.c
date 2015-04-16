#include "jpeg_opencl.h"
#include <pthread.h>
#include <assert.h>
static opencl_context* gInstance = NULL;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static cl_kernel gKernel[KERNELNUMBER];

static void initKernel(opencl_context* context)
{
    
}

opencl_context* jpeg_get_context()
{
    if (NULL == gInstance)
    {
        pthread_mutex_lock(&gMutex);
        if (NULL == gInstance)
        {
            gInstance = opencl_create_context(1);
            initKernel(gInstance);
        }
        pthread_mutex_unlock(&gMutex);
    }
    return gInstance;
}
cl_kernel jpeg_get_kernel(KERNELNAME name)
{
    assert(NULL!=gInstance);
    return gKernel[name];
}
