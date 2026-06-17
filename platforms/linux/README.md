# Ori Linux Platform

This folder contains the Linux bundle host files for Ori apps.

`ori build <project>` for `platform: linux` emits:

```text
build/linux/<name>/
  app.orb
  orivm
  run.sh
```

When built on Linux, `orivm` is the native VM from the current toolchain. When
packaged from another host, `core/orivm` must already be a Linux binary.
