import os
import win32api
from random import Random

def random_str():
	randomlength = 64
	str=''
	chars  = 'abcdefghijklmnopqrstuvwxyz0123456'
	length = len(chars) - 1
	random = Random()
	for i in range(randomlength):
		str+=chars[random.randint(0, length)]
	return str


files=os.listdir('../sample/test')
for file in files:
	print("Deal with file:"+file)
	for i in range(10):
		random_name=random_str()
		os.system(".\demo.exe "+"../sample/test/"+file+" -o ../sample/aftertest/"+random_name)
	print("The file:"+file+" has been processed 10 times.\n")

print("\nMISSION OVER!")