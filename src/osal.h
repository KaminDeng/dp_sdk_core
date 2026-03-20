// osal.h — Entry point for the osal library's platform configuration.
//
// This header is included by all osal implementation files to pull in
// osal_port.h (the single-file user port). It must resolve via the
// include path set by CMakeLists.txt from OSAL_PORT_DIR.
//
// Users: do NOT include this file directly. Include the individual
// component headers (osal_thread.h, osal_mutex.h, etc.) instead.
#pragma once

#if __has_include("osal_port.h")
#include "osal_port.h"
#else
#error \
    "[osal] osal_port.h not found. " \
         "Set OSAL_PORT_DIR in your CMakeLists.txt BEFORE add_subdirectory(osal):\n" \
         "  set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/your_port_dir)\n" \
         "Template: copy osal/port/template/osal_port.h to your port directory."
#endif
