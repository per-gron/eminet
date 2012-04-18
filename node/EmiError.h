#ifndef emilir_EmiError_h
#define emilir_EmiError_h

#include <string>

class EmiError {
 public:
  std::string domain;
  int32_t code;
  
 EmiError() : domain(""), code(0) {}
  
 EmiError(const std::string& domain_, int32_t code_) :
  code(code_), domain(domain_) {}
};

#endif
