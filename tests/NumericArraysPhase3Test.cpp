#include "starbytes/interop.h"

#include <cmath>
#include <iostream>

namespace {

int fail(const char *message) {
    std::cerr << "NumericArraysPhase3Test failure: " << message << '\n';
    return 1;
}

bool approx(double lhs,double rhs,double epsilon = 1e-12) {
    return std::fabs(lhs - rhs) <= epsilon;
}

}

int main() {
    auto ints = StarbytesArrayNew();
    StarbytesArrayReserve(ints,64);
    for(int i = 0;i < 64;++i) {
        auto value = StarbytesNumNew(NumTypeInt,i);
        StarbytesArrayPush(ints,value);
        StarbytesObjectRelease(value);
    }
    if(StarbytesArrayGetLength(ints) != 64u) {
        return fail("int array length mismatch after reserve/push");
    }
    auto *firstRead = StarbytesArrayIndex(ints,10);
    auto *secondRead = StarbytesArrayIndex(ints,10);
    if(firstRead != secondRead) {
        return fail("typed numeric array index should reuse cached boxed value");
    }
    if(!StarbytesObjectTypecheck(firstRead,StarbytesNumType()) ||
       StarbytesNumGetType(firstRead) != NumTypeInt ||
       StarbytesNumGetIntValue(firstRead) != 10) {
        return fail("int array element read mismatch");
    }

    auto replacement = StarbytesNumNew(NumTypeInt,42);
    StarbytesArraySet(ints,10,replacement);
    StarbytesObjectRelease(replacement);
    auto *updated = StarbytesArrayIndex(ints,10);
    if(!StarbytesObjectTypecheck(updated,StarbytesNumType()) ||
       StarbytesNumGetType(updated) != NumTypeInt ||
       StarbytesNumGetIntValue(updated) != 42) {
        return fail("int array set mismatch");
    }

    auto mixed = StarbytesArrayNew();
    auto one = StarbytesNumNew(NumTypeInt,1);
    auto two = StarbytesNumNew(NumTypeInt,2);
    auto three = StarbytesStrNewWithData("three");
    StarbytesArrayPush(mixed,one);
    StarbytesArrayPush(mixed,two);
    StarbytesArrayPush(mixed,three);
    StarbytesObjectRelease(one);
    StarbytesObjectRelease(two);
    StarbytesObjectRelease(three);
    if(StarbytesArrayGetLength(mixed) != 3u) {
        return fail("mixed array length mismatch");
    }
    auto *mixedThird = StarbytesArrayIndex(mixed,2);
    if(!StarbytesObjectTypecheck(mixedThird,StarbytesStrType()) ||
       std::string(StarbytesStrGetBuffer(mixedThird)) != "three") {
        return fail("mixed array fallback to boxed storage failed");
    }

    auto doubles = StarbytesArrayNew();
    auto d0 = StarbytesNumNew(NumTypeDouble,1.5);
    auto d1 = StarbytesNumNew(NumTypeDouble,2.25);
    auto d2 = StarbytesNumNew(NumTypeDouble,3.75);
    StarbytesArrayPush(doubles,d0);
    StarbytesArrayPush(doubles,d1);
    StarbytesArrayPush(doubles,d2);
    StarbytesObjectRelease(d0);
    StarbytesObjectRelease(d1);
    StarbytesObjectRelease(d2);

    auto doublesCopy = StarbytesArrayCopy(doubles);
    if(StarbytesArrayGetLength(doublesCopy) != 3u) {
        return fail("double array copy length mismatch");
    }
    auto *copiedSecond = StarbytesArrayIndex(doublesCopy,1);
    if(!StarbytesObjectTypecheck(copiedSecond,StarbytesNumType()) ||
       StarbytesNumGetType(copiedSecond) != NumTypeDouble ||
       !approx(StarbytesNumGetDoubleValue(copiedSecond),2.25)) {
        return fail("double array copy/read mismatch");
    }

    StarbytesArrayPop(doublesCopy);
    if(StarbytesArrayGetLength(doublesCopy) != 2u) {
        return fail("double array pop length mismatch");
    }

    StarbytesObjectRelease(ints);
    StarbytesObjectRelease(mixed);
    StarbytesObjectRelease(doubles);
    StarbytesObjectRelease(doublesCopy);
    return 0;
}
