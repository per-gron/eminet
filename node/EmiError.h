#ifndef eminet_EmiError_h
#define eminet_EmiError_h

#include <string>
#include <cstring>
#include <node.h>

class EmiError {
public:
    std::string domain;
    int32_t code;
    
    EmiError();
    EmiError(const std::string& domain_, int32_t code_);
    
    void format(const char *desc, char *buf, size_t bufSize);
    
    v8::Handle<v8::Value> raise(const char *desc);
};

#endif
