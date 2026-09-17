set(alternate_print_SOURCE_FILES
  alternate_print.cpp
)
add_executable(alternate_print ${alternate_print_SOURCE_FILES})

set(queue_SOURCE_FILES
  queue.cpp
)
add_executable(queue ${queue_SOURCE_FILES})

add_executable(condition_simple condition_simple.cpp)
target_compile_features(condition_simple PRIVATE cxx_std_17)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(condition_simple PRIVATE Threads::Threads)
endif()
