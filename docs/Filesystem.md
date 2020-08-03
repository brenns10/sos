Filesystem
==========

SOS has the absolute beginnings of a filesystem implementation. It is based on
several layers. I think they're pretty interesting and worth documenting, so
here goes.

Block Layer
-----------

The block layer interface is defined in `kernel/blk.h`. A block device is
represented by a `struct blkdev`. When a device is detected, the driver will
create a blkdev object and register it. The block device must implement a few
functions, mainly a `submit()` function which allows requests to be submitted to
the device. The request is represented by a `struct blkreq`. The request can be
a read or write request. Although the API appears to allow blocks of any size,
as it is currently implemented the only size supported is the device's block
size.

Further, this interface is implemented by only one block device (virtio-blk) and
it was designed after only knowing that device. So it's probably not nearly
general enough to support all block devices. But, what can you do?

Once a block request is submitted, the submitter can use the wait queue
functionality to `wait_for()` the result of the request. Since block sizes can
vary, there are some helper functions (`blkreq_wait_all()` and
`blkreq_free_all()` that simplify the process of submitting and waiting for the
result of all requests in a group). In the future, that API could even be
papered over by a nice one which lets users select any blocksize which is a
multiple of the device block size. But for now, this is a decent API.

FS Layer
--------

`kernel/fs.h`

I think this is roughly inspired by what I've seen of Linux's VFS... the FS
layer implements everything which seems to be agnostic of the filesystem, and
as a result it presents a nice interface which the filesystem can implement.

Files and directories are represented in memory by `struct fs_node`. FS nodes
can be a directory, file, or a "lazy directory". All nodes contain names, as
well as information regarding the node's physical location on disk.  Directory
nodes contain a list of all of their children. Lazy directory nodes do not
contain this list, but they contain all the information for the filesystem to
load that list. In this way, the directory structure can be loaded as necessary,
and says in memory once it is used. Unfortunately, I have no mechanism for
cleaning up this cached data (which would be useful under memory pressure, an
issue SOS is nowhere near close to having!). My vague idea is that all "leaf"
directory nodes could be on an LRU list. "leaf" directories have no child
directories, and so they could be freed relatively easily.

Anyway, the underlying FS must implement some operations (`struct fs_ops`). The
"list" one is the operation which expands a lazy directory into a full directory
node (its children of course are not expanded). The open operation returns a
struct file, which also needs to have operations implemented by the FS.

The FS layer has ksh commands which can be used to look at file systems once
they are mounted:

    ksh> fs ls /  # list root
    ksh> fs cat /FILE1  # print file

Multiple FS instances is not something I really support yet, we'll see how that
API ends up panning out.

Each FS must register itself into the FS layer with a mounted device. This could
be done by detecting the FS type from the first block, but I don't do that. I
just have a command for mounting my FAT filesystem (see below).

FAT Layer
---------

FAT is a filesystem (and the only one implemented so far). See the readme for
a relevant specification document. It implements list, open, as well as read. To
use the FAT implementation, you need a FAT disk image. The best way to do that
is to generate one using mtools. mtools is a bit of a difficult system to learn.
Here is an example (taken from integration tests):

    dd if=/dev/zero of=fatdisk bs=1M count=4

    # Create a file (all caps name) and add to the image in root directory.
    echo "contents" >FILE1
    mcopy -i fatdisk FILE1 ::/FILE1

    # Create directory in the image
    mmd -i fatdisk ::/MYDIR

To use this, copy the disk image you've created to the `mydisk` file in the root
of the repository. The disk will be used as the virtio-blk device, and you can
then use commands like this to mount it and then explore it:

    ush> exit  # exit ush, we want ksh
    ksh> fat init vblk79  # make sure to use correct device name

Some premade disk images exist in `fat` directory.
