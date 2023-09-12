## ImageIO fuzzing target example

This directory contains an example harness for fuzzing image parsers on macOS.

Build Jackalope as explained in the main README. To start a fuzzing session, run

`./fuzzer -in in -out out -t 100 -delivery shmem -instrument_module ImageIO -target_module test_imageio -target_method _fuzz -nargs 1 -iterations 1000 -persist -loop -cmp_coverage -generate_unwind -- ../examples/ImageIO/Release/test_imageio -m @@`

Explanation of the flags
 - `-in` and `-out` are the input corpus directory and the output directory.
 - `-t` is the timeout in milliseconds.
 - `-delivery shmem` means we are passing a sample to the target over shared memory.
 - `-instrument_module ImageIO` means we will be collecting coverage from the `ImageIO` module. This can be replaced according to the format being fuzzed. More on that later.
 - `-target_module test_imageio -target_method _fuzz -nargs 1 -iterations 1000 -persist -loop` flags configure Jackalope's persistent fuzzing mode. In combination, these flags mean that Jackalope is going to run the function `fuzz` in module `test_imageio` in a loop for every fuzzing iteration without restarting the process. The process is going to be periodically restarted after 1000 iteration (if not sooner due to other events).
 - `-cmp_coverage` enables TinyInst's cmp coverage, which enables the fuzzer to bruteforce through multibyte comparisons.
 - `-generate_unwind` is needed if the target throws C++ exceptions (which is going to be the case here).
 
 `test_imageio -m @@` is the command line of the target. As you can see in the source code of the imageio harness, `-m` means the harness is going to read the sample from shared memory whose name is in the next parameter. `@@` is the special parameter which Jackalope is going to replace with the name of the shared memory object.
 
If you want to test a sample from a file (e.g. if you find a crash), you can invoke the target as `test_imageio -f <filename>`

Parsers for different image formats are implemented in different modules. The example above uses `-instrument_module ImageIO` and ImageIO indeed implements some image format parsers, but you should select a format, find out which module handles its parsing and use that instead. To find out which modules are loaded for a particular input file, you can run

`../TinyInst/Release/litecov -trace_debug_events -- ../examples/ImageIO/Release/test_imageio -f <filename>`

which will print the module names when they get loaded.

Other useful flags:
 - `-nthreads <number>` speeds up your fuzzing session by running on multiple threads in parallel.
 - `-mem_limit <megabytes>`. Sometimes, fuzzing image files produces images with large dimensions that require a lot of memory and cause slowdowns. You can avoid saving those to the fuzzing corpus by setting a memory limit for the target process.
