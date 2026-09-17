add_executable(decltype)
target_sources(decltype PRIVATE decltype.cpp)

add_executable(decltype_class)
target_sources(decltype_class PRIVATE decltype_class.cpp)
target_compile_options(decltype_class PRIVATE -fno-elide-constructors)

add_executable(default_function)
target_sources(default_function PRIVATE default_function.cpp)