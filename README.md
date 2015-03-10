Intro
=====
r-instrumented is a modified R interpreter measuring a variety of
values during the execution of an R program. It is based on a
modification of the same name for R 2.12.1 from the Reactor project at
Purdue university: http://r.cs.purdue.edu/hg/r-instrumented/
FIXME: Mention debug scopes


Compiling r-instrumented
========================
The system requirements for compiling r-instrumented are the same as
for the standard R interpreter. Compilation under anything but 64-bit
x86 Linux has not been tested.

Compiling r-instrumented works the same way as compiling an unmodified
R interpreter (see the file INSTALL for details).
FIXME: Mention debug scopes


Using r-instrumented
====================
r-instrumented adds several command line options to the R interpreter
which can be used when running R or Rscript. If you use r-instrumented
to take measurements, it is recommended to use it only in
non-interactive mode, as the measurements are only written to the
output file at the end of the session.

The results of a run are always written to files in a
directory whose name is specified using the
`--tracedir` command line option. At minimum, r-instrumented will
gather data about the R program that was executed in a file called
`trace_summary` in the specified directory.

In addition to the main data collection facility, r-instrumented can
optionally capture data about every call to native code using the
`--trace-externalcalls` option. If this option is used, you also need
to use `--tracedir` to specify an output directory. A file called
`external_calls.txt.gz` will be created in the output directory
containing information about calls to native code, for more
information see below.

Sample command line:

    ./bin/Rscript --tracedir trace --trace all -f ../something.R

FIXME: Debug scopes


Format of trace_summary
=======================
The trace_summary file is a text file that should be importable in many
spreadsheet applications like LibreOffice Calc as a tab-separated CSV
file. Each line has a "keyword" in its first column followed by
tab-separated data values. Additionally a few lines starting with "#"
are embedded in the file to provide additional information about the
data when it is imported by traceR or when the file is manually
inspected - the fields in the "#LABEL" lines are lined up with the data
columns they correspond to. Since some of the keywords can be rather
long, it is recommended to increase the tab size to a suitable value
when viewing the output file in a text editor - 30 characters per tab
seems to be a good starting point.

The following lists all keywords that r-instrumented can currently
show in its output file. Please note that the descriptions of the
values assume a strong familiarity with the inner workings of the R
interpreter, reading at least the "Writing R Extensions" and "R
internals" documentation of the R interpreter is strongly recommended.
If in doubt, check the source code too.

- TraceDir

    This keyword lists the directory given to the `--tracedir` command
    line switch given to the R interpreter.

- Workdir

    This keyword lists the current directory at the time of the R
    invocation.

- Args

    This keyword lists the arguments given to the R program, separated
    by spaces. No special handling for arguments that contain spaces
    themselves is done.

- TraceDate

    This keyword lists the date and time when the output file was
    written, which may differ significantly from the start time if a
    long-running script was profiled. The format of the value is
    defined by the asctime C library function and is a string of the
    form "Wed Jun 30 21:49:08 1993".

- Rusage*

    The keywords starting with "Rusage" keywords show results from a
    call to the getrusage C function at the time the output file is
    written, which gives a record of some resource usage values of the
    R interpreter recorded by the operating system. Not every value is
    available on every operating system, for example Linux just
    returns zero for some of these values. Since r-instrumented is
    developed on Linux, the following descriptions are based on its
    usage of the rusage values. If you don't use Linux, check the
    documentation of your operating system (e.g. "man getrusage") for
    more details of values that are not explained here. Unused values
    are shown as 0.

- RusageMaxResidentMemorySet

    Value of ru_maxrss, the maximum resident set size in KiBytes. This
    is basically the maximum amount of RAM given to the interpreter by
    the operating system while it was running.

- RusageSharedMemSize

    Value of ru_ixrss, the integral shared memory size. This value is
    not used on Linux.

- RusageUnsharedDataSize

    Value of ru_idrss, the integral unshared data size. This value is
    not used on Linux.

- RusagePageReclaims

    Value of ru_minflt, the number of page faults in the R interpreter
    that could be serviced without requiring any I/O activity.
    (also known as "minor faults", the current keyword is kept for
    historical reasons)

