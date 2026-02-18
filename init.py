import os
import subprocess

def git_lib(repo_url, dst_path):
    if os.path.isdir(dst_path):
        print(f"skip: {dst_path} already exists")
        return
    subprocess.check_call(["git", "clone", "--depth", "1", repo_url, dst_path])


git_lib("https://github.com/Tencent/rapidjson.git", "deps/rapidjson")
git_lib("https://github.com/PCRE2Project/pcre2.git", "deps/pcre2")
