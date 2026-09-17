#include "template.h"
#include <iostream>

int main() {
  std::cout << is_allowed_extent_conversion<3, 5>::value << std::endl;
  std::cout << is_allowed_extent_conversion<3, dynamic_extent>::value << std::endl;
  std::cout << is_allowed_element_type_conversion<int[5], int[5]>::value << std::endl;     // 1
  std::cout << is_allowed_element_type_conversion<int, unsigned int>::value << std::endl;  // 1
}