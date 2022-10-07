#!/usr/bin/python
import argparse
import re
import os
import sys

def parse_api( filename ):
    with open( filename, 'r' ) as f:
        data = f.read()

    result = re.findall("static const luaL_Reg.*?\[\] = {(.+?)};", data, re.S)
    assert( len(result)==1 )
    result = re.findall( "\".*\"", result[0] )
    funclist = []
    for r in result:
        funclist += [r.replace("\"","")]

    return funclist

def generate_api( modname, filename ):
    api = parse_api( filename )
    api.sort()

    apilist = ""
    for a in api:
        apilist += f"            {a} = {{}},\n"

    return f"""stds.{modname} = {{
   read_globals = {{
      {modname} = {{
         fields = {{
{apilist}         }},
      }},
   }}
}}"""

if __name__ == "__main__":
    parser = argparse.ArgumentParser( description='Generates API for luacheckrc from a C source file.' )
    parser.add_argument('filename', metavar='FILENAME', nargs='+', type=str, help='Name of the file name to parse.')
    parser.add_argument('--output', default=None, help="Name of the output file.")
    args = parser.parse_args()

    if not args.output:
        output = sys.stdout
    else:
        output = open(args.output,'w')

    for filename in args.filename:
        modname = os.path.basename(filename).replace('nlua_','').replace('.c','')
        output.write( generate_api( modname, filename )+'\n' )

#api = generate_api( "news", "/home/ess/naev/src/nlua_news.c" )
