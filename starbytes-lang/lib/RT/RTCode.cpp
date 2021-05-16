#include "starbytes/RT/RTCode.h"
#include <fstream>

namespace starbytes {
namespace Runtime {

    #define RTCODE_STREAM_OBJECT_IN_IMPL(object) \
    std::istream & operator >>(std::istream & is,object & obj)

    #define RTCODE_STREAM_OBJECT_OUT_IMPL(object) \
    std::ostream & operator <<(std::ostream & os,object & obj)

    RTCODE_STREAM_OBJECT_IN_IMPL(RTID){
        is.read((char *)&obj.len,sizeof(size_t));
        is.read((char *)obj.value,sizeof(char) *obj.len);
        return is;
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTID){
        os.write((const char *)&obj.len,sizeof(size_t));
        os.write(obj.value,sizeof(char) * obj.len);
        return os;
    };

    RTCODE_STREAM_OBJECT_IN_IMPL(RTVar){
        is >> obj.id;
        return is;
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTVar){
        RTCode code = CODE_RTVAR;
        os.write((char *)&code,sizeof(RTCode));
        os << obj.id;
        return os;
    };

};
}
