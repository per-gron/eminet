#define BUILDING_NODE_EXTENSION

#include "EmiError.h"

#include <node.h>

using namespace v8;

EmiError::EmiError() : domain(""), code(0) {}

EmiError::EmiError(const std::string& domain_, int32_t code_) :
code(code_), domain(domain_) {}

void EmiError::format(const char *desc, char *buf, size_t bufSize) {
    snprintf(buf, bufSize, "%s: %s (%d)", desc, domain.c_str(), code);
}

Handle<Value> EmiError::raise(const char *desc) {
    HandleScope scope;
    
    char buf[256];
    format(desc, buf, sizeof(buf));
    
    Handle<Value> exc(Exception::Error(String::New(buf)));
    ThrowException(exc);
    
    return scope.Close(Undefined());
}
