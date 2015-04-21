#define DCTSIZE2 64
#define DCTSIZE 8

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

__kernel void yuv_rgb(__global float* yuvbuffer, __global unsigned char* rgba, struct ComponentInfo info, int output_stride)
{
    int x = get_global_id(0);
    int y_origin = get_global_id(1);
    int yoffset = y_origin % DCTSIZE;
    int y = y_origin/DCTSIZE;
    int mcux = x/info.max_x_sample;
    int mcuy = y/info.max_y_sample;
    __global unsigned char* output = rgba + 3*(output_stride*y_origin + x*DCTSIZE);
    __global float* basic = yuvbuffer + DCTSIZE2*info.blocksInMCU*(info.MCU_Per_Row*mcuy + mcux);
    __global float* Y = basic + DCTSIZE2*((x%info.YW) + 2*(y%info.YH)) + DCTSIZE*yoffset;
    __global float* U = basic + DCTSIZE2*((x%info.UW) + 2*(y%info.UH)+info.UOffset) + DCTSIZE*yoffset;
    __global float* V = basic + DCTSIZE2*((x%info.VW) + 2*(y%info.VH)+info.VOffset) + DCTSIZE*yoffset;
    float8 yy = vload8(0, Y) + float8(128);
    float8 uu = vload8(0, U);
    float8 vv = vload8(0, V);
    uchar8 r, g, b;
    uchar8 first, second, third;
#define RESULT(x) convert_uchar8(clamp(x, float8(0), float8(255)))
    r = RESULT(yy + float8(1.40200)*vv);
    g = RESULT(yy - float8(0.34414)*uu - float8(0.71414)*vv);
    b = RESULT(yy + float8(1.77200)*uu);
#undef RESULT
    first = (uchar8)(r.s0, g.s0, b.s0, r.s1, g.s1, b.s1, r.s2, g.s2);
    second= (uchar8)(b.s2, r.s3, g.s3, b.s3, r.s4, g.s4, b.s4, r.s5);
    third = (uchar8)(g.s5, b.s5, r.s6, g.s6, b.s6, r.s7, g.s7, b.s7);
    vstore8(first, 0, output);
    vstore8(second, 1, output);
    vstore8(third, 2, output);
}