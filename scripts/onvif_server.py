#!/usr/bin/env python3
# Minimal ONVIF device+media service: enough for NVR "ONVIF protocol" add.
# SOAP on :8899 (any path), WS-Discovery on UDP :3702.
# ponytail: profiles are read live from SRS, one per OSD stream; auth accepted-but-ignored.
import re
import json
import time
import uuid
import socket
import threading
import subprocess
import urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HTTP_PORT = 8899
RTSP_PORT = 554
SRS_API = 'http://127.0.0.1:1985/api/v1/streams/'

NS = ('xmlns:tds="http://www.onvif.org/ver10/device/wsdl" '
      'xmlns:tt="http://www.onvif.org/ver10/schema" '
      'xmlns:trt="http://www.onvif.org/ver10/media/wsdl"')

_ip_cache = (None, 0)


def board_ip():
    global _ip_cache
    if _ip_cache[0] and time.time() - _ip_cache[1] < 10:
        return _ip_cache[0]
    ip = '127.0.0.1'
    try:
        out = subprocess.check_output(['ip', '-4', 'route', 'get', '1.1.1.1'],
                                       text=True, timeout=3)
        m = re.search(r'src (\d+\.\d+\.\d+\.\d+)', out)
        if m:
            ip = m.group(1)
    except Exception:
        pass
    _ip_cache = (ip, time.time())
    return ip


def srs_streams():
    """Active OSD stream names, e.g. ['RT0000000000_92113']."""
    with urllib.request.urlopen(SRS_API, timeout=3) as r:
        d = json.loads(r.read())
    return [s['name'] for s in d.get('streams', [])
            if s.get('publish', {}).get('active')]


def serial_number():
    try:
        with open('/proc/device-tree/serial-number') as f:
            return f.read().strip('\x00\n') or 'RK3576'
    except Exception:
        return 'RK3576'


