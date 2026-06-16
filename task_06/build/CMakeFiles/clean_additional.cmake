# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\task_06_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\task_06_autogen.dir\\ParseCache.txt"
  "task_06_autogen"
  )
endif()
