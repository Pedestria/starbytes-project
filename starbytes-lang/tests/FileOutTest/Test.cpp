#include <iostream>
#include <fstream>
#include <string>

struct AnotherStruct {
    std::string other_name;
};

struct TestStruct {
    std::string name;
    AnotherStruct *test;
    int otherThing;
};

int main(){
    std::ofstream output ("./Test.txt",std::ios::app);
    TestStruct s;
    s.name = "Hello World!";
    s.otherThing = 15;
    AnotherStruct s1;
    s1.other_name = "Another Message";
    s.test = &s1;
    output.write((char *)&s,sizeof(s));
    return 0;
};