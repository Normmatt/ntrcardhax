import sys

with open(sys.argv[1], 'rb') as rf:
    r = rf.read()
    with open(sys.argv[1] + '.out', 'wb') as wf:
        wf.write(r[:0x20000])
