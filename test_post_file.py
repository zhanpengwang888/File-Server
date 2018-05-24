import os
import requests
import filecmp
from io import BytesIO

# Or 8080
port = "5000"

def test_post_file(path_to_file,filename):
    files = {'file': open(path_to_file, 'rb')}
    r = requests.post("http://localhost:" + port + "/" + filename, files=files)
    if (r.ok):
        print("POST returned OK result")
    else:
        print("POST result not OK")
        exit(1)
    print("Now getting it back")
    r = requests.get("http://localhost:" + port + "/" + filename)
    if (r.ok):
        print("Test returned OK result")
    else:
        print("Result not OK")
        exit(1)
    open("output", "wb").write(r.content)
    if filecmp.cmp('output', path_to_file):
        print("File is the same (passed): ", filename)
    else:
        print("File is different (failed), result in output ", filename)
        exit(1)

pairs = [["example-files/sample.pdf", "examplepdf.pdf"]]

for pair in pairs:
    test_post_file(pair[0], pair[1])
