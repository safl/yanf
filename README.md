# yanf

[![Build Status](https://travis-ci.org/safl/yanf.svg?branch=master)](https://travis-ci.org/safl/yanf)

Yet another Non-volatile memory Filesystem

## Usage

```
yanf -f -o direct_io,big_writes,max_write=131072,max_read=131072 /tmp/yanf
```

## Install

Install liblightnvm and libfuse, then build from source:

```
git clone https://github.com/safl/yanf.git
cd yanf
make all install
```

Or install Ubuntu deb:

```

```
