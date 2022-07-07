project(libbmp)
cmake_minimum_required(VERSION 3.15.0)
add_library(${PROJECT_NAME} STATIC bmp.c)
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")
add_compile_definitions(USESTB)
target_sources(${PROJECT_NAME} PRIVATE bmp.c)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} "3rd-party")
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