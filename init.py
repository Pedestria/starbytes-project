import os
import subprocess
import shutil
import urllib.request
import zipfile

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

def find_existing_perl(perl_root):
    if not os.path.isdir(perl_root):
        return None
    for root, _, files in os.walk(perl_root):
        if "perl.exe" not in files:
            continue
        candidate = os.path.join(root, "perl.exe")
        norm = candidate.replace("/", "\\").lower()
        if norm.endswith("\\perl\\bin\\perl.exe"):
            return candidate
    return None

def bootstrap_perl_from_automdeps():
    automdeps_root = os.environ.get("AUTOMDEPS")
    if not automdeps_root:
        print("warn: perl.exe not found on PATH and AUTOMDEPS is not set; cannot bootstrap OpenSSL on Windows")
        return None

    perl_root = os.path.join(automdeps_root, "perl")
    existing = find_existing_perl(perl_root)
    if existing:
        return existing

    os.makedirs(perl_root, exist_ok=True)
    perl_zip = os.path.join(automdeps_root, "strawberry-perl-portable.zip")
    perl_url = os.environ.get(
        "AUTOMDEPS_PERL_URL",
        "https://strawberryperl.com/download/5.38.2.2/strawberry-perl-5.38.2.2-64bit-portable.zip",
    )

    print(f"info: downloading portable perl from {perl_url}")
    urllib.request.urlretrieve(perl_url, perl_zip)
    print(f"info: extracting portable perl to {perl_root}")
    with zipfile.ZipFile(perl_zip, "r") as archive:
        archive.extractall(perl_root)

    installed = find_existing_perl(perl_root)
    if installed:
        return installed

    print("warn: perl download/extract completed but perl.exe was not found under AUTOMDEPS")
    return None

def bootstrap_openssl(dst_path):
    configure = os.path.join(dst_path, "Configure")
    if not os.path.isfile(configure):
        print(f"warn: missing OpenSSL Configure script at {configure}")
        return

    out_dir = os.path.join(dst_path, "out")
    if os.name == "nt":
        include_marker = os.path.join(out_dir, "include", "openssl", "opensslv.h")
        lib_marker = os.path.join(out_dir, "lib", "libcrypto.lib")
        if os.path.isfile(include_marker) and os.path.isfile(lib_marker):
            print("skip: deps/openssl/out already bootstrapped")
            return

        perl = shutil.which("perl")
        if not perl:
            perl = bootstrap_perl_from_automdeps()
        if not perl:
            return

        openssl_target = os.environ.get("OPENSSL_WINDOWS_TARGET", "VC-WIN64A")
        if not run_cmd([perl, "Configure", openssl_target, "--prefix=" + os.path.abspath(out_dir), "no-shared", "no-tests"], cwd=dst_path):
            return
        if not run_cmd(["nmake", "/nologo"], cwd=dst_path):
            return
        run_cmd(["nmake", "/nologo", "install_sw"], cwd=dst_path)
        return

    include_marker = os.path.join(out_dir, "include", "openssl", "opensslv.h")
    lib_marker = os.path.join(out_dir, "lib", "libcrypto.a")
    lib64_marker = os.path.join(out_dir, "lib64", "libcrypto.a")
    if os.path.isfile(include_marker) and (os.path.isfile(lib_marker) or os.path.isfile(lib64_marker)):
        print("skip: deps/openssl/out already bootstrapped")
        return

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
