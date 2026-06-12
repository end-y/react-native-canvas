#include "FontParser.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace rncanvas {

namespace {

std::string toLower(const std::string& s) {
  std::string out = s;
  for (char& ch : out) ch = (char)std::tolower((unsigned char)ch);
  return out;
}

std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace((unsigned char)s[b])) ++b;
  while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
  return s.substr(b, e - b);
}

// Splits on whitespace.
std::vector<std::string> words(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char ch : s) {
    if (std::isspace((unsigned char)ch)) {
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

// "16px" / "16.5px", optionally "/1.5" line-height glued on ("16px/1.5") —
// line-height is ignored. The px unit is REQUIRED (CSS; a bare number would
// also collide with numeric weights). False if not a size token.
bool isSizeToken(const std::string& tok) {
  std::string t = tok;
  const size_t slash = t.find('/');
  if (slash != std::string::npos) t = t.substr(0, slash);  // drop line-height
  if (t.empty()) return false;
  const char* begin = t.c_str();
  char* end = nullptr;
  const float v = std::strtof(begin, &end);
  if (end == begin || !std::isfinite(v) || v <= 0) return false;
  return toLower(std::string(end)) == "px";
}

float sizeValue(const std::string& tok) {
  std::string t = tok;
  const size_t slash = t.find('/');
  if (slash != std::string::npos) t = t.substr(0, slash);
  return std::strtof(t.c_str(), nullptr);
}

// Pre-size keyword: style/weight/ignorable. False = unknown token (whole
// string is invalid, like the web).
bool applyKeyword(const std::string& lower, FontSpec& out) {
  if (lower == "normal" || lower == "small-caps") return true;  // no-op
  if (lower == "italic" || lower == "oblique") {
    out.italic = true;
    return true;
  }
  if (lower == "bold") {
    out.weight = 700;
    return true;
  }
  if (lower == "bolder") {
    out.weight = 700;
    return true;
  }
  if (lower == "lighter") {
    out.weight = 300;
    return true;
  }
  // Numeric weight: 100..900.
  char* end = nullptr;
  const long w = std::strtol(lower.c_str(), &end, 10);
  if (end != lower.c_str() && *end == '\0' && w >= 100 && w <= 900) {
    out.weight = (uint16_t)w;
    return true;
  }
  return false;
}

}  // namespace

bool parseFont(const std::string& input, FontSpec& out) {
  FontSpec spec;

  // Find the size token; everything before it is keywords, everything after
  // (in the ORIGINAL string, to keep family spacing) is the family list.
  const std::string trimmed = trim(input);
  if (trimmed.empty()) return false;

  size_t pos = 0;  // scan position in `trimmed`
  bool sizeFound = false;
  while (pos < trimmed.size()) {
    while (pos < trimmed.size() && std::isspace((unsigned char)trimmed[pos]))
      ++pos;
    size_t end = pos;
    while (end < trimmed.size() && !std::isspace((unsigned char)trimmed[end]))
      ++end;
    if (end == pos) break;
    const std::string tok = trimmed.substr(pos, end - pos);
    // Keywords first: a numeric weight ("600") would otherwise be eaten by
    // the lenient bare-number-as-px size rule. CSS puts weight before size,
    // so a bare integer in 100..900 here IS a weight, not a size.
    if (applyKeyword(toLower(tok), spec)) {
      pos = end;
      continue;
    }
    if (isSizeToken(tok)) {
      spec.size = sizeValue(tok);
      pos = end;
      sizeFound = true;
      break;
    }
    return false;  // neither keyword nor size
  }
  if (!sizeFound) return false;

  // Family list: required by CSS. Comma-separated; quotes stripped.
  const std::string famStr = trim(trimmed.substr(pos));
  if (famStr.empty()) return false;
  size_t start = 0;
  while (start <= famStr.size()) {
    size_t comma = famStr.find(',', start);
    if (comma == std::string::npos) comma = famStr.size();
    std::string fam = trim(famStr.substr(start, comma - start));
    if (fam.size() >= 2 && (fam.front() == '"' || fam.front() == '\'') &&
        fam.back() == fam.front()) {
      fam = trim(fam.substr(1, fam.size() - 2));
    }
    if (fam.empty()) return false;  // "16px ," style garbage
    spec.families.push_back(fam);
    start = comma + 1;
  }

  out = std::move(spec);
  return true;
}

}  // namespace rncanvas
