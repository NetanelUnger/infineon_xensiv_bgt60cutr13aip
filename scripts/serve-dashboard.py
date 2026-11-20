"""Serve the dashboard only on loopback. No internet service or data upload."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import webbrowser

parser=argparse.ArgumentParser();parser.add_argument('--open',action='store_true');parser.add_argument('--port',type=int,default=8765)
args=parser.parse_args()
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self' ws: wss:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        super().end_headers()
    extensions_map={**SimpleHTTPRequestHandler.extensions_map,'.js':'text/javascript'}
root=Path(__file__).resolve().parents[1]/'web'
server=ThreadingHTTPServer(('127.0.0.1',args.port),partial(Handler,directory=str(root)))
url=f'http://localhost:{args.port}'
print(f'Radar dashboard: {url}',flush=True)
if args.open:webbrowser.open(url)
try:server.serve_forever()
except KeyboardInterrupt:pass
finally:server.server_close()
