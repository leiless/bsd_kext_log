#
# Created 20200103
#
# CMake list file used for auto-completion in CLion IDE
# Please use make to build kext
#

cmake_minimum_required(VERSION 2.8)
project (bsd_kext_log)

execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE SDKROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories("${SDKROOT}/System/Library/Frameworks/Kernel.framework/Headers")

add_compile_options(-Wall -Wextra -Wno-unused-function -Werror)
add_compile_options(-x c -std=c99 -nostdinc -fno-builtin -fno-common -mkernel)
add_compile_options(-DDEBUG)

add_library(
    bsd_kext_log
    kext/utils.h
    kext/log_kctl.h
    kext/kauth.c
    kext/log_sysctl.h
    kext/bsd_kext_log.c
    kext/utils.c
    kext/log_kctl.c
    kext/kextlog.h
    kext/kauth.h
    kext/log_sysctl.c
)

