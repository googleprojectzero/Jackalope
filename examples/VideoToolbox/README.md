## Apple video decoder fuzzing example

In December 2023, Apple fixed 15 vulnerabilities in video decoding modules in macOS Sonoma found using Jackalope. This directory contains an example that demonstrates how these issues were found.

On macOS, video decoding goes through the VideoToolbox module, which then loads decoders for a specific format.

One issue encountered during early testing is that, by default, VideoToolbox creates a separate decoding process, where the decoding actually happens. Thus, a fuzzing harness that just calls video decoding functions won't work well because all the interesting processing will not happen in the harness process. Fortunately, in the VideoToolbox module, a flag called `sVTRunVideoDecodersInProcess` exists, which as the name suggests, causes decoding to take place in the same process. While this flag is not exported, it can also be set by calling the exported function `VTApplyRestrictions` with the argument set to 1. This is what the harness does during initialization.

In order for the fuzzing to be as efficient as possible, after opening the video file, the harness decodes at most two video frames. This is because the two frames could require different routines to decode (e.g. the first frame could be a keyframe and the second frame could be decoded by referencing the first). Naturally, there is no wait time between decoding the frames, regardless of the frame rate / frame times.

Another issue with using VideoToolbox (at least through methods provided by `AVFoundation` framework) is that the `AVAsset` object, used to read the video file, does not provide the ability to read a sample from memory (only from a file or URL). Normally, I always recommend using Jackalope with samples delivered via shared memory, which is both faster and more reliable. See the [README](https://github.com/googleprojectzero/Jackalope/blob/main/README.md) and [ImageIO example](https://github.com/googleprojectzero/Jackalope/tree/main/examples/ImageIO) on how to take advantage of shared memory delivery.

But what to do if the library being fuzzed does not support it, and you don't want to be writing files to disk for every fuzzing iteration? One solution would be to reverse engineer the `AVAsset` class and provide a custom object that exposes the same interface, but reads a sample from memory. A simpler option on macOS, however, is to create a RAM disk and write samples there. You can create and mount a RAM disk with the size of 256MB using the following command.

``diskutil erasevolume HFS+ "RAMDisk" `hdiutil attach -nomount ram://524288` ``

Then, you could point the output directory to RAM disk. However, this approach also has a flaw: if you restart the computer, the contents of the output directory, together with the corpus and collected crashes, will be gone (this happened to us once in the early days of Jackalope. Fortunately, we were able to rediscover a lost crash). Instead, you can use the `-delivery_dir` to store only the current sample on the RAM disk. The corpus, crashes and everything else still gets stored on the normal persistent disk.

In the case of `AVAsset`, `-file_extension` flag is also needed to have the file stored with a specific action, otherwise `AVFoundation` will refuse to process it as a video file.

To run the video decoding fuzzer session
 - Compile Jackalope as described in the main README.
 - Collect some input video file samples for the format you want to fuzz.
 - By running `vtdecode` binary in a debugger, figure out which module(s) get loaded to handle the decoding of the format you want to fuzz. For example, on Intel macs, the decoding of HEVC videos with hardware acceleration is mostly done in `AppleGVA` module. Use the `-instrument_module` flag to collect coverage from this module.
 - From the build directory, run the fuzzer as (example):

`./Release/fuzzer -mute_child -in hevc_in -out hevc_out -t 1000 -delivery_dir /Volumes/RAMDisk -file_extension mov -instrument_module AppleGVA -target_module vtdecode -target_method _fuzz -nargs 1 -iterations 5000 -persist -loop -cmp_coverage -- ./examples/VideoToolbox/Release/vtdecode @@`

Other than the already explained flags, `-in`, `-out` and `-t` are fuzzer input, output and timeout, respectively, `-target_module vtdecode -target_method _fuzz -nargs 1 -iterations 5000 -persist -loop` are used to set up persistent fuzzing mode over `fuzz()` function of the harness and `-cmp_coverage` is used to enable the compare coverage.

Other useful flags:
 - Use `-nthreads <number>` to speeds up your fuzzing session by running on multiple threads in parallel.
 - Use `-mute_child` to suppress command line output from the decoder.
 - Use `-generate_unwind` if you encounter (false positive) crashes related to C++ exceptions.
 - Use `-target_env DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib` to have the target use guard malloc, which makes detecting memory issues easier. Note that using this will cause slowdowns.
