## Range tracking

When range tracking (exposed using `-track_ranges` flag) is enabled, the fuzzing harness collects information on which parts of the sample are being read and sends this information to Jackalope.
Jackalope then only mutates the part of the sample that are actually read. This can help Jackalope produce more relevant samples, especially when input samples are large by design.

More specifically, range tracking is useful when:

 - The target takes large samples as input but only a part of each sample is consumed (read) by the target
 - Which part(s) of sample are being read is not known in advance and is sample-dependent
 - The fuzzing harness can tell which parts of the samples were read (e.g. the target takes some kind of stream object as an input that can be overloaded)

In order to use the feature, it is necessary to modify the fuzzing harness so that:

 - The harness maps the shared memory object used to share the range information with the fuzzer. The name of the shared memory object is passed to the harness via @@ranges param. For example, if the target commend line is specified as `harness.exe @@ranges <other params>`, the first command line argument passed to the harness will be the name of the ranges shared memory object. The harness is responsible for mapping this shared memory object into its address space.
 - When ever the target reads a sample, the harness should write which part of the sample (from offset, to offset) was read into the shared buffer.
 - The format of the shared memory is as follows. The shared memory buffer is an array of unsigned 32-bit numbers. The first number is the number of ranges in the buffer. After that, the next 2 numbers are the `from` offset and the `to` offset of the first range etc.
 - For each fuzzing iteration, the target needs to reset the range informaton (set the first number in the buffer to zero)
