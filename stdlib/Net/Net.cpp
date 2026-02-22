#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <climits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef STARBYTES_HAS_ASIO
#include <asio.hpp>
#endif

namespace {

using starbytes::string_set;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

#ifdef STARBYTES_HAS_ASIO
struct TcpSocketState {
    asio::io_context io;
    asio::ip::tcp::socket socket;

    TcpSocketState(): io(), socket(io) {}
};

std::unordered_map<StarbytesObject,std::unique_ptr<TcpSocketState>> g_socketRegistry;
#endif

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

bool readStringArg(StarbytesFuncArgs args,std::string &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())) {
        return false;
    }
    outValue = StarbytesStrGetBuffer(arg);
    return true;
}

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

void skipOptionalModuleReceiver(StarbytesFuncArgs args,unsigned expectedUserArgs) {
    auto *raw = reinterpret_cast<NativeArgsLayout *>(args);
    if(!raw || raw->argc < raw->index) {
        return;
    }
    auto remaining = raw->argc - raw->index;
    if(remaining == expectedUserArgs + 1) {
        (void)StarbytesFuncArgsGetArg(args);
    }
}

StarbytesObject intVectorToArray(const std::vector<unsigned char> &values) {
    auto out = StarbytesArrayNew();
    for(unsigned char value : values) {
        StarbytesArrayPush(out,makeInt((int)value));
    }
    return out;
}

#ifdef STARBYTES_HAS_ASIO
TcpSocketState *requireSocketSelf(StarbytesFuncArgs args) {
    auto self = StarbytesFuncArgsGetArg(args);
    if(!self) {
        return nullptr;
    }
    auto it = g_socketRegistry.find(self);
    if(it == g_socketRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool readByteArrayArg(StarbytesFuncArgs args,std::vector<unsigned char> &outValues) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesArrayType())) {
        return false;
    }

    auto len = StarbytesArrayGetLength(arg);
    outValues.clear();
    outValues.reserve(len);
    for(unsigned i = 0; i < len; ++i) {
        auto item = StarbytesArrayIndex(arg,i);
        if(!item || !StarbytesObjectTypecheck(item,StarbytesNumType()) || StarbytesNumGetType(item) != NumTypeInt) {
            return false;
        }
        auto value = StarbytesNumGetIntValue(item);
        if(value < 0 || value > 255) {
            return false;
        }
        outValues.push_back((unsigned char)value);
    }
    return true;
}
#endif

