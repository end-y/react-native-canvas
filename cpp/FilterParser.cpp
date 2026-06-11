#include "FilterParser.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "ColorParser.h"

namespace rncanvas {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string toLower(const std::string& s) {
  std::string out = s;
  for (char& ch : out) ch = (char)std::tolower((unsigned char)ch);
  return out;
}

bool isSpace(char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; }

void skipSpace(const std::string& s, size_t& i) {
  while (i < s.size() && isSpace(s[i])) ++i;
}

// Splits a function's argument string on top-level whitespace, keeping
// parenthesized groups (rgb(255, 0, 0)) intact as one token.
std::vector<std::string> splitArgs(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  int depth = 0;
  for (char ch : s) {
    if (ch == '(') depth++;
    if (ch == ')') depth--;
    if (depth == 0 && isSpace(ch)) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur += ch;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

// Parses "<number><unit>" (unit may be empty). False if the token isn't a
// number or has trailing junk after the unit.
bool parseNumUnit(const std::string& tok, float& v, std::string& unit) {
  if (tok.empty()) return false;
  const char* begin = tok.c_str();
  char* end = nullptr;
  v = std::strtof(begin, &end);
  if (end == begin) return false;
  unit = toLower(std::string(end));
  for (char ch : unit) {
    if (!std::isalpha((unsigned char)ch) && ch != '%') return false;
  }
  return true;
}

// number or percentage -> factor (50% -> 0.5). Negative is invalid (CSS).
bool parseFactor(const std::string& tok, float& out) {
  float v;
  std::string unit;
  if (!parseNumUnit(tok, v, unit)) return false;
  if (unit == "%") v /= 100.0f;
  else if (!unit.empty()) return false;
  if (v < 0 || !std::isfinite(v)) return false;
  out = v;
  return true;
}

// CSS length -> px. Requires the px unit (or unitless 0, lenient: any
// unitless number is accepted as px).
bool parseLength(const std::string& tok, float& out, bool allowNegative) {
  float v;
  std::string unit;
  if (!parseNumUnit(tok, v, unit)) return false;
  if (!unit.empty() && unit != "px") return false;
  if (!std::isfinite(v)) return false;
  if (!allowNegative && v < 0) return false;
  out = v;
  return true;
}

// CSS angle -> degrees. deg/rad/grad/turn; unitless accepted as deg.
bool parseAngle(const std::string& tok, float& outDeg) {
  float v;
  std::string unit;
  if (!parseNumUnit(tok, v, unit)) return false;
  if (!std::isfinite(v)) return false;
  if (unit.empty() || unit == "deg") outDeg = v;
  else if (unit == "rad") outDeg = (float)(v * 180.0 / kPi);
  else if (unit == "grad") outDeg = v * 0.9f;
  else if (unit == "turn") outDeg = v * 360.0f;
  else return false;
  return true;
}

bool parseStep(const std::string& name, const std::string& args,
               FilterStep& out) {
  const auto toks = splitArgs(args);

  if (name == "blur") {
    out.fn = FilterFn::Blur;
    if (toks.empty()) {  // blur() = blur(0)
      out.a = 0;
      return true;
    }
    return toks.size() == 1 && parseLength(toks[0], out.a, false);
  }
  if (name == "hue-rotate") {
    out.fn = FilterFn::HueRotate;
    if (toks.empty()) {
      out.a = 0;
      return true;
    }
    return toks.size() == 1 && parseAngle(toks[0], out.a);
  }
  if (name == "drop-shadow") {
    out.fn = FilterFn::DropShadow;
    out.color = 0xFF000000;  // CSS default: currentcolor; we use black
    // Lengths in order dx dy [blur]; one optional color token anywhere.
    std::vector<float> lengths;
    bool haveColor = false;
    for (const std::string& tok : toks) {
      float px;
      // Lengths first by attempt: a token like "10px"/"−4" is a length.
      if (lengths.size() < 3 && parseLength(tok, px, lengths.size() < 2)) {
        lengths.push_back(px);
        continue;
      }
      uint32_t col;
      if (!haveColor && parseColor(tok, col)) {
        out.color = col;
        haveColor = true;
        continue;
      }
      return false;
    }
    if (lengths.size() < 2) return false;
    out.a = lengths[0];
    out.b = lengths[1];
    out.c = lengths.size() > 2 ? lengths[2] : 0.0f;
    if (out.c < 0) return false;
    return true;
  }

  // The factor family. Missing arg = the CSS initial value (1 for the
  // pass-through ones, sensible defaults otherwise).
  struct Fam {
    const char* name;
    FilterFn fn;
    float def;
    bool clamp01;
  };
  static const Fam fams[] = {
      {"brightness", FilterFn::Brightness, 1.0f, false},
      {"contrast", FilterFn::Contrast, 1.0f, false},
      {"saturate", FilterFn::Saturate, 1.0f, false},
      {"grayscale", FilterFn::Grayscale, 1.0f, true},
      {"sepia", FilterFn::Sepia, 1.0f, true},
      {"invert", FilterFn::Invert, 1.0f, true},
      {"opacity", FilterFn::Opacity, 1.0f, true},
  };
  for (const Fam& f : fams) {
    if (name != f.name) continue;
    out.fn = f.fn;
    if (toks.empty()) {
      out.a = f.def;
    } else {
      if (toks.size() != 1 || !parseFactor(toks[0], out.a)) return false;
    }
    if (f.clamp01 && out.a > 1.0f) out.a = 1.0f;
    return true;
  }
  return false;  // unknown function
}

}  // namespace

bool parseFilter(const std::string& input, FilterSpec& out) {
  out.clear();
  size_t i = 0;
  skipSpace(input, i);
  // "none" (or empty) -> empty spec.
  if (i >= input.size()) return true;
  if (toLower(input.substr(i)) == "none") return true;

  while (i < input.size()) {
    skipSpace(input, i);
    if (i >= input.size()) break;

    // Function name up to '('.
    size_t nameStart = i;
    while (i < input.size() && input[i] != '(' && !isSpace(input[i])) ++i;
    if (i >= input.size() || input[i] != '(') return false;
    const std::string name = toLower(input.substr(nameStart, i - nameStart));
    ++i;  // consume '('

    // Argument run up to the matching ')'.
    size_t argStart = i;
    int depth = 1;
    while (i < input.size() && depth > 0) {
      if (input[i] == '(') depth++;
      if (input[i] == ')') depth--;
      ++i;
    }
    if (depth != 0) return false;
    const std::string args = input.substr(argStart, i - 1 - argStart);

    FilterStep step;
    if (!parseStep(name, args, step)) return false;
    out.push_back(step);
  }
  return true;
}

}  // namespace rncanvas
