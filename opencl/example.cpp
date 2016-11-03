//
//  main.cpp
//  jpeg_opencl
//
//  Created by jiangxiaotang on 15/4/15.
//  Copyright (c) 2015年 jiangxiaotang. All rights reserved.
//

#include <iostream>
extern "C"
{
#include "jpeglib.h"
#include "jpeg_opencl.h"
};
void write_JPEG_file (const char * filename, JSAMPLE* image_buffer, int image_width, int image_height)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;        /* target file */
    JSAMPROW row_pointer[1];    /* pointer to JSAMPLE row[s] */
    int row_stride;        /* physical row width in image buffer */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = image_width;     /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;        /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;     /* colorspace of input image */
    jpeg_set_defaults(&cinfo);
    
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = image_width * 3;    /* JSAMPLEs per row in image_buffer */
    
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
}

int test_main() {
    const char* inputfile = "/Users/jiangxiaotang/Documents/platform_external_jpeg/qianlong.jpg";
    const char* outputfile = "/Users/jiangxiaotang/Documents/platform_external_jpeg/qianlong_output.jpeg";
    struct jpeg_decompress_struct cinfo;
    FILE* infile;
    JSAMPARRAY buffer;
    int row_stride;
    if ((infile = fopen(inputfile, "rb")) == NULL)
    {
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_stdio_src(&cinfo, infile);
    (void) jpeg_read_header(&cinfo, TRUE);
    cinfo.dct_method = JDCT_FLOAT;
    (void) jpeg_start_decompress(&cinfo);
    auto width = cinfo.output_width;
    auto height = cinfo.output_height;
    auto pixels = (JSAMPLE*)(malloc(width*height*3));
    TIME_START;
    if (1)
    {
        jpeg_decode_by_opencl(&cinfo, pixels);
        (void) jpeg_abort_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
    }
    else
    {
        row_stride = cinfo.output_width * cinfo.output_components;
        buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
        auto cur = pixels;
        int lines = 0;
        while (cinfo.output_scanline < cinfo.output_height)
        {
            JSAMPLE* cbuffer;
            (void) jpeg_read_scanlines(&cinfo, buffer, 1);
            cbuffer = (JSAMPLE*)(buffer[0]);
            ::memcpy(cur, cbuffer, 3*width);
            cur += 3*width;
            lines++;
        }
        (void) jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
    }
    
    fclose(infile);
    TIME_END;
    
    write_JPEG_file(outputfile, pixels, width, height);
    free(pixels);
    return 0;
}

int main()
{
    for (int i=0; i<10;++i)
    {
        test_main();
    }
    return 0;
}
