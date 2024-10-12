cmake_minimum_required(VERSION 3.5.0)

project(libbmp)

# Setup default options
option(ENABLE_JPEG "Enable JPG support via libjpg" OFF)
option(ENABLE_FTYPEFONT "Enable FreeType font wrapper" OFF)

set(BITMAP_SOURCE "bmp.c")

if(ENABLE_JPEG)
  add_compile_definitions(USEJPG)
  set(JPEG_INCLUDE_DIR "../jpeg")
endif()

if(ENABLE_FTYPEFONT)
  set(BITMAP_SOURCE ${BITMAP_SOURCE} "ftypefont/ftfont.c")
  set(FREETYPE_INCLUDE_DIR "../freetype/include")
endif()

add_library(${PROJECT_NAME} STATIC ${BITMAP_SOURCE})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${JPEG_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIR})

if (MSVC)
  # Statically link with the C runtime library
  set(CompilerFlags
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELWITHDEBINFO
  )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    set(${CompilerFlag} "${${CompilerFlag}}" CACHE STRING "msvc compiler flags" FORCE)
  endforeach()
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
