def encode(s):
    alpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/"
    r = ""
    p = ""
    c = len(s)%3
    if c > 0:
        while c < 3:
            p += '='
            s += '\0'
            c += 1
    for i in range(0,len(s),3):
        n = (ord(s[i]) << 16) + (ord(s[i+1]) << 8) + ord(s[i+2])
        n = [(n >> 18) & 63, (n >> 12)&63, (n>>6 & 63), n&63]
        r += alpha[n[0]] + alpha[n[1]] + alpha[n[2]] + alpha[n[3]]
    return r[:len(r) - len(p)] + p

def decode(s):
    raise NotImplementedError

def main():
    flag = 'E2zSywCTCxvPDgvFDgHLx2fSCgHHyMv0Fq=='
    guess = input("Enter flag: ")
    if encode(guess) == flag:
        print("Correct! Good job.")
    else:
        print("Incorrect.")

if __name__ == "__main__":
    main()