# Jackalope

```
Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## What is Jackalope

Jackalope is a customizable, distributed, coverage-guided fuzzer that is able to work with black-box binaries.

### Why another fuzzer?

While there are a lot of good coverage-guided fuzzers that work on targets where source code is available, there are relatively few that work on black box binaries, in particular on Windows and macOS operating systems, and those that do exist are mainly based on codebases that aren't very easy to customize. The initial goals of Jackalope are:
 - Easy to customize for targets where generic fuzzers might not work well. This might include
   - Custom mutators
   - Custom sample delivery mechanizms
   - Custom instrumentation, etc.
 - Easy to paralellize, both on a single machine and across multiple machines

### What does it do?

Jackalope can be used stand-alone, but is more powerful when used as a library, where users can plug in custom components that would replace the default behavior. By default, Jackalope ships with:
 - Binary instrumentation using TinyInst
 - A simple set of generic mutators
 - Sample delivery via file or via shared memory

Jackalope can be run in parallel
 - On a single machine: by passing the number of fuzzing threads via `-nthreads` command line parameter
 - Across multiple machines: By running one instance as a server (`-start_server` command line flag) and having fuzzers on the worker machines connect to this server (`-server` command line flag). The server then collects and distributes samples, crashes and coverage across the workers.

### What does it not do?

Jackalope does not currently include advanced mutation strategies. Instead it ships with a set of generic mutators, which will work for many targets, however the users are encouraged to write custom mutators and mutation strategies according to the targets they want to fuzz.

### Which platforms are supported?

Currently, Windows and macOS.

## Building Jackalope

1. Open a terminal and set up your build environment if needed. On Windows, instead of opening a generic command prompt, you'll want to open Visual Studio command prompt for the platform you are building for, or run `vcvars64.bat` / `vcvars32.bat`.

2. Navigate to the directory containing the source.

3. Run the following commands:

```
git clone --recurse-submodules git@github.com:googleprojectzero/TinyInst.git
(alternately: git clone --recurse-submodules https://github.com/googleprojectzero/TinyInst.git)
mkdir build
cd build
cmake <generator arguments> ..
cmake --build . --config Release
```

The generator arguments depend on your environmaent. On macOS you'd want to use `-G Xcode`, while for example on Windows with Visual Studio 2019 and for 64-bit build you would use `-G "Visual Studio 16 2019" -A x64`.

Getting `No CMAKE_C_COMPILER could be found` error on macOS? Try updating cmake. Also make sure Xcode is installed and you ran it at least once (it installs some components on 1st run).

## Running Jackalope

Usage:

`./fuzzer <fuzzer arguments> <instrumentation and other components arguments> -- <target command line>`

The following command line arguments are supported:

`-in` - Input directory (directory containing initial sample set). If input directory is "-", the fuzzer attempts to restore the previous session (same as using the `-restore` flag).

`-out` - Output directory

`-t` - Sample timeoutin ms

`-t1` - Timeout for target initialization (e.g. before reaching the target method if defined). Defaults to sample timeout.

`-nthreads` - Number of fuzzer threads. Default is 1.

`-delivery <file|shmem>` - Sample delivery mechanism to use. If `file`, each sample is output as file and "@@" in the target arguments is replaced with a path to the file. If `shmem`, the fuzzer creates shared memory instead and replaces "@@" in the target arguments with the name of the shared memory. It is the target's responsibility to open the shared memory and extract the sample in this case. Default is `file`.

`-restore` or `-resume` - Restores and resumes a previous fuzzing session. Both fuzzer and server process support restoring.

`-server` - Specifies the coverage server to use.

`-start_server` - Run a server process instead of fuzzing process.

For TinyInst instrumentation command line arguments, refer to [TinyInst readme](https://github.com/googleprojectzero/TinyInst).

Example (macOS):

```
./fuzzer -in in -out out -t 1000 -delivery shmem -instrument_module test -target_module test -target_method __Z4fuzzPc -nargs 1 -iterations 10000 -persist -loop -cmp_coverage -- ./test -m @@
```

Example (Windows):

```
fuzzer.exe -in in -out out -t 1000 -delivery shmem -instrument_module test.exe -target_module test.exe -target_method fuzz -nargs 1 -iterations 10000 -persist -loop -cmp_coverage -- test.exe -m @@
```

Explanation: This runs the fuzzer using "in" as input directory and "out" as output directory. Samples are delivered via shared memory without writing to disk (`-delivery shmem`). Coverage is collected from the `test` / `test.exe` module (`-instrument_module` flag). The target is run in persistent mode with function `fuzz()` from `test` / `test.exe` module being run in a loop. This function takes 1 argument and will be run in the loop for maximum of 10000 iterations before restarting the target process. Compare coverage is used (`-cmp_coverage` flag) in order to bruteforce through multi-byte comparisons easily. `test.exe -m @@` is the target command line where @@ gets replaced with the shared memory name (it would be replaced with the filename if `-delivery shmem` wasn't used).

## Architecture

Jackalope consists of the following main classes:

`Fuzzer` - The 'main' class that handles most of the high-level tasks such as keeping track of corpus and coverage, scheduling jobs to threads, communicating with the server (if present). The Fuzzer class exposes several virtual methods that can be used to modify it's behavior. Users can create custom fuzzers by subclassinng the `Fuzzer` class and overloading these methods.

`Sample` - A simple class for storing sample data (bytes).

`Mutator` -  Handles mutations. The main job of the Mutator class is to implement the `Mutate()` method which modifies a sample. However, mutators can also be more complex and, for example, define additional context for each sample that will be passed during `Mutate()` calls. Mutators can also be "meta-mutators" which combine other mutators in different ways. See `mutator.h` for the built-in mutators. When fuzzer selects an input sample, it will fuzz it for a certain number of "rounds", before moving on to the next sample, and a mutator can control the number of rounds. Specifically, the fuzzer will continue with the same input sample until the top-level mutator returns `false` from its `Mutate()` method.

`Instrumentation` - Handles running of target and collecting coverage. The fuzzer comes with an implementation of Instrumentation which uses [TinyInst](https://github.com/googleprojectzero/TinyInst)

`SampleDelivery` - Handles passing a sample to the target. The fuzzer comes bundled with sample delivery via file and via shared memory.

`PRNG` - Pseudorandom number generator. By defauly, Jackalope uses PRNG based on Mersenne twister.

`Server` - Implements server components of the fuzzr. The server is responsible for collecting coverage from clients as well as samples that trigger new coverage. The server then distributes those samples to other fuzzer processes.

`Client` - Implements methods for communicating with the server.

## Customizing the fuzzer

The "intended" way to customize the fuzzer is to subclass the `Fuzzer` class and override the relevant methods. See [main.cpp](https://github.com/googleprojectzero/Jackalope/blob/main/main.cpp) for a simple example. The methods that can be overriden are:

`CreateMutator()` - Creates a mutator configuration for the fuzzer. For an example, see https://github.com/googleprojectzero/Jackalope/blob/main/main.cpp#L25

`OutputFilter()` - Can modify a sample before passing it to the target, for example to fix the header or checksums. The default implementation will pass the sample as is.

`AdjustSamplePriority()` - The fuzzer maintains a queue of samples to fuzz sorted by priority. This method can be used to adjust the priority of a sample after each run. The default implementation decreases the priority of the sample for each run that did not result in new coverage. If a run resulted in the new coverage, the priority of the sample is reset. This ensures that the fuzzer will spend more time fuzzing the samples that produce new coverage when mutated.

`CreateSampleDelivery()` - Can be used to define custom mechanisms for delivering the sample to the target. An example could be sending the sample over network or IPC.

`CreateInstrumentation()` - Can be overriden in order for the fuzzer to use custom instrumentation.

`CreatePRNG()` - Can be overriden in order to use custom PRNG.

## Disclaimer

This is not an official Google product.
