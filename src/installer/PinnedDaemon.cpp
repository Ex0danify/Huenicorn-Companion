#include "installer/PinnedDaemon.hpp"

QString DaemonPin::version() const
{
  return tag.startsWith('v') ? tag.mid(1) : tag;
}


DaemonPin PinnedDaemon::current()
{
  DaemonPin pin;
  pin.repoUrl = "https://gitlab.com/openjowelsofts/huenicorn.git";
  pin.tag = "v1.2.0";
  pin.commit = "ede353ac329ba1922d354aae89adf5fe6ddb03f5";

  pin.dependencies = {
    {"nlohmann_json", "nlohmann_json",
     "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz",
     "42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa"},
    {"cpp_httplib", "httplib",
     "https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.46.0.tar.gz",
     "4b91c970132cd140586ba880beab099fd16f0dc48425a5a60c418422c04d9fc1"},
    {"glm", "glm",
     "https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz",
     "6775e47231a446fd086d660ecc18bcd076531cfedd912fbd66e576b118607001"},
  };

  pin.compilerFlags = {"-include", "array", "-include", "limits", "-include", "cmath"};
  return pin;
}
