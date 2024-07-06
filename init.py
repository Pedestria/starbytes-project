import io
# import requests
import os

def download_lib(a,b):
    lib = requests.get(a)
    file = io.open(a,"wb")
    file.write(lib.content)
    file.close()


def git_lib(a,b):
    os.system(f"git clone {a} {b}")
    


git_lib("https://github.com/Tencent/rapidjson.git","deps/rapidjson")
