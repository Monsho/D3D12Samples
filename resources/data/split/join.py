import os
import sys

def JoinBin(srcFilePath, outDir):
    fileName = os.path.basename(srcFilePath)
    outFilePath = os.path.join(outDir, fileName)
    with open(outFilePath, 'wb') as fout:
        i = 0
        while True:
            splitFilePath = f'{srcFilePath}.{i}'
            if not os.path.exists(splitFilePath):
                break
            with open(splitFilePath, 'rb') as fin:
                fout.write(fin.read())
            i = i + 1


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("py join.py [srcFilePath] [outDir]")
        sys.exit(0)

    JoinBin(sys.argv[1], sys.argv[2])
