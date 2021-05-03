import pyopencl as ocl
import sys

cl_codus = """
constant char b2c[2] = \"-'\";

kernel void viol(global uchar* in, global uchar8* out) {
    size_t i = get_global_id(0);
    
    out[i]    = b2c[in[i] >> 7 & 1];
    out[i].s1 = b2c[in[i] >> 6 & 1];
    out[i].s2 = b2c[in[i] >> 5 & 1];
    out[i].s3 = b2c[in[i] >> 4 & 1];
    out[i].s4 = b2c[in[i] >> 3 & 1];
    out[i].s5 = b2c[in[i] >> 2 & 1];
    out[i].s6 = b2c[in[i] >> 1 & 1];
    out[i].s7 = b2c[in[i]      & 1];
}
"""

ctx   = ocl.create_some_context()
queue = ocl.CommandQueue(ctx)
prog  = ocl.Program(ctx, cl_codus).build()

str0 = 'Fuck the club!'

inbuf      = bytearray(65536)
inbuf_siz  = len(inbuf)
outbuf_siz = inbuf_siz * 8
outbuf     = bytearray(outbuf_siz)

inbuf[:len(str0)] = str0

d_inbuf  = ocl.Buffer(ctx, ocl.mem_flags.READ_ONLY | ocl.mem_flags.USE_HOST_PTR, hostbuf=str0)
d_outbuf = ocl.Buffer(ctx, ocl.mem_flags.WRITE_ONLY, outbuf_siz)

viol = prog.viol
viol(queue, (inbuf_siz,), (512,), d_inbuf, d_outbuf).wait()

ocl.enqueue_copy(queue, outbuf, d_outbuf)
sys.stdout.buffer.write(outbuf[:len(str0)*8])