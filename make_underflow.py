import struct

with open("repro.orb", "wb") as f:
    f.write(b'ORB1')  # header
    f.write(struct.pack('<i', 0)) # constCount
    f.write(struct.pack('<i', 0)) # mainIndex
    f.write(struct.pack('<i', 1)) # funcCount

    # function 0
    name = b'main'
    f.write(struct.pack('<i', len(name)))
    f.write(name)
    f.write(struct.pack('<ii', 0, 0)) # arity, localCount
    f.write(struct.pack('<i', 1)) # codeCount
    f.write(struct.pack('<Bi', 5, 0)) # OP_POP (5), arg 0