STARBYTES_FUNC(net_tcpSocket) {
    skipOptionalModuleReceiver(args,0);

#ifdef STARBYTES_HAS_ASIO
    auto object = StarbytesObjectNew(StarbytesMakeClass("TcpSocket"));
    g_socketRegistry[object] = std::make_unique<TcpSocketState>();
    return object;
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(net_resolve) {
    skipOptionalModuleReceiver(args,2);

    std::string host;
    std::string service;
    if(!readStringArg(args,host) || !readStringArg(args,service) || host.empty() || service.empty()) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ASIO
    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    std::error_code ec;
    auto endpoints = resolver.resolve(host,service,ec);
    if(ec) {
        return nullptr;
    }

    string_set seen;
    auto out = StarbytesArrayNew();
    for(const auto &entry : endpoints) {
        auto endpoint = entry.endpoint();
        auto text = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        if(seen.insert(text).second) {
            StarbytesArrayPush(out,StarbytesStrNewWithData(text.c_str()));
        }
    }
    return out;
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(net_isIPAddress) {
    skipOptionalModuleReceiver(args,1);

    std::string value;
    if(!readStringArg(args,value) || value.empty()) {
        return makeBool(false);
    }

#ifdef STARBYTES_HAS_ASIO
    std::error_code ec;
    auto address = asio::ip::make_address(value,ec);
    (void)address;
    return makeBool(!ec);
#else
    return makeBool(false);
#endif
}

STARBYTES_FUNC(Net_TcpSocket_connect) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return nullptr;
    }

    std::string host;
    int port = 0;
    if(!readStringArg(args,host) || !readIntArg(args,port) || host.empty() || port <= 0 || port > 65535) {
        return nullptr;
    }

    std::error_code ec;
    asio::ip::tcp::resolver resolver(state->io);
    auto endpoints = resolver.resolve(host,std::to_string(port),ec);
    if(ec) {
        return nullptr;
    }

    asio::connect(state->socket,endpoints,ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(true);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(Net_TcpSocket_read) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return nullptr;
    }

    int maxBytes = 0;
    if(!readIntArg(args,maxBytes) || maxBytes < 0) {
        return nullptr;
    }
    if(maxBytes == 0) {
        return StarbytesArrayNew();
    }

    std::vector<unsigned char> buffer((size_t)maxBytes);
    std::error_code ec;
    auto readCount = state->socket.read_some(asio::buffer(buffer),ec);
    if(ec && ec != asio::error::eof) {
        return nullptr;
    }

    buffer.resize(readCount);
    return intVectorToArray(buffer);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(Net_TcpSocket_write) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return nullptr;
    }

    std::vector<unsigned char> bytes;
    if(!readByteArrayArg(args,bytes)) {
        return nullptr;
    }

    std::error_code ec;
    auto written = asio::write(state->socket,asio::buffer(bytes),ec);
    if(ec) {
        return nullptr;
    }

    if(written > (size_t)INT_MAX) {
        written = (size_t)INT_MAX;
    }
    return makeInt((int)written);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(Net_TcpSocket_writeText) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return nullptr;
    }

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

    std::error_code ec;
    auto written = asio::write(state->socket,asio::buffer(text),ec);
    if(ec) {
        return nullptr;
    }

    if(written > (size_t)INT_MAX) {
        written = (size_t)INT_MAX;
    }
    return makeInt((int)written);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(Net_TcpSocket_close) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return nullptr;
    }

    std::error_code ec;
    state->socket.close(ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(true);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(Net_TcpSocket_isOpen) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return makeBool(false);
    }
    return makeBool(state->socket.is_open());
#else
    return makeBool(false);
#endif
}

STARBYTES_FUNC(Net_TcpSocket_remoteAddress) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return StarbytesStrNewWithData("");
    }

    std::error_code ec;
    auto endpoint = state->socket.remote_endpoint(ec);
    if(ec) {
        return StarbytesStrNewWithData("");
    }
    auto addr = endpoint.address().to_string();
    return StarbytesStrNewWithData(addr.c_str());
#else
    return StarbytesStrNewWithData("");
#endif
}

STARBYTES_FUNC(Net_TcpSocket_remotePort) {
#ifdef STARBYTES_HAS_ASIO
    auto *state = requireSocketSelf(args);
    if(!state) {
        return makeInt(0);
    }

    std::error_code ec;
    auto endpoint = state->socket.remote_endpoint(ec);
    if(ec) {
        return makeInt(0);
    }
    return makeInt((int)endpoint.port());
#else
    return makeInt(0);
#endif
}

void addFunc(StarbytesNativeModule *module,const char *name,unsigned argCount,StarbytesFuncCallback callback) {
    StarbytesFuncDesc desc;
    desc.name = CStringMake(name);
    desc.argCount = argCount;
    desc.callback = callback;
    StarbytesNativeModuleAddDesc(module,&desc);
}

}

STARBYTES_NATIVE_MOD_MAIN() {
    auto module = StarbytesNativeModuleCreate();

    addFunc(module,"net_tcpSocket",0,net_tcpSocket);
    addFunc(module,"net_resolve",2,net_resolve);
    addFunc(module,"net_isIPAddress",1,net_isIPAddress);

    addFunc(module,"Net_TcpSocket_connect",3,Net_TcpSocket_connect);
    addFunc(module,"Net_TcpSocket_read",2,Net_TcpSocket_read);
    addFunc(module,"Net_TcpSocket_write",2,Net_TcpSocket_write);
    addFunc(module,"Net_TcpSocket_writeText",2,Net_TcpSocket_writeText);
    addFunc(module,"Net_TcpSocket_close",1,Net_TcpSocket_close);
    addFunc(module,"Net_TcpSocket_isOpen",1,Net_TcpSocket_isOpen);
    addFunc(module,"Net_TcpSocket_remoteAddress",1,Net_TcpSocket_remoteAddress);
    addFunc(module,"Net_TcpSocket_remotePort",1,Net_TcpSocket_remotePort);

    return module;
}
