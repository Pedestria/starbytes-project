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

def run_cmd(cmd, cwd):
    try:
        subprocess.check_call(cmd, cwd=cwd)
        return True
    except subprocess.CalledProcessError as exc:
        print(f"warn: command failed in {cwd}: {' '.join(cmd)} ({exc})")
        return False

def bootstrap_openssl(dst_path):
    if os.name == "nt":
        print("skip: OpenSSL vendored build not configured for Windows in init.py")
        return
    include_marker = os.path.join(dst_path, "out", "include", "openssl", "opensslv.h")
    lib_marker = os.path.join(dst_path, "out", "lib", "libcrypto.a")
    lib64_marker = os.path.join(dst_path, "out", "lib64", "libcrypto.a")
    if os.path.isfile(include_marker) and (os.path.isfile(lib_marker) or os.path.isfile(lib64_marker)):
        print("skip: deps/openssl/out already bootstrapped")
        return

    configure = os.path.join(dst_path, "Configure")
    if not os.path.isfile(configure):
        print(f"warn: missing OpenSSL Configure script at {configure}")
        return

    out_dir = os.path.join(dst_path, "out")
    jobs = str(os.cpu_count() or 4)
    if not run_cmd(["./Configure", "--prefix=" + os.path.abspath(out_dir), "no-shared", "no-tests"], cwd=dst_path):
        return
    if not run_cmd(["make", "-j", jobs], cwd=dst_path):
        return
    run_cmd(["make", "install_sw"], cwd=dst_path)


git_lib("https://github.com/Tencent/rapidjson.git", "deps/rapidjson")
git_lib("https://github.com/PCRE2Project/pcre2.git", "deps/pcre2")
git_lib("https://github.com/unicode-org/icu.git", "deps/icu")
git_lib("https://github.com/chriskohlhoff/asio.git", "deps/asio")
git_lib("https://github.com/curl/curl.git", "deps/libcurl")
git_lib("https://github.com/openssl/openssl.git", "deps/openssl")
git_lib("https://github.com/madler/zlib.git", "deps/zlib")
bootstrap_openssl("deps/openssl")
