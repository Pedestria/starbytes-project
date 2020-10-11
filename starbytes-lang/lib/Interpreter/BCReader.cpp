#include "starbytes/Interpreter/BCReader.h"
#include <vector>
#include "starbytes/Interpreter/RuntimeEngine.h"

STARBYTES_INTERPRETER_NAMESPACE


class BCReader {
    private:
    std::vector<Engine::Scope *> scopes;
    public:
    BCReader(){};
    ~BCReader(){};
    void read(ByteCode::BCProgram *prog);
};

void execBCProgram(ByteCode::BCProgram *program){

};

NAMESPACE_END