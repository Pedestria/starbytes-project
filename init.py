import os
import subprocess

def git_lib(repo_url, dst_path):
    if os.path.isdir(dst_path):
        print(f"skip: {dst_path} already exists")
        return
    try:
        subprocess.check_call(["git", "clone", "--depth", "1", repo_url, dst_path])
    except subprocess.CalledProcessError as exc:
        print(f"warn: failed to clone {repo_url} -> {dst_path} ({exc})")


git_lib("https://github.com/Tencent/rapidjson.git", "deps/rapidjson")
git_lib("https://github.com/PCRE2Project/pcre2.git", "deps/pcre2")
git_lib("https://github.com/unicode-org/icu.git", "deps/icu")
git_lib("https://github.com/chriskohlhoff/asio.git", "deps/asio")
git_lib("https://github.com/curl/curl.git", "deps/libcurl")
