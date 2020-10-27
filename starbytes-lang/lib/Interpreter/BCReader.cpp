#include "starbytes/Interpreter/BCReader.h"
#include "RuntimeEngine.h"
#include "starbytes/Base/Macros.h"
#include <vector>

STARBYTES_INTERPRETER_NAMESPACE

using namespace ByteCode;

class BCReader {
private:
  std::vector<Engine::Scope *> scopes;
  std::vector<std::string> scope_heirarchy;

  void set_current_scope(std::string & _scope_name) {
    scope_heirarchy.push_back(_scope_name);
  }

  void remove_current_scope() { scope_heirarchy.pop_back(); }

  std::string &get_current_scope() { return scope_heirarchy.front(); }

  void create_scope(std::string name) {
    Engine::Scope *c = new Engine::Scope(name);
    scopes.push_back(c);
  }

  Engine::Scope *get_scope(std::string name) {
    for (auto &s : scopes) {
      if (s->getName() == name) {
        return s;
      }
    }
    return nullptr;
  }

public:
  BCReader(){};
  ~BCReader(){};
  void read(ByteCode::BCProgram *& prog);
};

void BCReader::read(ByteCode::BCProgram *& prog){
    
};

void execBCProgram(BCProgram *&program){

};

NAMESPACE_END