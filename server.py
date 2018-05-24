# This is an extremely *extremely* stripped down version of the
# server. It should give you a rough idea of what it's supposed to
# do. But note that it doesn't include a ton of the spec: it doesn't
# check for any of the security features. It incorrectly handles MIME
# types. Etc... There are a ton of things it doesn't do, but this is
# the basic idea. Perhaps it will guide you (or at least convince you
# that the high-level idea isn't really that hard)
# 
# Run this with..
# export FLASK_APP=hello.py
# python -m flask run
import requests
from flask import Flask
from flask import request
from flask import make_response

app = Flask(__name__)
@app.route('/<path:path>', methods=["GET","POST","PUT","DELETE"])
def path(path):
  if request.method == "GET":
    print('Requesting path: %s' % path)
    # Open the file with "read" and "binary
    # Then turn it into a response using the response API
    response = make_response(open("./files/" + path,"rb").read())
    # Set some headers
    # Note this is *wrong!!!*
    response.headers.set('Content-Type', 'text/plain')
    return response
  if request.method == "POST":
    # http://flask.pocoo.org/docs/0.12/patterns/fileuploads/
    print('Going to save to %s' % path)
    f = request.files['file']
    f.save("./files/" + path)
    return "200 OK"
  if request.method == "PUT":
    print('Going to save to %s' % path)
    f = request.files['file']
    f.save("./files/" + path)
    return "200 OK"
  if request.method == "DELETE":
    os.remove("./files/"+path)
    return "200 OK"
