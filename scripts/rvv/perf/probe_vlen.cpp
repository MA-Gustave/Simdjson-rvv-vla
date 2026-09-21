#include <cstddef>
#include <cstdint>
#include <iostream>
#include <riscv_vector.h>

int main() {
  const std::size_t bytes = __riscv_vsetvlmax_e8m1();
  if (bytes == 0) {
    return 2;
  }
  std::cout << (bytes * 8) << "\n";
  return 0;
}
