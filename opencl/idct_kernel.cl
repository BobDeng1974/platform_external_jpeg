#define DCTSIZE 8
#define DCTSIZE2 64


#define LOADSRC(i, src) convert_float8(vload8(i, src))*vload8(i, table)
__kernel void idct_float(__global short* input, __global unsigned char* output, __global const float* dequantilize_table, __global const int* order, int blocks_per_mcu, size_t totalblocks)
{
    int blkn = get_global_id(0);
    if (blkn < totalblocks)
    {
        __global short* src = input + DCTSIZE2*blkn;
        __global unsigned char* outptr = output + DCTSIZE2*blkn;
        __global const float* table = dequantilize_table + order[blkn % blocks_per_mcu]*DCTSIZE2;
        float8 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
        float8 w0, w1, w2, w3, w4, w5, w6, w7;
        float8 tmp10, tmp11, tmp12, tmp13;
        float8 z5, z10, z11, z12, z13;
        tmp0 = LOADSRC(0, src);
        tmp1 = LOADSRC(2, src);
        tmp2 = LOADSRC(4, src);
        tmp3 = LOADSRC(6, src);
        tmp10 = tmp0 + tmp2;	/* phase 3 */
        tmp11 = tmp0 - tmp2;
        
        tmp13 = tmp1 + tmp3;	/* phases 5-3 */
        tmp12 = (tmp1 - tmp3) * float8(1.414213562) - tmp13; /* 2*c4 */
        
        tmp0 = tmp10 + tmp13;	/* phase 2 */
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;
        
        tmp4 = LOADSRC(1, src);
        tmp5 = LOADSRC(3, src);
        tmp6 = LOADSRC(5, src);
        tmp7 = LOADSRC(7, src);
        
        z13 = tmp6 + tmp5;		/* phase 6 */
        z10 = tmp6 - tmp5;
        z11 = tmp4 + tmp7;
        z12 = tmp4 - tmp7;
        
        tmp7 = z11 + z13;		/* phase 5 */
        tmp11 = (z11 - z13) * float8(1.414213562); /* 2*c4 */
        
        z5 = (z10 + z12) * float8(1.847759065); /* 2*c2 */
        tmp10 = float8(1.082392200) * z12 - z5; /* 2*(c2-c6) */
        tmp12 = float8(-2.613125930) * z10 + z5; /* -2*(c2+c6) */
        
        tmp6 = tmp12 - tmp7;	/* phase 2 */
        tmp5 = tmp11 - tmp6;
        tmp4 = tmp10 + tmp5;
        
        tmp0 = tmp0 + tmp7;
        tmp7 = tmp0 - float8(2)*tmp7;
        tmp1 = tmp1 + tmp6;
        tmp6 = tmp1 - float8(2)*tmp6;
        tmp2 = tmp2 + tmp5;
        tmp5 = tmp2 - float8(2)*tmp5;
        tmp4 = tmp3 + tmp4;
        tmp3 = float8(2)*tmp3 - tmp4;
        /*Cross*/
#define TRANS(w, i) w##i = (float8)(tmp0.s##i, tmp1.s##i, tmp2.s##i, tmp3.s##i, tmp4.s##i, tmp5.s##i, tmp6.s##i, tmp7.s##i)
        TRANS(w, 0);
        TRANS(w, 1);
        TRANS(w, 2);
        TRANS(w, 3);
        TRANS(w, 4);
        TRANS(w, 5);
        TRANS(w, 6);
        TRANS(w, 7);
#undef TRANS
        
        tmp10 = w0 + w4;
        tmp11 = w0 - w4;
        
        tmp13 = w2 + w6;
        tmp12 = (w2 - w6) * float8(1.414213562) - tmp13;
        
        tmp0 = tmp10 + tmp13;
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;
        
        z13 = w5 + w3;
        z10 = w5 - w3;
        z11 = w1 + w7;
        z12 = w1 - w7;
        
        tmp7 = z11 + z13;
        tmp11 = (z11 - z13) * float8(1.414213562);
        
        z5 = (z10 + z12) * float8(1.847759065); /* 2*c2 */
        tmp10 = float8(1.082392200) * z12 - z5; /* 2*(c2-c6) */
        tmp12 = float8(-2.613125930) * z10 + z5; /* -2*(c2+c6) */
        
        tmp6 = tmp12 - tmp7;
        tmp5 = tmp11 - tmp6;
        tmp4 = tmp10 + tmp5;
        
        tmp0 = tmp0 + tmp7;
        tmp7 = tmp0 - float8(2)*tmp7;
        tmp1 = tmp1 + tmp6;
        tmp6 = tmp1 - float8(2)*tmp6;
        tmp2 = tmp2 + tmp5;
        tmp5 = tmp2 - float8(2)*tmp5;
        tmp4 = tmp3 + tmp4;
        tmp3 = float8(2)*tmp3 - tmp4;
        /*Cross*/
#define TRANS(w, i) w##i = (float8)(tmp0.s##i, tmp1.s##i, tmp2.s##i, tmp3.s##i, tmp4.s##i, tmp5.s##i, tmp6.s##i, tmp7.s##i)
        TRANS(w, 0);
        TRANS(w, 1);
        TRANS(w, 2);
        TRANS(w, 3);
        TRANS(w, 4);
        TRANS(w, 5);
        TRANS(w, 6);
        TRANS(w, 7);
#undef TRANS
        /* Final output stage: scale down by a factor of 8 and range-limit */
#define RESULT(t) convert_uchar8(clamp((t)/float8(8)+float8(128), float8(0), float8(255)))
        vstore8(RESULT(w0), 0, outptr);
        vstore8(RESULT(w7), 7, outptr);
        vstore8(RESULT(w1), 1, outptr);
        vstore8(RESULT(w6), 6, outptr);
        vstore8(RESULT(w2), 2, outptr);
        vstore8(RESULT(w5), 5, outptr);
        vstore8(RESULT(w4), 4, outptr);
        vstore8(RESULT(w3), 3, outptr);
#undef RESULT
    }
}