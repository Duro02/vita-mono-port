cmake_minimum_required(VERSION 3.10)
project(vita_hello C)

include(${VITASDK}/share/vita.cmake REQUIRED)

set(VITA_APP_TITLE "Pipeline Test")
set(VITA_TITLEID  "PIPE00001")
set(VITA_VERSION  "01.00")

add_executable(vita_hello main.c)

target_link_libraries(vita_hello
    vita2d
    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    ScePgf_stub
    ScePvf_stub
    SceAppMgr_stub
    SceCommonDialog_stub
    SceCtrl_stub
    SceTouch_stub
    m
)

vita_create_self(vita_hello.self vita_hello)
vita_create_vpk(vita_hello.vpk ${VITA_TITLEID} vita_hello.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_TITLE}
)