- RusagePageFaults

    Value of ru_majflt, the number of page faults in the R interpreter
    that required I/O activity to service, e.g. reading from a
    memory-mapped file.
    (also known as "major faults", the current keyword is kept for
    historical reasons)

- RusageSwaps

    Value of ru_nswap, a field commented just as "swaps" in the Linux
    getrusage man page. This value is not used on Linux.

- RusageBlockInputOps

    Value of ru_inblock, the number of times the file system had to
    perform input, e.g. when a file is read.

- RusageBlockOutputOps

    Value of ru_outblock, the number of times the file system had to
    perform output, e.g. when a file is written.

- RusageIPCSends

    Value of ru_msgsnd, the number of IPC messages sent. This value is
    not used on Linux.

- RusageIPCRecv

    Value of ru_msgrcv, the number of IPC messages received. This
    value is not used on Linux.

- RusageSignalsRcvd

    Value of ru_nsignals, the number of signals received by the R
    interpreter process. This value is not used on Linux.

- RusageVolnContextSwitches

    Value of ru_nvcsw, the number of voluntary context switches. This
    value counts the number of times the R interpreter voluntarily
    gave up the CPU before its time slice ended, e.g. to wait for an
    I/O operation.

- RusageInvolnContextSwitches

    Value of ru_nivcsw, the number of involuntary context
    switches. This value counts the number of times the R interpreter
    was forced to give up the CPU by the operating system,
    e.g. because its time slice was exceeded or a higher-priority
    process became runnable.

- PtrSize

    This keyword shows the size of a void * pointer used by the R
    interpreter in bytes. This should usually be 4 for a 32-bit
    interpreter and 8 for a 64-bit interpreter.

- StructSize

    This keyword lists the sizes of two structures from R's object
    management. The first value is the size of the SEXPREC data
    structure, the second value is the size of the SEXPREC_ALIGN data
    structure. Both values are in byte. A 64-bit interpreter usually
    reports 56 and 40 here.


The Allocated* values calculated by r-instrumented only measure memory
allocations, completely ignoring freed memory. This means that they
are all represent the total amount of a resource allocated during the
execution of an R program and as such often exceed the actual amount
of RAM used by the interpreter by a large factor -- the size of this
factor depends on the memory usage of the R program and the efficiency
of the garbage collector.

- AllocatedCons

    This keyword lists the total number of bytes allocated for cons
    cells using the cons() C function in the R interpreter. To get the
    number of cons cells, divide this by the size of SEXPREC given in
    the first StructSize value.

- AllocatedConsPeak

    This keyword lists the maximum number of cons cells allocated at
    the same time in the R interpreter. This value is slightly
    different from the maximum number of cons cells in use at one time
    as deallocation happens only when the garbage collector is run.

    Note: A removal of this keyword is under consideration, as is a
    change to bytes instead of number of allocations.

- AllocatedNonCons

    This keyword lists the total number of bytes allocated by the
    allocS4Object C function in the R interpreter (the current name is
    an artifact caused by its implementation). To get the number of
    allocS4Object calls, divide this by the size of SEXPREC given in
    the first StructSize value.

- AllocatedEnv

    This keyword lists the total number of bytes allocated for
    environments using the NewEnvironment C function in the R
    interpreter. Please note that this value only counts the single
    class-0 allocation for the ENVSXP, not the size of the data stored
    in this environment. To get the number of NewEnvironment calls,
    divide the value by the size of SEXPREC given in the first
    StructSize value.

- AllocatedPromises

    This keyword lists the total number of bytes allocated for
    promises using the mkPROMISE C function in the R
    interpreter. Please note that this value only counts the single
    class-0 allocation for the PROMSXP and not the size of the
    expression contained in the promise or the size of its value after
    evaluation. To get the number of mkPROMISE calls, divide the value
    by the size of SEXPREC given in the first StructSize value.

- AllocatedSXP

    This keyword lists the total number of bytes allocated to SEXPRECs
    using the allocSExp C function in the R interpreter. Please note
    that this value only counts the single class-0 allocation for the
    S-Expression and not the size of anything referenced by it. To get
    the number of allocSExp calls, divide the value by the size of
    SEXPREC given in the first StructSize value.

