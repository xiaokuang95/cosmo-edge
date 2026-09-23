#!/usr/bin/env python3
# RTSP server: relay every active Cosmo OSD stream (FLV over HTTP) as RTSP /live/<name>.
# Mounts follow SRS streams, polled every 5s.
import json
import urllib.request
import gi

gi.require_version('Gst', '1.0')
gi.require_version('GstRtspServer', '1.0')
from gi.repository import Gst, GstRtspServer, GLib

SRS_API = 'http://127.0.0.1:1985/api/v1/streams/'


def srs_streams():
    with urllib.request.urlopen(SRS_API, timeout=3) as r:
        d = json.loads(r.read())
    return [s['name'] for s in d.get('streams', [])
            if s.get('publish', {}).get('active')]


class Server(GstRtspServer.RTSPServer):
    def __init__(self):
        super().__init__()
        self.props.service = '554'
        self.mounts = self.get_mount_points()
        self.live = {}
        self.attach(GLib.MainContext.default())
        self.sync_mounts()
        GLib.timeout_add_seconds(5, self.sync_mounts)

    def sync_mounts(self):
        try:
            names = set(srs_streams())
        except Exception as e:
            print('srs poll failed:', e, flush=True)
            return True
        for name in names:
            path = '/live/' + name
            if path in self.live:
                continue
            factory = GstRtspServer.RTSPMediaFactory()
            factory.set_launch(
                'souphttpsrc location=http://127.0.0.1:18088/live/{}.flv '
                'is-live=true do-timestamp=true ! flvdemux ! h264parse ! '
                'rtph264pay name=pay0 pt=96'.format(name))
            factory.set_shared(True)
            self.mounts.add_factory(path, factory)
            self.live[path] = True
            print('mount added:', path, flush=True)
        for path in list(self.live):
            if path[len('/live/'):] not in names:
                self.mounts.remove_factory(path)
                del self.live[path]
                print('mount removed:', path, flush=True)
        return True


def main():
    Gst.init(None)
    Server()
    print('RTSP serving rtsp://0.0.0.0:554/live/<stream>', flush=True)
    GLib.MainLoop().run()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
