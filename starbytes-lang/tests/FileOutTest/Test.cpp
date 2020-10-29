#include <iostream>
#include <fstream>
#include <string>
#include "starbytes/Base/Base.h"
#include "starbytes/ByteCode/BCDef.h"
#include "starbytes/ByteCode/BCSerializer.h"


using namespace Starbytes;

using namespace ByteCode;

BCCodeBegin * tst1(std::string name){
    BCCodeBegin * rc = new BCCodeBegin();
    rc->code_node_name = name;
    return rc;
};

BCCodeEnd * tst2(std::string name){
    BCCodeEnd * rc = new BCCodeEnd();
    rc->code_node_name = name;
    return rc;
};

int main(){
    std::ofstream output ("./Test.txt",std::ios::app);
    BCCodeBegin * a = tst1("crtvr");
    BCCodeEnd * b = tst2("crtvr");
    output.write((char *)a,sizeof(*a));
    output.write((char *)b,sizeof(*b));
    output.close();
    std::cout << "Successfully Wrote to ./Text.txt" << std::endl;
    return 0;
};