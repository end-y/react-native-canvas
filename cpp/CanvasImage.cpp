#include "CanvasImage.h"

#include <cstring>
#include <string>
#include <vector>

#include "ImageDecode.h"
#include "JsiBytes.h"

namespace rncanvas {

using namespace facebook;

jsi::Value ImageHost::get(jsi::Runtime& rt, const jsi::PropNameID& nameId) {
  const std::string name = nameId.utf8(rt);
  if (name == "width") return jsi::Value((double)data_->width);
  if (name == "height") return jsi::Value((double)data_->height);
  // Bytes are present and bounds-sniffed by construction.
  if (name == "complete") return jsi::Value(true);
  return jsi::Value::undefined();
}

void installImage(jsi::Runtime& rt) {
  auto create = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forUtf8(rt, "__rncanvasCreateImage"), 1,
      [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
         size_t count) -> jsi::Value {
        if (count < 1) return jsi::Value::null();
        size_t len = 0;
        const uint8_t* bytes = jsiBytes(rt, args[0], len);
        if (!bytes || len == 0) return jsi::Value::null();

        auto img = std::make_shared<EncodedImage>();
        img->bytes.assign(bytes, bytes + len);  // own a copy (JS may free)
        if (!imageBounds(img->bytes.data(), img->bytes.size(), img->width,
                         img->height)) {
          return jsi::Value::null();  // not a decodable png/jpeg/webp
        }
        return jsi::Value(rt, jsi::Object::createFromHostObject(
                                  rt, std::make_shared<ImageHost>(
                                          std::move(img))));
      });
  rt.global().setProperty(rt, "__rncanvasCreateImage", create);
}

}  // namespace rncanvas
