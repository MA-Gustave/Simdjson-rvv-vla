#include "simdjson.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using clock_type = std::chrono::steady_clock;

static void die(const std::string &message) {
  std::cerr << message << "\n";
  std::exit(2);
}

static std::string escape_json(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

int main(int argc, char **argv) {
  std::string impl_name;
  std::string mode;
  std::string filename;
  std::size_t iterations = 100;
  std::size_t warmup = 5;

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    auto need = [&](const char *name) -> std::string {
      if (++i >= argc) die(std::string("missing value for ") + name);
      return std::string(argv[i]);
    };
    if (arg == "--impl") impl_name = need("--impl");
    else if (arg == "--mode") mode = need("--mode");
    else if (arg == "--iterations") iterations = std::stoull(need("--iterations"));
    else if (arg == "--warmup") warmup = std::stoull(need("--warmup"));
    else if (arg == "--file") filename = need("--file");
    else if (arg == "--help") {
      std::cout << "Usage: rvv_microbench --impl NAME --mode minify|utf8 "
                   "--iterations N --warmup N --file FILE\n";
      return 0;
    } else {
      die("unknown argument: " + arg);
    }
  }

  if (impl_name.empty() || mode.empty() || filename.empty()) {
    die("missing --impl, --mode or --file");
  }

  const simdjson::implementation *impl =
      simdjson::get_available_implementations()[impl_name];
  if (!impl || !impl->supported_by_runtime_system()) {
    die("implementation unavailable on this machine: " + impl_name);
  }
  simdjson::get_active_implementation() = impl;

  simdjson::padded_string input;
  auto load_error = simdjson::padded_string::load(filename).get(input);
  if (load_error) {
    die("cannot load input: " + std::string(simdjson::error_message(load_error)));
  }

  volatile std::uint64_t sink = 0;
  std::unique_ptr<char[]> output;
  if (mode == "minify") {
    output.reset(new char[input.size() + simdjson::SIMDJSON_PADDING]);
  } else if (mode != "utf8") {
    die("unsupported mode: " + mode);
  }

  auto once = [&]() {
    if (mode == "minify") {
      std::size_t new_length = 0;
      auto error = simdjson::minify(
          input.data(), input.size(), output.get(), new_length);
      if (error) {
        die("minify failed: " + std::string(simdjson::error_message(error)));
      }
      sink ^= static_cast<std::uint64_t>(new_length);
    } else {
      bool ok = simdjson::validate_utf8(input.data(), input.size());
      if (!ok) die("validate_utf8 rejected benchmark input");
      sink ^= static_cast<std::uint64_t>(ok);
    }
  };

  for (std::size_t i = 0; i < warmup; ++i) once();

  const auto t0 = clock_type::now();
  for (std::size_t i = 0; i < iterations; ++i) once();
  const auto t1 = clock_type::now();

  const double seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
  const double total_bytes =
      static_cast<double>(input.size()) * static_cast<double>(iterations);
  const double gbps = (total_bytes / 1.0e9) / seconds;

  std::cout << std::fixed << std::setprecision(9)
            << "{"
            << "\"implementation\":\"" << escape_json(impl_name) << "\","
            << "\"mode\":\"" << escape_json(mode) << "\","
            << "\"file\":\"" << escape_json(filename) << "\","
            << "\"bytes\":" << input.size() << ","
            << "\"iterations\":" << iterations << ","
            << "\"seconds\":" << seconds << ","
            << "\"gbps\":" << gbps << ","
            << "\"sink\":" << sink
            << "}\n";
  return 0;
}
