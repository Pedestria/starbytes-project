#! /usr/bin/python3

import requests
import tarfile
import os
import sys
import time

print("Downloading LLVM-Project 12",flush=True)
# stream = requests.get("https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/llvm-12.0.0.src.tar.xz")


# class ProgressBar:
# 	currentIdx:int
# 	len:int
# 	file = sys.stdout
# 	def __init__(self,len:int):
# 		self.currentIdx = 0
# 		self.len = len
# 		self.file.write("[")
# 		for i in range(0,len):
# 			sys.stdout.write(" ")
# 		self.file.write("]")
# 		self.file.write("\n")
# 		self.file.write(f"0/{len}")
# 	def add(self):
# 		self.currentIdx += 1
# 		self.file.write("\r")
# 		self.file.flush()
# 		self.file.write("[")
# 		for i in range(0,self.currentIdx):
# 			self.file.write("#")
# 		for i in range(0,self.len - self.currentIdx):
# 			self.file.write(" ")
# 		self.file.write("]")
# 		self.file.write("\n")
# 		self.file.write(f"{self.currentIdx}/{self.len}")
# 		return
# 	def finish(self):
# 		self.file.write("\n")
# 		self.file.flush()
# 		return
# bar = ProgressBar(8)
# for i in range(0,8):
# 	bar.add()
# 	time.sleep(0.005)
# bar.finish()

# with open("./llvm-project.tar.xz","wb") as file:
# 	l = len(stream.content)
# 	for chunk in stream.iter_content(chunk_size = 1024):
# 	    if chunk:
# 		    file.write(chunk) 
			


# print("Extracting LLVM-Project 12 to \"./llvm-project\"")
# z = tarfile.open("./llvm-project.tar.xz","r:xz")
# zmems = z.getmembers()
# zbar = ProgressBar(len(zmems))
# for mem in zmems:
# 	z.extract(mem,"./llvm-project")
# z.close()
# os.remove("./llvm-project.tar.xz")
