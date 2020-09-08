#pragma once
#include <string>
#include <vector>

namespace Starbytes {
    namespace Runtime {
        namespace Lib {
            class SBChar {

            };
            /*Starbytes String*/
            class SBString{
                private:
                    std::string INTERAL_STRING;
                public:
                    SBString(std::string v):INTERAL_STRING(v){};
                    void append();
                    SBChar * atIndex(int i);
                    void sliceAt(int i);
                    void findSubstr(SBString *substr);
            };
            template <class _Type>
            class SBArray {
                private:
                    std::vector<_Type> INTERNAL_ARRAY;
                public:
                    SBArray<_Type>(std::vector<_Type> v):INTERNAL_ARRAY(v){};
                    void push();
                    void filter(bool condition);
                    void find(bool condition);
                    _Type * getAt(int i);
            };

            template <class _Type>
            class SBSet {
                private:
                    std::vector<_Type> INTERNAL_SET;
                public:
                    SBSet<_Type>(std::vector<_Type> v):INTERNAL_SET(v){};
                    bool push();
                    void filter(bool condition);
                    void find(bool condition);
                    _Type * getAt(int i);
            };

        };
    };
};