#include "rvv_test_common.h"

#include <fstream>
#include <sstream>

#ifndef RVV_SOURCE_DIR
#define RVV_SOURCE_DIR "."
#endif

static bool read_file(const std::string &path, std::string &out) {
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) { return false; }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

int main() {
  if (!rvv_test::require_rvv_and_fallback()) { return 1; }
  bool ok = true;
  size_t found = 0;
  const char *names[] = {
    "twitter.json",
    "citm_catalog.json",
    "gsoc-2018.json",
    "github_events.json",
    "canada.json"
  };

  for (const char *name : names) {
    const std::string path = std::string(RVV_SOURCE_DIR) + "/jsonexamples/" + name;
    std::string data;
    if (!read_file(path, data)) {
      std::cout << "[SKIP] corpus not present: " << path << "\n";
      continue;
    }
    ++found;
    ok &= rvv_test::compare_dom(std::string("corpus-") + name, data);
  }

  if (found == 0) {
    std::cout << "[INFO] no optional corpus files were present\n";
  }
  return rvv_test::finish(ok, "rvv_corpus_tests");
}
