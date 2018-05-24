from subprocess import call

tests = ["test_get.py",
         "test_get_harder.py",
         "test_post_file.py",
         "test_put_file.py"]

print("Running all tests now")

passed = 0

for test in tests:
    print("Running test %s" % test)
    if (call(["python", test]) != 1):
        passed += 1
        print("Test %s PASSED! : )" % test)
    else:
        print("Test %s FAILED : (" % test)

print("%d out of %d tests passed" % (passed, len(tests)))
