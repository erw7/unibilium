cmake_minimum_required(VERSION 2.8.12)

find_program(CCACHE_EXE ccache)
if(CCACHE_EXE)
  option(CCACHE_ENABLE "Use ccache, if available" ON)
else()
  option(CCACHE_ENABLE "Use ccache, if available" OFF)
endif()

if(CCACHE_EXE AND CCACHE_ENABLE)
  message(STATUS "Enable ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_EXE}")
endif()

project(unibilium LANGUAGES C)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.c)

file(STRINGS Makefile Makefile)
foreach(line IN LISTS Makefile)
  if (line MATCHES "PKG_MAJOR=([0-9]+)")
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
  elseif(line MATCHES "PKG_MINOR=([0-9]+)")
    set(VERSION_MINOR ${CMAKE_MATCH_1})
  elseif(line MATCHES "PKG_REVISION=([0-9]+)")
	  set(VERSION_REVISION ${CMAKE_MATCH_1})
  endif()
endforeach(line)

add_library(unibilium_shared SHARED ${SRC_FILES})
set_target_properties(unibilium_shared PROPERTIES
  PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/unibilium.h
  VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
set_target_properties(unibilium_shared PROPERTIES OUTPUT_NAME "unibilium")

add_library(unibilium_static STATIC ${SRC_FILES})
set_target_properties(unibilium_static PROPERTIES
  PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/unibilium.h)
set_target_properties(unibilium_static PROPERTIES OUTPUT_NAME "unibilium")

if(NOT WIN32)
  execute_process(COMMAND sh -c "ncursesw6-config --terminfo-dirs 2>/dev/null || \
      ncurses6-config  --terminfo-dirs 2>/dev/null || \
      ncursesw5-config --terminfo-dirs 2>/dev/null || \
      ncurses5-config  --terminfo-dirs 2>/dev/null || \
      echo '/etc/terminfo:/lib/terminfo:/usr/share/terminfo:/usr/lib/terminfo:/usr/local/share/terminfo:/usr/local/lib/terminfo'"
    OUTPUT_VARIABLE TERMINFO_DIRS)
  # Remove trailing newline
  string(STRIP "${TERMINFO_DIRS}" TERMINFO_DIRS)
else()
  set(TERMINFO_DIRS "")
endif()
target_compile_definitions(unibilium_shared PUBLIC "TERMINFO_DIRS=\"${TERMINFO_DIRS}\"")
target_compile_definitions(unibilium_static PUBLIC "TERMINFO_DIRS=\"${TERMINFO_DIRS}\"")

set(INCDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
set(LIBDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION})
configure_file(unibilium.pc.in unibilium.pc @ONLY)

include(GNUInstallDirs)
install(FILES ${PROJECT_BINARY_DIR}/unibilium.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(TARGETS unibilium_shared unibilium_static
  PUBLIC_HEADER
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
