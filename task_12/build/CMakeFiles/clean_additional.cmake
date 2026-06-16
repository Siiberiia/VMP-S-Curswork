# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\task_12_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\task_12_autogen.dir\\ParseCache.txt"
  "task_12_autogen"
  )
endif()
