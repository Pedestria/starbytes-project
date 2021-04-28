#! /usr/bin/python3

import os 

llvm_dir = "./llvm-project/llvm-12.0.0.src"

os.system(f"vc.bat && cmake -S {llvm_dir} -B {llvm_dir}/build -G\"Ninja\" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_ASM_COMPILER=nasm")
os.system(f"vc.bat && cmake --build {llvm_dir}/build")

llvm_include_dir = f"{llvm_dir}/include"
os.system(f"cmake -S ./starbytes-lang -B ./build -DLLVM_DIR=\"{llvm_dir}\" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++")
os.system(f"cmake --build ./build")
