This is a OpenCL accelarated implementation of the K3C (formely 5CP) specified raw *violeur format* encoder and decoder written in C99, to benifit off any type of OpenCL compatible device not just the CPU, for now it chooses the default.

### Building

This requires the [ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader) and [OpenCL headers](https://github.com/KhronosGroup/OpenCL-Headers) and assumes that those are in compiler path.

The default `Makefile` uses `gcc` and getopt-style which is supported almost everywhere, to compile on different compiler do `make CC=cumpiler CFLAGS="-I/usr/include -L/usr/lib64" build` (you might omit `CFLAGS` as needed).

The resultant executable is `chlr.out` or `.exe` suffixed if on Windows.

### Using

#### Environment variables
- `IO_BUFFER_SIZE` is a `size_t` which defines the minimum buffer size of data, by default 65536 bytes.
- `CL_LWG_SIZE` is a `size_t` which defines the OpenCL work-group size, by default maxed out. If `IO_BUFFER_SIZE` is not a mulitple of it, `IO_BUFFER_SIZE` is rounded up to the nearest multiple of it.

For example to encode text `FINNA BUST A NUT TONIGHT`.

```bash
$ ./chlr.out e /dev/stdin /dev/stdout <<< 'FINNA BUST A NUT TONIGHT'
-'---''--'--'--'-'--'''--'--'''--'-----'--'------'----'--'-'-'-'-'-'--''-'-'-'----'------'-----'--'------'--'''--'-'-'-'-'-'-'----'------'-'-'---'--''''-'--'''--'--'--'-'---'''-'--'----'-'-'------'-'-
```

And, with environs.

```
$ IO_BUFFER_SIZE=16384 CL_LWG_SIZE=64 ./chlr.out enc /proc/cpuinfo /tmp/cholorinfo
```

And, to decode. If the last blocking read would return bytes with size not mutiple to eight, last missing bytes will be zero concealed.

```
$ ./chlr.out d /tmp/cholorinfo /dev/stdout
```
