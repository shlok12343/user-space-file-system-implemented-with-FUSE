
- [README.md](README.md) - This README
- [helpers](helpers)     - Helper code implementing access to bitmaps and blocks
- [hints](hints)         - Incomplete bits and pieces that you might want to use as inspiration
- [nufs.c](nufs.c)       - The main C file of the file system driver
- [nufs_cpp.cpp](nufs_cpp.cpp) - C++ version of the file system driver
- [test.pl](test.pl)     - Tests to exercise the file system

## Running the tests

You might need install an additional package to run the provided tests:

```
$ sudo apt-get install libtest-simple-perl
```

Then using `make test` will run the provided tests.

## C++ driver

To build the C++ version of the filesystem driver:

```bash
make nufs_cpp
```

This produces a `nufs_cpp` binary that can be used in place of `nufs`. For example:

```bash
mkdir -p mnt
./nufs_cpp -s -f mnt filesystem-data.bin
```


