#include "jpeg_opencl.h"
#include <pthread.h>
#include <assert.h>
#include <string.h>
static opencl_context* gInstance = NULL;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static cl_kernel gKernel[KERNELNUMBER];
#include "KernelWarp.h"
static void initKernel(opencl_context* context)
{
    gKernel[IDCT_FLOAT] = opencl_compile_create_kernel(context, idct_kernel_clclh, "idct_float");
    //gKernel[YUV_RGB] = opencl_compile_create_kernel(context,yuv_rgb_clclh, "convert");
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

int jpeg_decode_by_opencl(j_decompress_ptr cinfo, JSAMPLE* output_buf)
{
    int i, blkn;
    size_t sta, fin;
    size_t totalMCUNumber = cinfo->MCU_rows_in_scan * cinfo->MCUs_per_row;
    size_t totalBlock = totalMCUNumber*cinfo->blocks_in_MCU;
    opencl_context* context = jpeg_get_context();
    opencl_mem* inputBuffer = opencl_create_mem(context, totalBlock * sizeof(JBLOCK));
    JBLOCK* mcu = (JBLOCK*)inputBuffer->map;
    JBLOCK** MCU_buffer = (JBLOCK**)malloc(sizeof(JBLOCK*)*cinfo->blocks_in_MCU);
    opencl_mem* yuvBuffer = opencl_create_mem(context, totalMCUNumber*cinfo->blocks_in_MCU * DCTSIZE2 * sizeof(float));
    opencl_mem* rgbBuffer = opencl_create_mem(context, 3*sizeof(unsigned char)*DCTSIZE2*cinfo->MCU_rows_in_scan * cinfo->MCUs_per_row);
    opencl_mem* tablebuffer = opencl_create_mem(context, cinfo->comps_in_scan*DCTSIZE2*sizeof(float));
    opencl_mem* offsetbuffer = opencl_create_mem(context, cinfo->blocks_in_MCU*sizeof(int));
    int currentoffset = 0;
    assert(cinfo->dct_method == JDCT_FLOAT);
    assert(NULL!=yuvBuffer);
    assert(NULL!=rgbBuffer);
    assert(NULL!=MCU_buffer);
    assert(NULL!=tablebuffer);
    assert(NULL!=inputBuffer);
    assert(NULL!=offsetbuffer);
    sta = clock();
    opencl_sync_mem(inputBuffer, TOCPU);
    jzero_far(inputBuffer->map, totalMCUNumber*cinfo->blocks_in_MCU * sizeof(JBLOCK));
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
    opencl_sync_mem(inputBuffer, 1);
    fin = clock();
    printf("Opencl time is %lu / %d, in %s, %d\n", fin-sta, CLOCKS_PER_SEC, __func__, __LINE__);
    /*Upload quantry table*/
    opencl_sync_mem(tablebuffer, 0);
    opencl_sync_mem(offsetbuffer, 0);
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
    opencl_sync_mem(tablebuffer, TOGPU);
    opencl_sync_mem(offsetbuffer, TOGPU);
    //TODO
    /*idct*/
    {
        int error;
        cl_kernel kernel = jpeg_get_kernel(IDCT_FLOAT);
        opencl_set_mem(kernel, inputBuffer, 0);
        opencl_set_mem(kernel, yuvBuffer, 1);
        opencl_set_mem(kernel, tablebuffer, 2);
        opencl_set_mem(kernel, offsetbuffer, 3);
        error = clSetKernelArg(kernel, 4, sizeof(int), &cinfo->blocks_in_MCU);
        assert(CL_SUCCESS == error);
        error = clEnqueueNDRangeKernel(context->queue, kernel, 1, NULL, &totalBlock, NULL, 0, NULL, NULL);
        assert(CL_SUCCESS == error);
    }
    opencl_destroy_mem(inputBuffer);
    opencl_destroy_mem(tablebuffer);
    /*TODO Sample YUV to RGB*/
    {
        opencl_sync_mem(yuvBuffer, TOCPU);
        fin = clock();
        printf("Opencl time is %lu / %d, in %s, %d\n", fin-sta, CLOCKS_PER_SEC, __func__, __LINE__);
        memset(output_buf, 128, cinfo->output_width*cinfo->output_height*3);
        for (int i=0; i<cinfo->output_height; ++i)
        {
            for (int j=0; j<cinfo->output_width; ++j)
            {
                int mcu_y = i / 8;
                int mcu_x = j / 8;
                int by = i%8;
                int bx = j%8;
                float* blockbasic = (float*)(yuvBuffer->map) + DCTSIZE2*3*(mcu_x + cinfo->MCUs_per_row*mcu_y);
                float y = *(blockbasic + by * 8 + bx) + 128;
                float u = *(blockbasic + by * 8 + bx + DCTSIZE2);
                float v = *(blockbasic + by * 8 + bx + DCTSIZE2*2);
#define limit(r) r = r>255?255:r; r=r<0?0:r;
                float r = y + 1.40200f*v;
                limit(r);
                float g = y - 0.34414*u - 0.71414*v;
                limit(g);
                float b = y + 1.77200f*u;
                limit(b);
                JSAMPLE* basic = (output_buf + (cinfo->output_width * i + j)*3);
#undef limit
                basic[0] = r;
                basic[1] = g;
                basic[2] = b;
            }
        }
    }
    ////
    opencl_destroy_mem(yuvBuffer);
    opencl_destroy_mem(offsetbuffer);
    /*Copy rgbbuffer to output_buf*/
    
    opencl_destroy_mem(rgbBuffer);
    
    fin = clock();
    printf("Opencl time is %lu / %d, in %s, %d\n", fin-sta, CLOCKS_PER_SEC, __func__, __LINE__);
    return 0;
}

