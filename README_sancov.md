## Sanitizer Coverage on Linux

While Jackalope is primarily a black box binary fuzzer, it can also fuzz targets compiled with Sanitizer Coverage. This mode is only available on Linux. And vice-versa; The only instrumentation mode currently supported on Linux is Sanitizer Coverage.

### Preparing the target

The fuzzing target for this mode must be prepared as follows:

 - The target project must include `sancovclient.h` / `sancovclient.cpp`
 - The target must call `__pre_fuzz()` before and `__post_fuzz()` aafter the code being fuzzed. This defines a fuzzing iteration. Alternately, the target can use `JACKALOPE_FUZZ_LOOP` macro defined in `sancovclient.h`
 - The target should be compiled with `-fsanitize-coverage=trace-pc-guard`

Plese refer to `sancovtest.cpp` and the appropriate section in `CMakeLists.txt` as an example on how to prepare and build a target.
 
### Building Jackalope on Linux

On Linux, Jackalope should be built using Clang, otherwise building the example target binary with Sanitizer Coverage will fail.

Build Example:

```
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Running

Example:

```
./fuzzer -in in -out out -t 1000 -delivery shmem -iterations 10000 -mute_child -- ./sancovtest -m @@
```

