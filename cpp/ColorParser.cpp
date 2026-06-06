#include "ColorParser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace rncanvas {

namespace {

uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
  return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// Hex nibble -> value, or -1 if not a hex digit.
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string toLowerTrim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace((unsigned char)s[b])) ++b;
  while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
  std::string out;
  out.reserve(e - b);
  for (size_t i = b; i < e; ++i) out.push_back((char)std::tolower((unsigned char)s[i]));
  return out;
}

// The limited named-color set promised by DESIGN §4. ARGB, fully opaque.
const std::unordered_map<std::string, uint32_t>& namedColors() {
  static const std::unordered_map<std::string, uint32_t> m = {
      {"transparent", 0x00000000},
      {"black", 0xFF000000},
      {"white", 0xFFFFFFFF},
      {"red", 0xFFFF0000},
      {"green", 0xFF008000},
      {"lime", 0xFF00FF00},
      {"blue", 0xFF0000FF},
      {"yellow", 0xFFFFFF00},
      {"cyan", 0xFF00FFFF},
      {"aqua", 0xFF00FFFF},
      {"magenta", 0xFFFF00FF},
      {"fuchsia", 0xFFFF00FF},
      {"gray", 0xFF808080},
      {"grey", 0xFF808080},
      {"silver", 0xFFC0C0C0},
      {"orange", 0xFFFFA500},
      {"purple", 0xFF800080},
  };
  return m;
}

bool parseHex(const std::string& s, uint32_t& out) {
  // s starts with '#'. Lengths: 4 (#rgb), 7 (#rrggbb), 9 (#rrggbbaa).
  auto nib = [&](size_t i) { return hexVal(s[i]); };
  if (s.size() == 4) {
    int r = nib(1), g = nib(2), b = nib(3);
    if (r < 0 || g < 0 || b < 0) return false;
    out = argb(255, (uint8_t)(r * 17), (uint8_t)(g * 17), (uint8_t)(b * 17));
    return true;
  }
  if (s.size() == 7 || s.size() == 9) {
    for (size_t i = 1; i < s.size(); ++i)
      if (nib(i) < 0) return false;
    uint8_t r = (uint8_t)(nib(1) * 16 + nib(2));
    uint8_t g = (uint8_t)(nib(3) * 16 + nib(4));
    uint8_t b = (uint8_t)(nib(5) * 16 + nib(6));
    uint8_t a = 255;
    if (s.size() == 9) a = (uint8_t)(nib(7) * 16 + nib(8));
    out = argb(a, r, g, b);
    return true;
  }
  return false;
}

uint8_t clampChannel(double v) {
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)std::lround(v);
}

// Parses the comma-separated body of rgb()/rgba(). `hasAlpha` selects rgba.
bool parseRgbBody(const std::string& body, bool hasAlpha, uint32_t& out) {
  double vals[4];
  int n = 0;
  size_t i = 0;
  while (i < body.size() && n < 4) {
    while (i < body.size() && (body[i] == ',' || std::isspace((unsigned char)body[i]))) ++i;
    if (i >= body.size()) break;
    char* end = nullptr;
    double v = std::strtod(body.c_str() + i, &end);
    if (end == body.c_str() + i) return false;  // no number consumed
    vals[n++] = v;
    i = (size_t)(end - body.c_str());
  }
  int want = hasAlpha ? 4 : 3;
  if (n != want) return false;
  uint8_t a = hasAlpha ? clampChannel(vals[3] * 255.0) : 255;
  out = argb(a, clampChannel(vals[0]), clampChannel(vals[1]), clampChannel(vals[2]));
  return true;
}

}  // namespace

bool parseColor(const std::string& input, uint32_t& out) {
  std::string s = toLowerTrim(input);
  if (s.empty()) return false;

  if (s[0] == '#') return parseHex(s, out);

  if (s.rfind("rgba", 0) == 0 || s.rfind("rgb", 0) == 0) {
    bool hasAlpha = s.rfind("rgba", 0) == 0;
    size_t open = s.find('(');
    size_t close = s.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) return false;
    return parseRgbBody(s.substr(open + 1, close - open - 1), hasAlpha, out);
  }

  auto it = namedColors().find(s);
  if (it != namedColors().end()) {
    out = it->second;
    return true;
  }
  return false;
}

}  // namespace rncanvas
