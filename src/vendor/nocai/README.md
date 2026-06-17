# Vendor Direct-Print Adapter

This folder contains the source adapter only.

Proprietary SDK drops, demo packages, shared libraries, generated binaries, and local test copies must remain local and ignored by git. Do not commit vendor SDK files here.

Set `DIRECT_PRINT_SDK_ROOT` to the local vendor SDK path before configuring or building when direct-print SDK packaging is needed:

```bash
export DIRECT_PRINT_SDK_ROOT=/path/to/local/vendor/sdk/drop
```

The C++ adapter sources in this directory are part of the application and should remain tracked.
