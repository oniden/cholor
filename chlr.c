#define CL_TARGET_OPENCL_VERSION 100

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <CL/cl.h>

#include "clprog.h"

#define DEFAULT_IO_BUFFER_SIZE (1024 * 64)

static inline size_t round_up(size_t n, size_t m) {
    return n - 1 - (n - 1) % m + m;
}

static inline void print_usage(const char* name) {
    fprintf(stderr, "usage: %s [mode=e/enc/encode/d/dec/decode] [infile] [ofile]\n", name), exit(0);
}

int main(int argc, char** argv) {
    if(argc < 4) print_usage(argv[0]);

    struct {
        cl_int err;
        cl_platform_id pf;
        cl_device_id dev;
        cl_context ctx;
        cl_command_queue cmdq;
        cl_program prog;
        cl_kernel kern;
        struct { cl_mem in, out; } buf;
        size_t lwg_size;
    } cl;
    
    enum { op_encode, op_decode } op_mode;
         if(*argv[1] == 'e' || strcmp(argv[1], "enc") == 0 || strcmp(argv[1], "encode") == 0) op_mode = op_encode;
    else if(*argv[1] == 'd' || strcmp(argv[1], "dec") == 0 || strcmp(argv[1], "decode") == 0) op_mode = op_decode;
    else print_usage(argv[0]);
    
    {   // initialisation work, to get OpenCL up and running.
        assert(clGetPlatformIDs(1, &cl.pf, NULL) == CL_SUCCESS);
        assert(clGetDeviceIDs(cl.pf, CL_DEVICE_TYPE_DEFAULT, 1, &cl.dev, NULL) == CL_SUCCESS);
        assert((cl.ctx = clCreateContext(NULL, 1, &cl.dev, NULL, NULL, &cl.err), cl.err == CL_SUCCESS));
        assert((cl.cmdq = clCreateCommandQueue(cl.ctx, cl.dev, 0, &cl.err), cl.err == CL_SUCCESS));
        assert((cl.prog = clCreateProgramWithSource(cl.ctx, 1, &cl_codus, NULL, &cl.err), cl.err == CL_SUCCESS));
        assert(clBuildProgram(cl.prog, 0, NULL, NULL, NULL, NULL) == CL_SUCCESS);

        // retrieve the max work group size.
        clGetDeviceInfo(cl.dev, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &cl.lwg_size, NULL);
    }

    FILE *in  = fopen(argv[2], "rb");
    FILE *out = fopen(argv[3], "wb");

    if(in == NULL) fputs("error: input file not found\n", stderr), exit(-1);
    if(out == NULL) fputs("error: output file not found\n", stderr), fclose(in), exit(-1);

    // disable buffering, else it is (our buffering + internal buffer).
    setvbuf(in,  NULL, _IONBF, 0);
    setvbuf(out, NULL, _IONBF, 0);

    char *inbuf, *outbuf;
    size_t inbufsiz = DEFAULT_IO_BUFFER_SIZE;
    size_t outbufsiz;

    {
        const char* cl_lwg_size = getenv("CL_LWG_SIZE");
        const char* io_buf_size = getenv("IO_BUF_SIZE");
        size_t     max_lwg_size = cl.lwg_size;

        if(cl_lwg_size != NULL) sscanf(cl_lwg_size, "%zu", &cl.lwg_size);
        if(io_buf_size != NULL) sscanf(io_buf_size, "%zu", &inbufsiz);

        if(cl.lwg_size > max_lwg_size)
            fprintf(stderr, "warning: CL_LWG_SIZE exceeds range of (0..%zu], clipping\n", max_lwg_size);

        if(inbufsiz % cl.lwg_size != 0)
            fputs("warning: IO_BUFFER_SIZE is not a multiple of CL_LWG_SIZE, rounding up\n", stderr);

        cl.lwg_size = cl.lwg_size > max_lwg_size ? max_lwg_size : cl.lwg_size;
        
        if(op_mode == op_encode) {
            inbufsiz  = round_up(inbufsiz, cl.lwg_size);
            outbufsiz = inbufsiz * 8;
        } else if(op_mode == op_decode) {
            outbufsiz = round_up(inbufsiz, cl.lwg_size);
            inbufsiz  = outbufsiz * 8;
        }
    }

    assert((inbuf = malloc(inbufsiz)) != NULL && (outbuf = malloc(outbufsiz)) != NULL);

    cl.buf.in  = clCreateBuffer(cl.ctx, 
        CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY | CL_MEM_USE_HOST_PTR, 
        inbufsiz, inbuf, NULL);
    
    cl.buf.out = clCreateBuffer(cl.ctx, 
        CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
        outbufsiz, NULL, NULL);
    
    cl.kern = clCreateKernel(cl.prog, op_mode == op_encode ? "viol" : "unviol", &cl.err);

    clSetKernelArg(cl.kern, 0, sizeof(cl_mem), &cl.buf.in);
    clSetKernelArg(cl.kern, 1, sizeof(cl_mem), &cl.buf.out);

    if(op_mode == op_encode)
        for(size_t insize; (insize = fread(inbuf, 1, inbufsiz, in)) > 0;) {
            clEnqueueWriteBuffer(cl.cmdq, cl.buf.in, CL_TRUE, 0, insize, inbuf, 0, NULL, NULL);
            clEnqueueNDRangeKernel(cl.cmdq, cl.kern, 1, NULL,
                &(const size_t){round_up(insize, cl.lwg_size)},
                &cl.lwg_size, 0, NULL, NULL);
            clEnqueueReadBuffer(cl.cmdq, cl.buf.out, CL_TRUE, 0, insize * 8, outbuf, 0, NULL, NULL);

            fwrite(outbuf, 8, insize, out);
        }
    else if(op_mode == op_decode)
        for(size_t insize; (insize = fread(inbuf, 1, inbufsiz, in)) > 0;) {
            if(insize % 8 != 0) {
                size_t isz_rem = insize % 8;
                memset(inbuf + insize + isz_rem, 0, 8 - isz_rem);
                insize = round_up(insize, 8);
            }
            size_t outsize = insize / 8;

            clEnqueueWriteBuffer(cl.cmdq, cl.buf.in, CL_TRUE, 0, insize, inbuf, 0, NULL, NULL);
            clEnqueueNDRangeKernel(cl.cmdq, cl.kern, 1, NULL,
                &(const size_t){round_up(outsize, cl.lwg_size)},
                &cl.lwg_size, 0, NULL, NULL);
            clEnqueueReadBuffer(cl.cmdq, cl.buf.out, CL_TRUE, 0, outsize, outbuf, 0, NULL, NULL);

            fwrite(outbuf, 1, outsize, out);
        }
    
    clReleaseKernel(cl.kern);
    clReleaseProgram(cl.prog);
    clReleaseCommandQueue(cl.cmdq);
    clReleaseContext(cl.ctx);
    clReleaseDevice(cl.dev);

    free(inbuf);
    free(outbuf);

    fclose(in);
    fclose(out);
}
