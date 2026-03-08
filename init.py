import os
import platform
import subprocess
import shutil
import urllib.request
import zipfile

def git_lib(repo_url, dst_path, branch=None):
    preserved_files = {}
    if os.path.isdir(dst_path):
        entries = [entry for entry in os.listdir(dst_path) if entry != ".DS_Store"]
        if os.path.isdir(os.path.join(dst_path, ".git")):
            print(f"skip: {dst_path} already exists")
            return
        if not entries or entries == ["README.bootstrap"]:
            if "README.bootstrap" in entries:
                placeholder = os.path.join(dst_path, "README.bootstrap")
                with open(placeholder, "r", encoding="utf-8") as handle:
                    preserved_files["README.bootstrap"] = handle.read()
            shutil.rmtree(dst_path)
        else:
            print(f"skip: {dst_path} already exists")
            return
    try:
        cmd = ["git", "clone", "--depth", "1"]
        if branch:
            cmd.extend(["--branch", branch, "--single-branch"])
        cmd.extend([repo_url, dst_path])
        subprocess.check_call(cmd)
        for rel_path, content in preserved_files.items():
            abs_path = os.path.join(dst_path, rel_path)
            os.makedirs(os.path.dirname(abs_path), exist_ok=True)
            with open(abs_path, "w", encoding="utf-8") as handle:
                handle.write(content)
    except subprocess.CalledProcessError as exc:
        if preserved_files and not os.path.isdir(dst_path):
            os.makedirs(dst_path, exist_ok=True)
        for rel_path, content in preserved_files.items():
            abs_path = os.path.join(dst_path, rel_path)
            os.makedirs(os.path.dirname(abs_path), exist_ok=True)
            with open(abs_path, "w", encoding="utf-8") as handle:
                handle.write(content)
        print(f"warn: failed to clone {repo_url} -> {dst_path} ({exc})")

def run_cmd(cmd, cwd, env=None):
    try:
        subprocess.check_call(cmd, cwd=cwd, env=env)
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

def append_flag_list(existing, extra):
    values = existing.split()
    for flag in extra.split():
        if flag not in values:
            values.append(flag)
    return " ".join(values).strip()

def find_icu_source_dir(dst_path):
    candidates = [
        os.path.join(dst_path, "icu4c", "source"),
        os.path.join(dst_path, "source"),
    ]
    for candidate in candidates:
        if os.path.isfile(os.path.join(candidate, "runConfigureICU")):
            return candidate
    return None

def icu_bootstrap_complete(out_dir):
    include_marker = os.path.join(out_dir, "include", "unicode", "utypes.h")
    if not os.path.isfile(include_marker):
        return False
    lib_dirs = [
        os.path.join(out_dir, "lib"),
        os.path.join(out_dir, "lib64"),
    ]
    uc_names = ("libicuuc.a", "libicuuc.dylib", "libicuuc.so", "icuuc.lib")
    i18n_names = ("libicui18n.a", "libicui18n.dylib", "libicui18n.so", "icuin.lib", "icui18n.lib")
    for lib_dir in lib_dirs:
        has_uc = any(os.path.isfile(os.path.join(lib_dir, name)) for name in uc_names)
        has_i18n = any(os.path.isfile(os.path.join(lib_dir, name)) for name in i18n_names)
        if has_uc and has_i18n:
            return True
    return False

def default_icu_configure_target():
    if os.name == "nt":
        return None
    system = platform.system()
    if system == "Darwin":
        return "MacOSX/GCC"
    if system == "Linux":
        return "Linux"
    return None

def bootstrap_icu(dst_path):
    dst_path = os.path.abspath(dst_path)
    out_dir = os.path.join(dst_path, "out")
    if icu_bootstrap_complete(out_dir):
        print("skip: deps/icu/out already bootstrapped")
        return

    source_dir = find_icu_source_dir(dst_path)
    if not source_dir:
        print(f"warn: missing ICU source tree under {dst_path}")
        return

    if os.name == "nt":
        print("warn: automatic ICU bootstrap is not implemented on Windows; build ICU manually under deps/icu/out")
        return

    configure_target = os.environ.get("ICU_CONFIGURE_TARGET", default_icu_configure_target() or "")
    if not configure_target:
        print(f"warn: unsupported platform for automatic ICU bootstrap ({platform.system()}); set ICU_CONFIGURE_TARGET to override")
        return

    build_dir = os.path.join(dst_path, "build-starbytes")
    os.makedirs(build_dir, exist_ok=True)

    env = os.environ.copy()
    env["CFLAGS"] = append_flag_list(env.get("CFLAGS", ""), "-fPIC")
    env["CXXFLAGS"] = append_flag_list(env.get("CXXFLAGS", ""), "-fPIC")

    configure_cmd = [
        os.path.join(source_dir, "runConfigureICU"),
        configure_target,
        "--prefix=" + os.path.abspath(out_dir),
        "--enable-static",
        "--disable-shared",
        "--disable-tests",
        "--disable-samples",
        "--with-data-packaging=archive",
    ]

    if not run_cmd(configure_cmd, cwd=build_dir, env=env):
        return

    jobs = str(os.cpu_count() or 4)
    if not run_cmd(["make", "-j", jobs], cwd=build_dir, env=env):
        return
    run_cmd(["make", "install"], cwd=build_dir, env=env)


git_lib("https://github.com/Tencent/rapidjson.git", "deps/rapidjson")
git_lib("https://github.com/PCRE2Project/pcre2.git", "deps/pcre2")
# Track ICU's current stable maintenance branch by default; override with ICU_GIT_BRANCH when advancing.
git_lib("https://github.com/unicode-org/icu.git", "deps/icu", branch=os.environ.get("ICU_GIT_BRANCH", "maint/maint-78"))
git_lib("https://github.com/chriskohlhoff/asio.git", "deps/asio")
git_lib("https://github.com/curl/curl.git", "deps/libcurl")
git_lib("https://github.com/openssl/openssl.git", "deps/openssl")
git_lib("https://github.com/madler/zlib.git", "deps/zlib")
bootstrap_icu("./deps/icu")
bootstrap_openssl("./deps/openssl")
