# Use to compile multiple samples of the io bounded or cpu bounded programs
# Usages:
# python compile_aux_programs.py iobounded.c
# python compile_aux_programs.py cpubounded.c

import sys
import os

program_template = open(sys.argv[1], 'r').read()
os.system("mkdir -p ./tmp")
for i in range(1, 11):
  source = program_template.replace("::NUMBER::", str(i))
  f = open("./tmp/program.c", "w")
  f.write(source)
  f.close()
  os.system("gcc ./tmp/program.c -o " + os.path.basename(sys.argv[1])[:-2] + str(i))
