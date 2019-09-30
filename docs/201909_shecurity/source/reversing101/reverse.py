with open("flag.jpg", "rb") as file:
    bytes_rev = b""
    bytes_read = bytearray(file.read())

    while bytes_read:
        bytes_rev += bytes_read[::-1] # all items in the array, reversed
        bytes_read = file.read()
        
    with open("reversed.jpg", "wb") as newfile:
        newfile.write(bytes_rev)
