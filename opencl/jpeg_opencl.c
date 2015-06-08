#include "jpeg_opencl.h"
#include <pthread.h>
#include <assert.h>
#include <string.h>


static opencl_context* gInstance = NULL;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static cl_kernel gKernel[KERNELNUMBER];
static size_t gMaxGroupSize[KERNELNUMBER];
#include "KernelWarp.h"
static void initKernel(opencl_context* context)
{
    int error;
    gKernel[IDCT_FLOAT] = opencl_compile_create_kernel(context, idct_kernel_clclh, "idct_float");
    gKernel[YUV_RGB] = opencl_compile_create_kernel(context,yuv_rgb_clclh, "yuv_rgb");
    error = clGetKernelWorkGroupInfo(gKernel[IDCT_FLOAT], gInstance->device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), gMaxGroupSize+IDCT_FLOAT, NULL);
    assert(CL_SUCCESS == error);
    error = clGetKernelWorkGroupInfo(gKernel[YUV_RGB], gInstance->device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), gMaxGroupSize+YUV_RGB, NULL);
    assert(CL_SUCCESS == error);
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

struct ComponentInfo
{
    int max_x_sample;
    int max_y_sample;
    int YW;
    int YH;
    int UW;
    int UH;
    int UOffset;
    int VW;
    int VH;
    int VOffset;
    int blocksInMCU;
    int MCU_Per_Row;
};


