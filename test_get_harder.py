# Basic test that checks for `index.html` and `catgif.gif`
import os
import requests
import filecmp
from io import BytesIO

## Index.html
def test_get_file(name, location):
    r = requests.get("http://localhost:8080/" + name)
    if (r.ok):
        print("Test returned OK result")
    else:
        print("Result not OK")
        exit(1)
    open("output", "wb").write(r.content)
    if filecmp.cmp('output', location):
        print("File is the same (passed): ", name)
    else:
        print("File is different (failed), result in output ", name)
        exit(1)

pairs = [["index.html", "files/index.html"], ["catgif.gif", "files/catgif.gif"],
         ["demo.docx", "files/demo.docx"], ["sample.pdf", "files/sample.pdf"],
         ["example.mp4", "files/example.mp4"]]

for pair in pairs:
    test_get_file(pair[0], pair[1])

exit(0)