- AllocatedExternal

    This keyword lists the total number of bytes allocated by the
    R_alloc function which returns a block of memory similar to the C
    malloc function, but which is tracking in the garbage collection
    system of the R interpreter. Although the keyword is called
    "external", many calls to the allocation function can be found in
    libraries included with the R interpreter as well as the mein
    interpreter code itself. Currently the number of allocations are
    not tracked and since these allocations do not use any fixed size,
    manual conversion is not possible.

- AllocatedList

    This keyword lists two values for pairlist allocations, the first
    one being the total number of list allocations and the second one
    the total number of elements allocated in pairlists. These values
    only cover pairlists created using the allocList C function in the
    R interpreter, but it is also possible to create a pairlist
    directly from cons cells allocated using the cons() function. One
    example where is happens is during duplication of a LISTSXP in
    duplicate.c:duplicate1() - to avoid reading the list twice, the
    code there duplicates a list by copying each element by itself and
    linking them together instead of calling allocList().

    To convert from the number of elements to a total size in bytes,
    multiply the second value of this keyword by the size of SEXPREC
    given in the first StructSize value.

- AllocatedStringBuffer

    This keyword lists three values for string buffer allocations. The
    first is the total number of string buffer allocations, the second
    the number of characters (including terminating 0) allocated in
    string buffers and the third one the number of bytes actually
    allocated. The difference between the second and third values
    stems from the fact that R rounds up the allocation to the next
    multiple of a certain block size (which can be different for each
    string buffer).

- AllocatedVectors

    This keyword lists four values for vector allocations, calculated
    over all vector allocations. The same values are also available
    for four classes of vectors based on the vector's size as
    AllocatedZeroVectors, AllocatedOneVectors, AllocatedSmallVectors
    and AllocatedLargeVectors as well as an even more detailed
    histogram by the number of elements in the vector (see below).

    The first value is the total number of vector allocations. The
    second is total number of vector elements that are
    allocated. Values three and four are the total sizes in byte of
    the allocated vectors. Value three is just the number of bytes
    required to hold the number of allocated elements, value four is
    the actual number of bytes allocated -- R internally rounds up all
    allocations to certain boundaries as this increases the efficiency
    of the memory management. All vectors also have a header block
    that specifies their type and holds additional data required for
    memory management, but the size of this header is NOT included in
    these figures. For vectors of length zero, the header is one
    SEXPREC; for vectors with non-zero size the header is one
    SEXPREC_ALIGN. The size of both is shown in the StructSize
    keyword.

A note about vector lengths: In the current implementation of the R
interpreter, the length of a vector is set at allocation time and can
not be changed. If an R program does change the length of a vector,
the interpreter creates a copy with the new length and the old
instance may be discarded by garbage collection if there are no more
references to it.

- AllocatedZeroVectors

    Same as AllocatedVectors, but only for vectors with length
    zero. Such vectors can be explicitly created (e.g. `numeric(0)`),
    but they can also result from operations like subsetting with an
    empty result.

- AllocatedOneVectors

    Same as AllocatedVectors, but only for vectors with length
    one.

- AllocatedSmallVectors

    Same as AllocatedVectors, but only for vectors with a length
    greater than one, but still small enough that the R interpreter
    considers them to be "small". Currently this is the case when the
    data of the vector occupies 128 bytes or less which corresponds to
    8 complex, 16 double or 32 integer/logical elements. For vectors
    within this limit (as well as zero/one-element vectors) the R
    interpreter uses a set of storage pools to avoid calling the
    operating system's memory allocation function for every single
    vector.

- AllocatedLargeVectors

    Same as AllocatedVectors, but only for vectors with a length that
    the R interpreter considers to be large. Currently this is the
    case when the data of the vector occupies more than 128 bytes. For
    these vectors, R requests a memory block from the operating
    system's memory allocation function individually for each vector.

