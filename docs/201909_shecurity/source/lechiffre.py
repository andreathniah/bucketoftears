import string

def encrypt(plaintext):
    key = 'shecurity'
    ciphertext = ""
    i = 0
    a = string.ascii_lowercase
    for p in plaintext:
        if p in a:
            m1 = a.index(p)
            k1 = a.index(key[i % len(key)])
            c1 = (m1 + k1) % 26
            c = a[c1]
            i += 1
        else:
            c = p
        ciphertext += c
    return ciphertext

def main():
    flag = '{xsei-nym_cmq_vj_elpxmm}'
    guess = input("Enter flag: ")
    if encrypt(guess) == flag:
        print("Correct! Good job.")
    else:
        print("Incorrect.")

if __name__ == "__main__":
    main()