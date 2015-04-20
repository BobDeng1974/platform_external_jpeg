#!/usr/bin/python
def analysis(filename):
    c = ''
    with open(filename) as f:
        lines = f.read().split('\n')
        for l in lines:
            if (len(l) < 1):
                continue
            c += '\"' + l + '\\n\"' + '\n'
    return c
def main():
    import os
    cmd = 'find . -name \"*.cl\"'
    lines = os.popen(cmd).read().split('\n')
    filecontents = ""
    for l in lines:
        if len(l) <=1:
            continue
        contents = analysis(l)
        name = l.split('.cl')[0].split('/')
        name = name[len(name)-1] + '_clclh'
        filecontents += 'const char* ' + name + ' = \n'
        filecontents += contents
        filecontents += ';\n'
    with open('KernelWarp.h', 'w') as f:
        f.write(filecontents);

if __name__ == '__main__':
    main()