- Vector allocation size histogram (VectorAllocExactLimit and VectorAllocBin)

    To provide a more detailed insight about the use of various vector
    lengths by the R program, r-instrumented also generates a
    histogram of (allocation-time) vector lengths. Since it would be
    infeasible to record the number of vectors for each possible
    vector length from 0 to 2^32, the lengths are grouped into a
    smaller set of bins. For vectors with a small number of elements,
    up to (and including) the value given by the VectorAllocExactLimit
    keyword (15 by default), the bin size is 1. For vectors with more
    elements, the bins are sized in powers of two.

    The VectorAllocBin keyword occurs multiple times and has seven
    values. The first value is the number of the bin specified by the
    current line. The second and third values specify the lower and
    upper bounds for the number of elements in a vector that can be
    counted in this bin -- so both values are identical up to the
    limit given by VectorAllocExactLimit and then follow a [2^n,
    2^(n+1)-1] pattern. The fourth to seventh value are the total
    number of allocations, number of elements, size of elements and
    allocation size for the line's range of vector sizes, similar to
    the Allocated*Vectors keywords. All bins from bin number 0 to the
    highest bin number with non-zero allocation count are given, even if
    some bins inbetween had zero allocations.

- GC_count

    This keyword lists the number of times the garbage collector was
    run.

- Promises

    This keyword lists three values related to promises. The first one
    is the number of promises created during the execution of the R
    program, the second one is the number of promises that were
    garbage-collected when the interpreter is about to exit. Both
    values should always be identical, otherwise there is a bug in
    r-instrumented. The third value is the number of promises which
    did not have a value set when they were garbage-collected,
    i.e. which were never evaluated.

- PromiseSetval

    This keyword lists five values which show the relation between the
    function call depth at the time a promise was created compared to
    the time it was evaluated. r-instrumented currently considers only
    the call to a function defined in R code or to an R builtin as
    function call. Due to limitations of the size of the data
    structures involved, promises at function call depths beyond 65534
    are ignored.

    The first three values of this keyword show the number of promises
    that were evaluated at the same, a lower or a higher function call
    depth compared to the creation of the promise. Due to the way
    promises are created, calling a function with a parameter that is
    used directly in this function is seen as a level difference of +1
    by r-instrumented, so for PromiseSetval it would be reported as
    "higher".

    The fourth value of PromiseSetval records the number of times a
    promise could not be checked because the function call depth was
    larger than than 65534; the fifth value counts the number of times
    a promise was assigned a value even though it already had one from
    a previous assignment. If the latter case happens, no further data
    is recorded for this particular promise value assignment.

    Note: Recording the maximum function call depth ever seen in the
    execution of an R program is planned

- PromiseMaxDiff

    This keyword lists the maximum difference in function call levels
    when a promise is assigned compared to its creation time. The
    first value is for promises that were evaluated at a lower
    function call depth than they were created, the second value is
    for promises that are evaluated at a higher call depth
    (e.g. created in a function and evaluated in a function called
    from there).

- PromiseLevelDifference

    This keyword can appear multiple times in the output. Taken
    together, their values provide a histogram about the function call
    depth difference between the creation and the evaluation of
    promises. The first value for this keyword is the depth
    difference, the second value is the number of times a promise was
    assigned a value while it had exactly this level difference. The
    size of the histogram can be changed in the source code in
    `src/include/trace.h` in the definitions of
    `TRACER_PROMISE_LOWER_LIMIT` and `TRACER_PROMISE_HIGHER_LIMIT`. If
    the difference exceeds these limits, it is counted as the maximum
    value instead.

- Duplicate

    This keyword lists three values related to the `duplicate()`
    function within the R interpreter, which is the main function used
    to duplicate values (but not the only way duplications are made in
    the R interpreter). The first value is the number of R interpreter
    objects that were duplicated by a call to duplicate. The second
    and third values count the number of elements that were
    duplicated. The difference between this and the first value is
    that for example an object with attributes is counted only as 1
    in the first value, but as 1 for the object and 1 for each
    duplicated attribute in the second/third value. The difference
    between the second and third value is that the second one counts
    each single element for vectors while the third counts vectors as
    1 regardless of their length.

- MallocmeasureQuantum

    This keyword specifies the time quantum used for the values
    specified in the PeakMemory values. It should usually be 1, unless
    the program you ran via r-instrumented took longer than a day to
    complete. To get the actual time in seconds of a PeakMemory value,
    multiply its time value with the value of MallocmeasureQuantum.

    This keyword is currently only present if r-instrumented is
    compiled for Linux. In case you want to port it: The code needs
    the RTLD_NEXT parameter for dlsym() and the malloc_usable_size()
    function (GNU extension). Patches are welcome.

