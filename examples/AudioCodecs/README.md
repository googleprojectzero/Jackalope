## Apple audio decoder fuzzing example

In January 2025, Apple fixed 3 vulnerabilities in audio decoding modules in macOS Sonoma found using Jackalope. 

```
CoreAudio
Available for: macOS Sequoia

Impact: Parsing a file may lead to an unexpected app termination
Description: The issue was addressed with improved checks.

CVE-2025-24160: Google Threat Analysis Group
CVE-2025-24161: Google Threat Analysis Group
CVE-2025-24163: Google Threat Analysis Group
```

This directory contains an example that demonstrates how these issues were found. On macOS, most of the audio decoding goes through the AudioCodecs module, which then might load other decoders for a specific format. In order for the fuzzing to be as efficient as possible, after opening the file, the harness decodes all audio tracks. This is because tracks can use different encoders and often times interesting tracks (e.g. Apple Positional Audio Codec) are encoded at the end of the file.

Since the audio API requires files to writen on disk with the proper extension (`-file_extension` needed), you need to create a RAM disk and write samples there using the following command for example.

``diskutil erasevolume HFS+ "RAMDisk" `hdiutil attach -nomount ram://524288` ``

Then, you could point the output directory to RAM disk.

To run the audio decoding fuzzer session
 - Compile Jackalope as described in the main README.
 - Collect some input video or audio file samples for the format you want to fuzz.
 - Compile `audiodecode` using `clang -o audiodecode audiodecode.m -framework AVFoundation -framework AudioToolbox -framework Foundation -framework CoreMedia`
 - By running `audiodecode` binary in a debugger, figure out which module(s) get loaded to handle the decoding of the format you want to fuzz. For example, the decoding of APAC audio with hardware acceleration is done in `AudioCodecs` module. Use the `-instrument_module` flag to collect coverage from this module.
 - From the build directory, run the fuzzer as (example):

`./Release/fuzzer -mute_child -in in/ -out out/ -nthreads 4 -t 4000 -delivery_dir /Volumes/RAMDisk -file_extension mov -instrument_module AudioCodecs -target_module audiodecode -target_method _fuzz -nargs 1 -iterations 5000 -persist -loop -cmp_coverage -generate_unwind -max_sample_size 2000000  -- ./audiodecode @@`

Other than the already explained flags, `-in`, `-out` and `-t` are fuzzer input, output and timeout, respectively, `-target_module audiodecode -target_method _fuzz -nargs 1 -iterations 5000 -persist -loop` are used to set up persistent fuzzing mode over `fuzz()` function of the harness and `-cmp_coverage` is used to enable the compare coverage.

Other useful flags:
 - Use `-nthreads <number>` to speeds up your fuzzing session by running on multiple threads in parallel.
 - Use `-mute_child` to suppress command line output from the decoder.
 - Use `-target_env DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib` to have the target use guard malloc, which makes detecting memory issues easier. Note that using this will cause slowdowns.
