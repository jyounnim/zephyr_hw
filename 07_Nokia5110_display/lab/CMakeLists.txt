cmake_minimum_required(VERSION 3.20.0)

# Custom "zds,pcd8544" devicetree binding lives under this app's own
# dts/bindings/ - DTS_ROOT must be extended BEFORE find_package(Zephyr...)
# runs, or the devicetree compiler won't find the binding and the build
# fails with "'zds,pcd8544' compatible not found".
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nokia5110_lab)

target_sources(app PRIVATE src/main.c)
