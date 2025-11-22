#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <iostream>
#include <string>

class Exception_ : public std::exception {
private:
  std::string type;
  std::string message;
  int code;
public:
  Exception_(std::string type, std::string msg, int code=400) : 
  type(type), message(msg), code(code) {}

  const char* what() const noexcept {
    return message.c_str();
  }
  int code_() const noexcept {return code;}
};

#endif