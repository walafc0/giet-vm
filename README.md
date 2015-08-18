# The GIET_VM  operating system

The GIET_VM is a static operating system for shared address space, many-cores
architectures. These architectures are generally NUMA (Non Uniform memory
    Acces), because the memory is logically shared, but physically distributed,
  and the main goal of the GIET_VM is to address these NUMA issues.

The GIET_VM assumes that the hardware architecture is structured as a 2D mesh of
clusters. There is one physical memory bank, and several processor cores per
cluster. Each processor is identified by a composite index [x,y,p] where x, y
are the cluster coordinates, and p is the local processor index.

The GIET_VM is written for the MIPS32 processor. The virtual addresses are on 32
bits and use the (unsigned int) type. The physical addresses can have up to 40
bits, and use the (unsigned long long) type.

The GIET_VM supports a paged virtual memory, with two types of pages BPP (Big
    Physical Pages: 2 Mbytes), and SPP (Small Physical Pages: 4 Kbytes). The
physical memory allocation is fully static: all page tables (one page table per
    virtual space) are completely build and initialized in the boot phase.
There is no page fault, and no swap on disk in the GIET_VM.

The GIET_VM supports parallel multi-tasks user applications. A GIET_VM user
application is similar to a POSIX process: one virtual space per application. A
task is similar to a Posix thread: all tasks in a given application share the
same virtual space. Any task can be allocated to any processor, but the
allocation is fully static : no task migration.

When there is more than one task allocated to a processor, the scheduling is
pre-emptive, and uses a periodic TICK interrupt. It implements a round-robin
policy between all runnable tasks.There is one private scheduler for each
processor.

The GIET_VM supports generic architectures: The hardware parameter values (such
    as the number of clusters, or the number of cores per cluster) are defined
in the hard_config.h file.

The GIET-VM configuration parameters (such as the TICK period, or the max number
    of open files) are defined in the giet_config.h file.
