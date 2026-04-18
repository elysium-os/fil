# Freestanding Inflate Library

Minimal inflate library with zero dependencies, designed for freestanding environments.

Currently support GZIP/Deflate implemented according to [RFC1952](https://datatracker.ietf.org/doc/html/rfc1952) and [RFC1951](https://datatracker.ietf.org/doc/html/rfc1951).

## Building

FIL is built with [fabricate](https://github.com/elysium-os/fabricate).

```sh
fabricate setup
# Or for a release version
# fabricate setup -o buildtype=release

fabricate build
```

## API

The api is documented in the header file located [here](include/fil.h).
