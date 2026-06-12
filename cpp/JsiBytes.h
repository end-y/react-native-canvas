// Views a JS ArrayBuffer / TypedArray / DataView as raw bytes. Shared by the
// byte-ingesting factories (__rncanvasCreateImage, __rncanvasRegisterFont).
// The returned pointer is only valid for the duration of the host call.
#pragma once

#include <jsi/jsi.h>

#include <cstddef>
#include <cstdint>

namespace rncanvas {

inline const uint8_t* jsiBytes(facebook::jsi::Runtime& rt,
                               const facebook::jsi::Value& v, size_t& len) {
  namespace jsi = facebook::jsi;
  len = 0;
  if (!v.isObject()) return nullptr;
  jsi::Object o = v.getObject(rt);
  if (o.isArrayBuffer(rt)) {
    jsi::ArrayBuffer ab = o.getArrayBuffer(rt);
    len = ab.size(rt);
    return ab.data(rt);
  }
  jsi::Value bufVal = o.getProperty(rt, "buffer");
  if (!bufVal.isObject()) return nullptr;
  jsi::Object bufObj = bufVal.getObject(rt);
  if (!bufObj.isArrayBuffer(rt)) return nullptr;
  jsi::ArrayBuffer ab = bufObj.getArrayBuffer(rt);
  // Validate the view's claimed window against the REAL buffer size — a lying
  // object must not make us read out of bounds.
  const double offD = o.getProperty(rt, "byteOffset").asNumber();
  const double lenD = o.getProperty(rt, "byteLength").asNumber();
  const size_t bufSize = ab.size(rt);
  if (!(offD >= 0) || !(lenD >= 0)) return nullptr;  // also rejects NaN
  const size_t off = (size_t)offD;
  const size_t n = (size_t)lenD;
  if (off > bufSize || n > bufSize - off) return nullptr;
  len = n;
  return ab.data(rt) + off;
}

}  // namespace rncanvas
