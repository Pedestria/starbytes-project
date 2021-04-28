#! /usr/bin/python3

import requests
import tarfile
import os

print("Downloading LLVM-Project 12")
stream = requests.get("https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/llvm-12.0.0.src.tar.xz")

with open("./llvm-project.tar.xz","wb") as file:
    for chunk in stream.iter_content(chunk_size = 1024):
	    if chunk:
		    file.write(chunk)

print("Extracting LLVM-Project 12 to \"./llvm-project\"")
z = tarfile.open("./llvm-project.tar.xz","r:xz")
z.extractall("./llvm-project")
z.close()
os.remove("./llvm-project.tar.xz")
