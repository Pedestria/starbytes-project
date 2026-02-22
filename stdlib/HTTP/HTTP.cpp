#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <algorithm>
#include <climits>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

#ifdef STARBYTES_HAS_CURL
#include <curl/curl.h>
#endif

namespace {

using starbytes::string_map;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

struct HttpResult {
    bool ok = false;
    long status = 0;
    std::string body;
    string_map<std::string> headers;
};

#ifdef STARBYTES_HAS_CURL
std::once_flag g_curlInitFlag;
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

bool readStringArrayArg(StarbytesFuncArgs args,std::vector<std::string> &outValues) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesArrayType())) {
        return false;
    }

    auto len = StarbytesArrayGetLength(arg);
    outValues.clear();
    outValues.reserve(len);
    for(unsigned i = 0; i < len; ++i) {
        auto item = StarbytesArrayIndex(arg,i);
        if(!item || !StarbytesObjectTypecheck(item,StarbytesStrType())) {
            return false;
        }
        outValues.emplace_back(StarbytesStrGetBuffer(item));
    }
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

std::string trimCopy(const std::string &value) {
    size_t start = 0;
    while(start < value.size() && std::isspace((unsigned char)value[start]) != 0) {
        ++start;
    }
    size_t end = value.size();
    while(end > start && std::isspace((unsigned char)value[end - 1]) != 0) {
        --end;
    }
    return value.substr(start,end - start);
}

#ifdef STARBYTES_HAS_CURL
size_t writeBodyCallback(char *ptr,size_t size,size_t nmemb,void *userdata) {
    auto *body = reinterpret_cast<std::string *>(userdata);
    if(!body || !ptr) {
        return 0;
    }
    auto total = size * nmemb;
    body->append(ptr,total);
    return total;
}

size_t writeHeaderCallback(char *buffer,size_t size,size_t nitems,void *userdata) {
    auto *headers = reinterpret_cast<string_map<std::string> *>(userdata);
    if(!headers || !buffer) {
        return 0;
    }
    auto total = size * nitems;
    std::string line(buffer,total);

    auto colon = line.find(':');
    if(colon != std::string::npos) {
        auto key = trimCopy(line.substr(0,colon));
        auto value = trimCopy(line.substr(colon + 1));
        if(!key.empty()) {
            (*headers)[key] = value;
        }
    }
    return total;
}

