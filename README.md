## bsd\_kext\_log

`bsd_kext_log` is a macOS kernel extension used for experimental kext logging propagation from kernel into user space.

### Rationale

One can use [log(1)](x-man-page://1/log)/[syslog(1)](x-man-page://1/syslog) to capture log from kernel, just as [kextlogd](https://github.com/lynnlx/kextlogd) do.  Yet due to kext cache mechanism, new logging system [log(1)](x-man-page://1/log) may capture nothing while the kext was indeed issued a log via [printf](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/osfmk/kern/printf.c#853) or [IOLog](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/iokit/Kernel/IOLib.cpp#1152).

Thusly this kernel extension tries to fix this flaw by introducing a simple in-kernel logging mechanism, which it uses [kernel control socket](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/NKEConceptual/control/control.html) to communicate with user space log-daemon client, and pushing messages to it(daemon).

So user space daemon can do bottom-half work like print the log, persist into disk, etc.

This logging technique inspired by Microsoft's [VFS for Git](https://github.com/Microsoft/VFSForGit)(ProjFS.Mac), yet they use [IOSharedDataQueue](http://xr.anadoxin.org/source/xref/macos-10.13.6-highsierra/xnu-4570.71.2/iokit/IOKit/IOSharedDataQueue.h#55) for messaging propagation, which is part of [`IOKit` environment](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/Architecture/Architecture.html#//apple_ref/doc/uid/TP30000905-CH1g-TPXREF102), unavailable for BSD portion.

### Build

You must install Apple's [Command Line Tools](https://developer.apple.com/download/more) as a minimal build environment, or Xcode as a full build environment in App Store.

#### Kernel extension

```shell
# cd into repository home
cd bsd_kext_log

# Default target is debug
make [release]
```

#### User space log daemon

```shell
cd bsd_kext_log/daemon

# Default target is debug
make [release]
```

### Test

After you compiled kext and daemon, you can load kext and run daemon to capture kernel messages.

Currently user space log daemon implementation is trivial, it only print logs directly into tty. Log persistence not yet implemented, we leave it to developers.

Sample way to test it:

```shell
cd bsd_kext_log
make
sudo cp -r bsd_kext_log.kext /tmp

sudo kextload -v 6 /tmp/bsd_kext_log.kext

cd daemon
make
./kextlog_daemon-debug
```

You'll see massive logs from KAuth subsystem, this experimental kext listen to several [KAuth scopes](https://developer.apple.com/library/archive/technotes/tn2127/_index.html) and push their logs into user space.

To stop the test, you should firstly terminate the daemon, and [kextunload(8)](x-man-page://8/kextunload) the kext.