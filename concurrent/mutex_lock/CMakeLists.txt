set(mutex_lock_SOURCE_FILES
  mutex_lock.cpp
)
add_executable(mutex_lock ${mutex_lock_SOURCE_FILES})
find_package(Threads REQUIRED)
target_link_libraries(mutex_lock PRIVATE Threads::Threads)
target_compile_features(mutex_lock PRIVATE cxx_std_17)
