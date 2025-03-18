# libxslt fuzzer

This example demonstrates the use of two non-default Jackalope modes:
 - sanitizer coverage mode, which uses compiler-level instrumentation and is an effective way of getting coverage when the target source code is available.
 - grammar fuzzing mode, which generates and mutates samples according to [grammar rules](https://github.com/googleprojectzero/Jackalope/blob/main/mutators/grammar/README.md) and can be used for structured and language fuzzing.

Note that Jackalope only supports sanitizer coverage mode on Linux.

This fuzzer has been used to find two security issues in libxslt; CVE-2024-55549 and CVE-2025-24855.

To build the harness, first build Jackalope as described in the main readme. Then, from `examples/libxslt` directory, run

```
git clone https://gitlab.gnome.org/GNOME/libxml2.git
git clone https://gitlab.gnome.org/GNOME/libxslt.git
mkdir build
cd build
export CC=clang
cmake ..
cmake --build . --config Release
```

To start the fuzzer, run

```
../../../build/fuzzer -grammar ../grammar.txt -instrumentation sancov -in empty -out out -t 1000 -delivery shmem -iterations 10000 -mute_child -nthreads 6 -- ./harness -m @@
```

You can adjust the `-nthreads` parameter to the number of threads you want to use for fuzzing.