def xml_escape(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def profile_xml(name):
    return (
        f'<trt:Profiles token="{name}" fixed="true">'
        f'<tt:Name>{xml_escape(name)}</tt:Name>'
        '<tt:VideoSourceConfiguration token="VSC_1"><tt:Name>VSC_1</tt:Name>'
        '<tt:SourceToken>VSrc_1</tt:SourceToken>'
        '<tt:Bounds x="0" y="0" width="1280" height="720"/>'
        '</tt:VideoSourceConfiguration>'
        '<tt:VideoEncoderConfiguration token="VEC_1"><tt:Name>VEC_1</tt:Name>'
        '<tt:Encoding>H264</tt:Encoding>'
        '<tt:Resolution><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:Resolution>'
        '<tt:Quality>5</tt:Quality>'
        '<tt:RateControl><tt:FrameRateLimit>10</tt:FrameRateLimit>'
        '<tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>2048</tt:BitrateLimit>'
        '</tt:RateControl>'
        '<tt:H264><tt:GovLength>25</tt:GovLength><tt:H264Profile>High</tt:H264Profile></tt:H264>'
        '<tt:SessionTimeout>PT60S</tt:SessionTimeout>'
        '</tt:VideoEncoderConfiguration>'
        '</trt:Profiles>'
    )


def vec_xml():
    return ('<trt:GetVideoEncoderConfigurationResponse>'
            '<tt:VideoEncoderConfiguration token="VEC_1"><tt:Name>VEC_1</tt:Name>'
            '<tt:Encoding>H264</tt:Encoding>'
            '<tt:Resolution><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:Resolution>'
            '<tt:Quality>5</tt:Quality>'
            '<tt:RateControl><tt:FrameRateLimit>10</tt:FrameRateLimit>'
            '<tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>2048</tt:BitrateLimit>'
            '</tt:RateControl>'
            '<tt:H264><tt:GovLength>25</tt:GovLength><tt:H264Profile>High</tt:H264Profile></tt:H264>'
            '<tt:SessionTimeout>PT60S</tt:SessionTimeout>'
            '</tt:VideoEncoderConfiguration></trt:GetVideoEncoderConfigurationResponse>')


def vsc_xml(resp_name):
    return (f'<trt:{resp_name}>'
            '<tt:Configurations token="VSC_1"><tt:Name>VSC_1</tt:Name>'
            '<tt:SourceToken>VSrc_1</tt:SourceToken>'
            '<tt:Bounds x="0" y="0" width="1280" height="720"/>'
            '</tt:Configurations></trt:%s>' % resp_name)


def handle_soap(action, body):
    ip = board_ip()
    now = datetime.now(timezone.utc)
    if action == 'GetSystemDateAndTime':
        t = ('<tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>'
             '<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date>'
             % (now.hour, now.minute, now.second, now.year, now.month, now.day))
        return ('<tds:GetSystemDateAndTimeResponse><tt:SystemDateTime>'
                '<tt:DateTimeType>UTC</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>'
                f'<tt:UTCDateTime>{t}</tt:UTCDateTime></tt:SystemDateTime>'
                '</tds:GetSystemDateAndTimeResponse>')
    if action == 'GetServices':
        return ('<tds:GetServicesResponse>'
                '<tds:Service><tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>'
                f'<tds:XAddr>http://{ip}:{HTTP_PORT}/onvif/device_service</tds:XAddr>'
                '<tds:Version><tt:Major>2</tt:Major><tt:Minor>10</tt:Minor></tds:Version></tds:Service>'
                '<tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>'
                f'<tds:XAddr>http://{ip}:{HTTP_PORT}/onvif/media_service</tds:XAddr>'
                '<tds:Version><tt:Major>2</tt:Major><tt:Minor>10</tt:Minor></tds:Version></tds:Service>'
                '</tds:GetServicesResponse>')
    if action == 'GetCapabilities':
        return ('<tds:GetCapabilitiesResponse><tt:Capabilities>'
                '<tt:Media>'
                f'<tt:XAddr>http://{ip}:{HTTP_PORT}/onvif/media_service</tt:XAddr>'
                '<tt:StreamingCapabilities><tt:RTP_TCP tt:bool="true"/>'
                '<tt:RTP_RTSP_TCP tt:bool="true"/><tt:NonAggregateControl tt:bool="false"/>'
                '<tt:NoRTSPStreaming tt:bool="false"/></tt:StreamingCapabilities>'
                '</tt:Media>'
                '<tt:Extension><tt:DeviceIO>'
                f'<tt:XAddr>http://{ip}:{HTTP_PORT}/onvif/device_service</tt:XAddr>'
                '</tt:DeviceIO></tt:Extension>'
                '</tt:Capabilities></tds:GetCapabilitiesResponse>')
    if action == 'GetDeviceInformation':
        return ('<tds:GetDeviceInformationResponse>'
                '<tds:Manufacturer>CosmoEdge</tds:Manufacturer>'
                '<tds:Model>CosmoEdge-OSD</tds:Model>'
                '<tds:FirmwareVersion>1.1.0</tds:FirmwareVersion>'
                f'<tds:SerialNumber>{serial_number()}</tds:SerialNumber>'
                '<tds:HardwareId>RK3576</tds:HardwareId>'
                '</tds:GetDeviceInformationResponse>')
    if action == 'GetProfiles':
        try:
            names = srs_streams()
        except Exception:
            names = []
        profiles = ''.join(profile_xml(n) for n in names)
        return f'<trt:GetProfilesResponse>{profiles}</trt:GetProfilesResponse>'
    if action == 'GetProfile':
        m = re.search(r'token="([^"]+)"', body)
        if m:
            return f'<trt:GetProfileResponse>{profile_xml(m.group(1))}</trt:GetProfileResponse>'
        return fault_xml('Sender', 'ter:InvalidArgVal', 'missing token')
    if action == 'GetStreamUri':
        m = re.search(r'token="([^"]+)"', body)
        token = m.group(1) if m else ''
        try:
            names = srs_streams()
        except Exception:
            names = []
        if token not in names:
            if names:
                token = names[0]
            else:
                return fault_xml('Sender', 'ter:InvalidArgVal', 'no active stream')
        return (f'<trt:GetStreamUriResponse><trt:MediaUri>'
                f'<tt:Uri>rtsp://{ip}:{RTSP_PORT}/live/{token}</tt:Uri>'
                '<tt:InvalidAfterConnect tt:bool="false"/>'
                '<tt:InvalidAfterReboot tt:bool="false"/>'
                '<tt:Timeout>PT60S</tt:Timeout></trt:MediaUri></trt:GetStreamUriResponse>')
    if action == 'GetVideoEncoderConfiguration' or action == 'GetVideoEncoderConfigurations':
        return vec_xml()
    if action == 'GetVideoSourceConfiguration' or action == 'GetVideoSourceConfigurations':
        return vsc_xml('GetVideoSourceConfigurationResponse' if action.endswith('Configuration')
                       else 'GetVideoSourceConfigurationsResponse')
    if action == 'GetServiceCapabilities':
        return ('<trt:GetServiceCapabilitiesResponse><trt:Capabilities SnapshotUri="false" '
                'Rotation="false" VideoSourceMode="false" OSD="false" ProfileCapabilities="true" '
                '/></trt:GetServiceCapabilitiesResponse>')
    if action == 'GetHostname':
        return ('<tds:GetHostnameResponse><tt:Hostname>cosmo-edge</tt:Hostname>'
                '</tds:GetHostnameResponse>')
    if action == 'GetNetworkInterfaces' or action == 'GetNetworkProtocols':
        return (f'<tds:Get{action[3:]}Response></tds:Get{action[3:]}Response>')
    return fault_xml('Sender', 'ter:ActionNotSupported',
                     f'action {action} not supported')


def fault_xml(code, subcode, text):
    return ('<s:Fault><s:Code><s:Value>s:%s</s:Value><s:Subcode><s:Value>%s</s:Value>'
            '</s:Subcode></s:Code><s:Reason><s:Text xml:lang="en">%s</s:Text></s:Reason></s:Fault>'
            % (code, subcode, xml_escape(text)))


class OnvifHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, fmt, *args):
        print('%s %s' % (self.address_string(), fmt % args), flush=True)

    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length).decode('utf-8', 'replace') if length else ''
        action = None
        m = re.search(r'<[A-Za-z0-9]+:Action[^>]*>([^<]+)</[A-Za-z0-9]+:Action>', body)
        if m:
            action = m.group(1).rsplit('/', 1)[-1].rsplit(':', 1)[-1]
        if not action:
            sa = self.headers.get('SOAPAction', '')
            if sa:
                action = sa.strip('"\'').rsplit('/', 1)[-1].rsplit(':', 1)[-1]
        if not action:
            m = re.search(r'Body[^>]*>\s*<(?:[A-Za-z0-9_]+:)?([A-Za-z][A-Za-z0-9_]*)', body)
            if m:
                action = m.group(1)
        action = action or ''
        payload = handle_soap(action, body)
        resp = ('<?xml version="1.0" encoding="UTF-8"?>'
                f'<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" {NS}>'
                f'<s:Body>{payload}</s:Body></s:Envelope>')
        ctype = 'application/soap+xml; charset=utf-8'
        if (self.headers.get('Content-Type') or '').startswith('text/xml'):
            ctype = 'text/xml; charset=utf-8'
        data = resp.encode()
        self.send_response(200)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def discovery_loop():
    mcast = '239.255.255.250'
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('', 3702))
    mreq = socket.inet_aton(mcast) + socket.inet_aton('0.0.0.0')
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    while True:
        try:
            data, addr = s.recvfrom(65535)
        except OSError:
            continue
        if b'Probe' not in data or b'MessageID' not in data:
            continue
        m = re.search(rb'<[A-Za-z0-9]+:MessageID[^>]*>([^<]+)<', data)
        mid = m.group(1).decode() if m else 'urn:uuid:0'
        ip = board_ip()
        reply = (
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" '
            'xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" '
            'xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery" '
            'xmlns:dn="http://www.onvif.org/ver10/network/wsdl" '
            f'xmlns:tds="http://www.onvif.org/ver10/device/wsdl">'
            '<soap:Header>'
            f'<a:MessageID>urn:uuid:{uuid.uuid4()}</a:MessageID>'
            f'<a:RelatesTo>{xml_escape(mid)}</a:RelatesTo>'
            '<a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:To>'
            '<a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</a:Action>'
            '<d:AppSequence InstanceId="1" MessageNumber="1"/>'
            '</soap:Header><soap:Body><d:ProbeMatches><d:ProbeMatch>'
            '<d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>'
            '<d:Scopes>onvif://www.onvif.org/Profile/Streaming '
            'onvif://www.onvif.org/name/CosmoEdge onvif://www.onvif.org/hardware/RK3576</d:Scopes>'
            f'<d:XAddrs>http://{ip}:{HTTP_PORT}/onvif/device_service</d:XAddrs>'
            '<d:MetadataVersion>1</d:MetadataVersion>'
            '</d:ProbeMatch></d:ProbeMatches></soap:Body></soap:Envelope>'
        )
        s.sendto(reply.encode(), addr)


def main():
    threading.Thread(target=discovery_loop, daemon=True).start()
    print(f'ONVIF serving http://0.0.0.0:{HTTP_PORT} (any path) + WS-Discovery :3702', flush=True)
    ThreadingHTTPServer(('0.0.0.0', HTTP_PORT), OnvifHandler).serve_forever()


if __name__ == '__main__':
    main()