int jpeg_decode_by_opencl(j_decompress_ptr cinfo, JSAMPLE* output_buf)
{
    int i, blkn;
    size_t totalMCUNumber = cinfo->MCU_rows_in_scan * cinfo->MCUs_per_row;
    size_t totalBlock = totalMCUNumber*cinfo->blocks_in_MCU;
    opencl_context* context;
    opencl_mem* inputBuffer;
    JBLOCK* mcu;
    JBLOCK** MCU_buffer;
    opencl_mem* yuvBuffer;
    opencl_mem* tablebuffer;
    opencl_mem* offsetbuffer;
    int currentoffset = 0;
    TIME_START;
    if (cinfo->dct_method != JDCT_FLOAT || cinfo->comps_in_scan != 3 || DCTSIZE != 8)
    {
        return -1;
    }
    context = jpeg_get_context();
    TIME_END;
    for (i=0;i<3; ++i)
    {
        assert(cinfo->cur_comp_info[i]->DCT_scaled_size == 8);
    }
    inputBuffer = opencl_create_mem(context, totalBlock * sizeof(JBLOCK));
    opencl_sync_mem(inputBuffer, TOCPU);
    jzero_far(inputBuffer->map, totalMCUNumber*cinfo->blocks_in_MCU * sizeof(JBLOCK));
    MCU_buffer = (JBLOCK**)malloc(sizeof(JBLOCK*)*cinfo->blocks_in_MCU);
    mcu = (JBLOCK*)inputBuffer->map;
    for (i = 0; i < totalMCUNumber; ++i)
    {
        for (blkn = 0; blkn < cinfo->blocks_in_MCU; ++blkn)
        {
            MCU_buffer[blkn] = mcu + cinfo->blocks_in_MCU*i + blkn;
        }
        if ( FALSE == (*cinfo->entropy->decode_mcu) (cinfo, MCU_buffer))
        {
            break;
        }
    }
    free(MCU_buffer);
    opencl_sync_mem(inputBuffer, TOGPU);
    TIME_END;
    /*Upload quantry table*/
    offsetbuffer = opencl_create_mem(context, cinfo->blocks_in_MCU*sizeof(int));
    tablebuffer = opencl_create_mem(context, cinfo->comps_in_scan*DCTSIZE2*sizeof(float));
    opencl_sync_mem(tablebuffer, TOCPU);
    opencl_sync_mem(offsetbuffer, TOCPU);
    for (i = 0; i< cinfo->comps_in_scan; ++i)
    {
        int j;
        jpeg_component_info* info = cinfo->cur_comp_info[i];
        float* table = (float*)info->dct_table;//FIXME
        float* table_in_buffer = (float*)(tablebuffer->map) + DCTSIZE2*i;
        int* offset = (int*)offsetbuffer->map;
        memcpy(table_in_buffer, table, sizeof(float)*DCTSIZE2);
        for (j=0; j<info->h_samp_factor*info->v_samp_factor; ++j)
        {
            offset[currentoffset + j] = i;
        }
        currentoffset+=info->h_samp_factor*info->v_samp_factor;
    }
    TIME_END;
    opencl_sync_mem(tablebuffer, TOGPU);
    opencl_sync_mem(offsetbuffer, TOGPU);
    yuvBuffer = opencl_create_mem(context, totalMCUNumber*cinfo->blocks_in_MCU * DCTSIZE2 * sizeof(float));
    TIME_END;
    /*idct*/
    {
        int error;
        size_t alignblocks;
        cl_kernel kernel = jpeg_get_kernel(IDCT_FLOAT);
        opencl_set_mem(kernel, inputBuffer, 0);
        opencl_set_mem(kernel, yuvBuffer, 1);
        opencl_set_mem(kernel, tablebuffer, 2);
        opencl_set_mem(kernel, offsetbuffer, 3);
        error = clSetKernelArg(kernel, 4, sizeof(int), &cinfo->blocks_in_MCU);
        assert(CL_SUCCESS == error);
        error = clSetKernelArg(kernel, 5, sizeof(size_t), &totalBlock);
        assert(CL_SUCCESS == error);
        alignblocks = (totalBlock + gMaxGroupSize[IDCT_FLOAT]-1)/gMaxGroupSize[IDCT_FLOAT]*gMaxGroupSize[IDCT_FLOAT];
        error = clEnqueueNDRangeKernel(context->queue, kernel, 1, NULL, &alignblocks, gMaxGroupSize+IDCT_FLOAT, 0, NULL, NULL);
        assert(CL_SUCCESS == error);
    }
    opencl_destroy_mem(inputBuffer);
    opencl_destroy_mem(tablebuffer);
    opencl_destroy_mem(offsetbuffer);
    TIME_END;
    /*TODO Sample YUV to RGB*/
    {
        opencl_mem* rgbBuffer = opencl_create_mem(context, 3*sizeof(unsigned char)*DCTSIZE2*cinfo->MCU_rows_in_scan * cinfo->MCUs_per_row * cinfo->max_h_samp_factor * cinfo->max_v_samp_factor);
        int stride = cinfo->MCUs_per_row * cinfo->max_h_samp_factor * DCTSIZE;
        struct ComponentInfo info;
        size_t global[2] = {cinfo->MCUs_per_row*cinfo->max_h_samp_factor, cinfo->MCU_rows_in_scan*cinfo->max_v_samp_factor*DCTSIZE};
        cl_kernel kernel = jpeg_get_kernel(YUV_RGB);
        opencl_sync_mem(rgbBuffer, TOGPU);
        assert(NULL!=rgbBuffer);
        info.YW = cinfo->cur_comp_info[0]->h_samp_factor;
        info.YH = cinfo->cur_comp_info[0]->v_samp_factor;
        info.UW = cinfo->cur_comp_info[1]->h_samp_factor;
        info.UH = cinfo->cur_comp_info[1]->v_samp_factor;
        info.VW = cinfo->cur_comp_info[2]->h_samp_factor;
        info.VH = cinfo->cur_comp_info[2]->v_samp_factor;
        info.UOffset = info.YW*info.YH;
        info.VOffset = info.UOffset + info.UW*info.UH;
        info.blocksInMCU = cinfo->blocks_in_MCU;
        info.MCU_Per_Row = cinfo->MCUs_per_row;
        info.max_x_sample = cinfo->max_h_samp_factor;
        info.max_y_sample = cinfo->max_v_samp_factor;
        {
            int error = clSetKernelArg(kernel, 2, sizeof(struct ComponentInfo), &info);
            opencl_set_mem(kernel, yuvBuffer, 0);
            opencl_set_mem(kernel, rgbBuffer, 1);
            assert(CL_SUCCESS == error);
            error = clSetKernelArg(kernel, 3, sizeof(int), &stride);
            assert(CL_SUCCESS == error);
            error = clEnqueueNDRangeKernel(context->queue, kernel, 2, NULL, global, NULL, 0, NULL, NULL);
            assert(CL_SUCCESS == error);
        }
        /*Copy rgbbuffer to output_buf*/
        opencl_sync_mem(rgbBuffer, TOCPU);
        TIME_END;
        for (i=0; i<cinfo->output_height; ++i)
        {
            unsigned char* src = (unsigned char*)(rgbBuffer->map) + 3*stride*i;
            unsigned char* dst = output_buf + 3*cinfo->output_width*i;
            memcpy(dst, src, 3*cinfo->output_width);
        }
        opencl_destroy_mem(rgbBuffer);
        TIME_END;
    }
    opencl_destroy_mem(yuvBuffer);
    return 1;
}

