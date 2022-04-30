import os
import sys

def SplitBin(srcFilePath, chunkSize, outDir):
    with open(srcFilePath, 'rb') as fin:
        SplitBinStream(fin, os.path.basename(srcFilePath), chunkSize, outDir)

def SplitBinStream(fin, srcFileName, chunkSize, outDir):
    if not os.path.exists(outDir):
        os.mkdir(outDir)
    i = 0
    while True:
        data = fin.read(chunkSize)
        if not len(data):
            break
        dstFileName = f'{srcFileName}.{i}'
        dstFilePath = os.path.join(outDir, dstFileName)
        WriteData(dstFilePath, data)
        i = i + 1

def WriteData(outFilePath, data):
    with open(outFilePath, 'wb') as fout:
        fout.write(data)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("py split.py [srcFilePath] [outDir]")
        sys.exit(0)

    SplitBin(sys.argv[1], 50*1024*1024, sys.argv[2])
