#!/bin/env python3

# Rudimentary test wrapper for SDXI samples

import subprocess

TESTS = [
        ("./context", "create a context"),
        ("./uadd",    "atomic add"),
        ("./write_imm",    "write immediate data via descriptor"),
        ("./memcopy", "memory copy"),
        ("./repcopy", "repeated memory copy")
]

def main():
    test_index = 1
    test_count = len(TESTS)

    print("%d..%d" % (test_index, test_count))
    for pair in TESTS:
        try:
            result = subprocess.run([pair[0]],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except FileNotFoundError:
            print("Please run this script in the samples directory after a successful build")
            return

        if result.returncode != 0:
            result_string = "not ok"
        else:
            result_string = "ok"

        print("%s %d - %s" % (result_string, test_index, pair[1]))
        test_index +=1


if __name__ == "__main__":
    main()