bool ensureCurlInitialized() {
    std::call_once(g_curlInitFlag,[]() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
    return true;
}

HttpResult performHttpRequest(const std::string &method,
                              const std::string &url,
                              const std::string &body,
                              int timeoutMillis,
                              const std::vector<std::string> &requestHeaders) {
    HttpResult result;
    if(url.empty()) {
        return result;
    }

    ensureCurlInitialized();

    CURL *easy = curl_easy_init();
    if(!easy) {
        return result;
    }

    struct curl_slist *headerList = nullptr;
    for(const auto &entry : requestHeaders) {
        headerList = curl_slist_append(headerList,entry.c_str());
    }

    curl_easy_setopt(easy,CURLOPT_URL,url.c_str());
    curl_easy_setopt(easy,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,writeBodyCallback);
    curl_easy_setopt(easy,CURLOPT_WRITEDATA,&result.body);
    curl_easy_setopt(easy,CURLOPT_HEADERFUNCTION,writeHeaderCallback);
    curl_easy_setopt(easy,CURLOPT_HEADERDATA,&result.headers);

    if(timeoutMillis > 0) {
        curl_easy_setopt(easy,CURLOPT_TIMEOUT_MS,(long)timeoutMillis);
    }

    if(headerList) {
        curl_easy_setopt(easy,CURLOPT_HTTPHEADER,headerList);
    }

    auto upperMethod = method;
    std::transform(upperMethod.begin(),upperMethod.end(),upperMethod.begin(),[](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if(upperMethod == "POST") {
        curl_easy_setopt(easy,CURLOPT_POST,1L);
        curl_easy_setopt(easy,CURLOPT_POSTFIELDS,body.c_str());
        curl_easy_setopt(easy,CURLOPT_POSTFIELDSIZE,(long)body.size());
    }
    else if(upperMethod != "GET") {
        curl_easy_setopt(easy,CURLOPT_CUSTOMREQUEST,upperMethod.c_str());
        if(!body.empty()) {
            curl_easy_setopt(easy,CURLOPT_POSTFIELDS,body.c_str());
            curl_easy_setopt(easy,CURLOPT_POSTFIELDSIZE,(long)body.size());
        }
    }

    auto code = curl_easy_perform(easy);
    if(code == CURLE_OK) {
        curl_easy_getinfo(easy,CURLINFO_RESPONSE_CODE,&result.status);
        result.ok = true;
    }

    if(headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(easy);
    return result;
}
#endif

StarbytesObject makeHeadersDict(const string_map<std::string> &headers) {
    auto dict = StarbytesDictNew();
    for(const auto &entry : headers) {
        auto key = StarbytesStrNewWithData(entry.first.c_str());
        auto value = StarbytesStrNewWithData(entry.second.c_str());
        StarbytesDictSet(dict,key,value);
    }
    return dict;
}

StarbytesObject makeHttpResponseObject(const HttpResult &result) {
    auto response = StarbytesObjectNew(StarbytesMakeClass("HttpResponse"));

    int status = 0;
    if(result.status > LONG_MAX) {
        status = INT_MAX;
    }
    else if(result.status > INT_MAX) {
        status = INT_MAX;
    }
    else if(result.status < 0) {
        status = 0;
    }
    else {
        status = static_cast<int>(result.status);
    }

    StarbytesObjectAddProperty(response,(char *)"status",makeInt(status));
    StarbytesObjectAddProperty(response,(char *)"body",StarbytesStrNewWithData(result.body.c_str()));
    StarbytesObjectAddProperty(response,(char *)"headers",makeHeadersDict(result.headers));
    StarbytesObjectAddProperty(response,(char *)"ok",makeBool(result.ok && status >= 200 && status < 300));
    return response;
}

STARBYTES_FUNC(http_get) {
    skipOptionalModuleReceiver(args,3);

    std::string url;
    int timeoutMillis = 0;
    std::vector<std::string> headers;
    if(!readStringArg(args,url) || !readIntArg(args,timeoutMillis) || !readStringArrayArg(args,headers)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_CURL
    auto result = performHttpRequest("GET",url,"",timeoutMillis,headers);
    if(!result.ok) {
        return nullptr;
    }
    return makeHttpResponseObject(result);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(http_post) {
    skipOptionalModuleReceiver(args,4);

    std::string url;
    std::string body;
    int timeoutMillis = 0;
    std::vector<std::string> headers;
    if(!readStringArg(args,url) || !readStringArg(args,body) || !readIntArg(args,timeoutMillis) || !readStringArrayArg(args,headers)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_CURL
    auto result = performHttpRequest("POST",url,body,timeoutMillis,headers);
    if(!result.ok) {
        return nullptr;
    }
    return makeHttpResponseObject(result);
#else
    return nullptr;
#endif
}

STARBYTES_FUNC(http_request) {
    skipOptionalModuleReceiver(args,5);

    std::string method;
    std::string url;
    std::string body;
    int timeoutMillis = 0;
    std::vector<std::string> headers;
    if(!readStringArg(args,method) || !readStringArg(args,url) || !readStringArg(args,body)
       || !readIntArg(args,timeoutMillis) || !readStringArrayArg(args,headers)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_CURL
    auto result = performHttpRequest(method,url,body,timeoutMillis,headers);
    if(!result.ok) {
        return nullptr;
    }
    return makeHttpResponseObject(result);
#else
    return nullptr;
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

    addFunc(module,"http_get",3,http_get);
    addFunc(module,"http_post",4,http_post);
    addFunc(module,"http_request",5,http_request);

    return module;
}