- PeakMemory

    The PeakMemory keyword can appear multiple times in the
    output. Taken together, all Peakmemory keywords provide a time
    series of the peak memory allocation of the R interpreter.

    The measurements are taken in regular intervals by calculating
    the peak amount of memory that the interpreter had allocated
    during this interval. Only allocations via the malloc() family of
    library functions are considered, but ignoring the C stack this is
    currently the only source of memory allocations in the core R
    interpreter. Since the measurements are made by providing
    alternative versions of the library functions, they should also
    cover any native code loaded by packages.

    Each PeakMemory keyword has two values. The first one is the time
    index, the second one the peak memory (in bytes) seen in the
    interval starting at this time index. Initially each time index
    corresponds to one second, but if the runtime of the R program
    exceeds 86400 time indices (one day at one-second resolution), the
    interval per time index will be doubled. The final interval size
    is given in the MallocmeasureQuantum keyword.

- ArgCount

    The ArgCount keyword can appear multiple times in the
    output. Taken together, all ArgCount keywords provide multiple
    histograms about the usage of argument passing styles in R function
    calls in the R program. The first value (_count_) is the number of
    arguments
    the current ArgCount line refers to. The second value (_calls_) is
    the number of calls where exactly that number of arguments were
    passed to the called function. For most values of this keyword,
    the count is based on the actual number of arguments specified in
    the function call, i.e. default arguments that are not given in
    the function call are not counted.

    The third to fifth values of this keyword (*by_position*,
    *by_keyword* and *by_dots*) are the total number of arguments
    passed by position, by keyword or by `...` for calls with _count_
    total arguments given. Consider the following examples:

        test <- function (a, b, ..., c=NA) {}

        # counted in ArgCount with count = 3 as
        #   by_position = 2, by_keyword = 0, by_dots = 1
        test(1, 2, 3)

        # counted in ArgCount with count = 3 as
        #   by_position = 1, by_keyword = 2, by_dots = 0
        test(1, b=2, c=99)

        # counted in ArgCount with count = 5 as
        #   by_position = 2, by_keyword = 0, by_dots = 3
        test(1, 2, 3, 4, 5)

    The sixth to eighth values of this keyword (*npos_calls*,
    *nkey_calls*, *ndots_calls*) use a different interpretation of the
    _count_ value: They record the number of calls that have _count_
    positional, keyword or dot arguments. For the above example, the
    first call to `test` would increment npos_calls by one for count
    2, nkey_calls for count 0 and ndot_calls for count 1. The second
    would increment npos_calls by one for count 1, nkey_calls for
    count 2 and ndot_calls for count 0. The third example would
    increment npos_calls by one for count 2, nkey_calls for count 0
    and ndots_calls for count 3. An easy way to experiment with this
    is to wrap a test call to an empty function in a `for (i in
    1:1000000)` loop to ensure that the tested situation stands out
    against the background noise of R's initialisation.

    The number of ArgCount lines in the output file varies based on
    the maximum number of arguments seen during the execution of the R
    program. All lines between zero and this maximum will appear in
    the output, even if there were no calls with this number of
    arguments.



Format of external_calls.txt.gz
===============================
The external_calls.txt.gz file is a gzip-compressed text file which
is only written if r-instrumented is run with the
`--trace-externalcalls` option. It contains one line of text per call
to an external function (via `.Call`, `.External`, `.C` and
`.Fortran`). Each line has three space-separated item. The first one
is the symbol type (`NativeSymbolType` from Rdynload.h), which
actually specifies the R method that was used to call it. Type 1 is
.C, type 2 .Call, type 3 .Fortran and type 4 .External.

The second item is the name of the function that is called. The third
item is the address of the function written as hex (including 0x
prefix) which may vary between multiple runs of the interpreter, but
can be useful as an identification value of the function as it should
be unique for each function in a single run.

Please note that R also uses these external function call interfaces
to call functions from its own base packages, especially in the
_methods_ package.

Unfortunately we have not yet found a reliable way to determine the
name of the dynamic library where the function symbol was imported
from. If we add this, the format of this file will change slightly.
