#include <iostream>
#include <string>

using namespace std;

class Exception_ : public exception {
private:
  string type;
  string message;
  int code;
public:
  Exception_(string type, string msg, int code=400) : 
  type(type), message(msg), code(code) {}

  const char* what() const noexcept {
    return message.c_str();
  }
  int code_() const noexcept {return code;}
};
