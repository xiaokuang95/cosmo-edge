#!/usr/bin/env python3
# Minimal ONVIF Profile S: enough for Hikvision NVR add-by-IP.
# SOAP :8899 (any path), WS-Discovery UDP :3702.
# ponytail: profiles follow SRS; if SRS empty still advertise one placeholder
# so the NVR can add the device. Auth accepted-but-ignored.
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
PLACEHOLDER = 'RT0000000000_92113'

NS = ('xmlns:tds="http://www.onvif.org/ver10/device/wsdl" '
      'xmlns:tt="http://www.onvif.org/ver10/schema" '
      'xmlns:trt="http://www.onvif.org/ver10/media/wsdl"')

_ip_cache = (None, 0)


def board_ip():
    global _ip_cache
    if _ip_cache[0] and time.time() - _ip_cache[1] < 10:
        return _ip_cache[0]
    ip = ''
    try:
        out = subprocess.check_output(
            ['ip', '-4', 'route', 'get', '1.1.1.1'], text=True, timeout=3)
        m = re.search(r'src (\d+\.\d+\.\d+\.\d+)', out)
        if m and not m.group(1).startswith('127.'):
            ip = m.group(1)
    except Exception:
        pass
    if not ip:
        try:
            for cand in subprocess.check_output(
                    ['hostname', '-I'], text=True, timeout=2).split():
                if cand and not cand.startswith('127.') and ':' not in cand:
                    ip = cand
                    break
        except Exception:
            pass
    ip = ip or '127.0.0.1'
    _ip_cache = (ip, time.time())
    return ip


def srs_streams():
    """Active OSD stream names, e.g. ['RT0000000000_92113']."""
    with urllib.request.urlopen(SRS_API, timeout=3) as r:
        d = json.loads(r.read())
    return [s['name'] for s in d.get('streams', [])
            if s.get('publish', {}).get('active')]


def stream_names():
    try:
        names = srs_streams()
    except Exception as e:
        print('srs poll failed:', e, flush=True)
        names = []
    return names or [PLACEHOLDER]


def serial_number():
    try:
        with open('/proc/device-tree/serial-number') as f:
            return f.read().strip('\x00\n') or 'RK3576'
    except Exception:
        return 'RK3576'


def xml_escape(s):
    return (s.replace('&', '&amp;').replace('<', '&lt;')
             .replace('>', '&gt;').replace('"', '&quot;'))


def profile_token(body):
    m = re.search(r'<(?:[\w]+:)?ProfileToken[^>]*>([^<]+)</', body)
    if m:
        return m.group(1).strip()
    m = re.search(r'\btoken="([^"]+)"', body)
    return m.group(1).strip() if m else ''


def profile_xml(name):
    n = xml_escape(name)
    return (
        f'<trt:Profiles token="{n}" fixed="true">'
        f'<tt:Name>{n}</tt:Name>'
        '<tt:VideoSourceConfiguration token="VSC_1">'
        '<tt:Name>VSC_1</tt:Name><tt:UseCount>1</tt:UseCount>'
        '<tt:SourceToken>VSrc_1</tt:SourceToken>'
        '<tt:Bounds x="0" y="0" width="1280" height="720"/>'
        '</tt:VideoSourceConfiguration>'
        '<tt:VideoEncoderConfiguration token="vec">'
        '<tt:Name>VEC</tt:Name><tt:UseCount>1</tt:UseCount>'
        '<tt:Encoding>H264</tt:Encoding>'
        '<tt:Resolution><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:Resolution>'
        '<tt:Quality>5</tt:Quality>'
        '<tt:RateControl><tt:FrameRateLimit>10</tt:FrameRateLimit>'
        '<tt:EncodingInterval>1</tt:EncodingInterval>'
        '<tt:BitrateLimit>2048</tt:BitrateLimit></tt:RateControl>'
        '<tt:H264><tt:GovLength>25</tt:GovLength>'
        '<tt:H264Profile>Main</tt:H264Profile></tt:H264>'
        '<tt:Multicast><tt:Address><tt:Type>IPv4</tt:Type>'
        '<tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address>'
        '<tt:Port>0</tt:Port><tt:TTL>1</tt:TTL>'
        '<tt:AutoStart>false</tt:AutoStart></tt:Multicast>'
        '<tt:SessionTimeout>PT60S</tt:SessionTimeout>'
        '</tt:VideoEncoderConfiguration>'
        '</trt:Profiles>'
    )


def vec_xml():
    return ('<trt:GetVideoEncoderConfigurationResponse>'
            '<trt:Configuration token="vec">'
            '<tt:Name>VEC</tt:Name><tt:UseCount>1</tt:UseCount>'
            '<tt:Encoding>H264</tt:Encoding>'
            '<tt:Resolution><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:Resolution>'
            '<tt:Quality>5</tt:Quality>'
            '<tt:RateControl><tt:FrameRateLimit>10</tt:FrameRateLimit>'
            '<tt:EncodingInterval>1</tt:EncodingInterval>'
            '<tt:BitrateLimit>2048</tt:BitrateLimit></tt:RateControl>'
            '<tt:H264><tt:GovLength>25</tt:GovLength>'
            '<tt:H264Profile>Main</tt:H264Profile></tt:H264>'
            '<tt:SessionTimeout>PT60S</tt:SessionTimeout>'
            '</trt:Configuration></trt:GetVideoEncoderConfigurationResponse>')


def public_ip(host_header=''):
    # Hikvision DVR Host is often just the IP (no port). Use that so
    # GetStreamUri is on the same subnet the NVR used to reach us.
    if host_header:
        h = host_header.split(',')[0].strip()
        h = h[1:].split(']')[0] if h.startswith('[') else h.split(':')[0]
        if h and not h.startswith('127.') and h.lower() != 'localhost':
            return h
    return board_ip()


def handle_soap(action, body, host_header=''):
    ip = public_ip(host_header)
    now = datetime.now(timezone.utc)
    if action == 'GetSystemDateAndTime':
        t = ('<tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute>'
             '<tt:Second>%d</tt:Second></tt:Time>'
             '<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month>'
             '<tt:Day>%d</tt:Day></tt:Date>'
             % (now.hour, now.minute, now.second, now.year, now.month, now.day))
        return ('<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime>'
                '<tt:DateTimeType>NTP</tt:DateTimeType>'
                '<tt:DaylightSavings>false</tt:DaylightSavings>'
                '<tt:TimeZone><tt:TZ>CST-8</tt:TZ></tt:TimeZone>'
                f'<tt:UTCDateTime>{t}</tt:UTCDateTime>'
                f'<tt:LocalDateTime>{t}</tt:LocalDateTime>'
                '</tds:SystemDateAndTime></tds:GetSystemDateAndTimeResponse>')
    if action == 'GetServices':
        return ('<tds:GetServicesResponse>'
                '<tds:Service>'
                '<tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>'
                f'<tds:XAddr>http://{ip}:{HTTP_PORT}/onvif/device_service</tds:XAddr>'
                '<tds:Version><tt:Major>2</tt:Major><tt:Minor>20</tt:Minor></tds:Version>'
                '</tds:Service><tds:Service>'
                '<tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>'
                f'<tds:XAddr>http://{ip}:{HTTP_PORT}/onvif/media_service</tds:XAddr>'
                '<tds:Version><tt:Major>2</tt:Major><tt:Minor>20</tt:Minor></tds:Version>'
                '</tds:Service></tds:GetServicesResponse>')
    if action == 'GetCapabilities':
        return ('<tds:GetCapabilitiesResponse><tds:Capabilities>'
                '<tt:Device>'
                f'<tt:XAddr>http://{ip}:{HTTP_PORT}/onvif/device_service</tt:XAddr>'
                '<tt:Network><tt:IPFilter>false</tt:IPFilter>'
                '<tt:ZeroConfiguration>false</tt:ZeroConfiguration>'
                '<tt:IPVersion6>false</tt:IPVersion6>'
                '<tt:DynDNS>false</tt:DynDNS></tt:Network>'
                '<tt:System><tt:DiscoveryResolve>true</tt:DiscoveryResolve>'
                '<tt:DiscoveryBye>false</tt:DiscoveryBye>'
                '<tt:RemoteDiscovery>false</tt:RemoteDiscovery>'
                '<tt:SystemBackup>false</tt:SystemBackup>'
                '<tt:SystemLogging>false</tt:SystemLogging>'
                '<tt:FirmwareUpgrade>false</tt:FirmwareUpgrade>'
                '<tt:SupportedVersions><tt:Major>2</tt:Major>'
                '<tt:Minor>20</tt:Minor></tt:SupportedVersions></tt:System>'
                '<tt:IO><tt:InputConnectors>0</tt:InputConnectors>'
                '<tt:RelayOutputs>0</tt:RelayOutputs></tt:IO>'
                '<tt:Security><tt:TLS1.1>false</tt:TLS1.1><tt:TLS1.2>false</tt:TLS1.2>'
                '<tt:OnboardKeyGeneration>false</tt:OnboardKeyGeneration>'
                '<tt:AccessPolicyConfig>false</tt:AccessPolicyConfig>'
                '<tt:X.509Token>false</tt:X.509Token>'
                '<tt:SAMLToken>false</tt:SAMLToken>'
                '<tt:KerberosToken>false</tt:KerberosToken>'
                '<tt:RELToken>false</tt:RELToken></tt:Security>'
                '</tt:Device>'
                '<tt:Media>'
                f'<tt:XAddr>http://{ip}:{HTTP_PORT}/onvif/media_service</tt:XAddr>'
                '<tt:StreamingCapabilities>'
                '<tt:RTPMulticast>false</tt:RTPMulticast>'
                '<tt:RTP_TCP>true</tt:RTP_TCP>'
                '<tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP>'
                '</tt:StreamingCapabilities>'
                '</tt:Media>'
                '</tds:Capabilities></tds:GetCapabilitiesResponse>')
    if action == 'GetDeviceInformation':
        return ('<tds:GetDeviceInformationResponse>'
                '<tds:Manufacturer>CosmoEdge</tds:Manufacturer>'
                '<tds:Model>IPCamera</tds:Model>'
                '<tds:FirmwareVersion>1.1.0</tds:FirmwareVersion>'
                f'<tds:SerialNumber>{xml_escape(serial_number())}</tds:SerialNumber>'
                '<tds:HardwareId>RK3576</tds:HardwareId>'
                '</tds:GetDeviceInformationResponse>')
    if action == 'GetScopes':
        return ('<tds:GetScopesResponse>'
                '<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef>'
                '<tt:ScopeItem>onvif://www.onvif.org/type/NetworkVideoTransmitter</tt:ScopeItem>'
                '</tds:Scopes>'
                '<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef>'
                '<tt:ScopeItem>onvif://www.onvif.org/Profile/Streaming</tt:ScopeItem>'
                '</tds:Scopes>'
                '<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef>'
                '<tt:ScopeItem>onvif://www.onvif.org/name/CosmoEdge</tt:ScopeItem>'
                '</tds:Scopes>'
                '</tds:GetScopesResponse>')
    if action == 'GetDiscoveryMode':
        return ('<tds:GetDiscoveryModeResponse>'
                '<tds:DiscoveryMode>Discoverable</tds:DiscoveryMode>'
                '</tds:GetDiscoveryModeResponse>')
    if action == 'SetSystemDateAndTime':
        # Hikvision NVR/DVR always sends this after GetSystemDateAndTime.
        # go2rtc #1299: missing this → DVR reports Offline / Parameter Error.
        return '<tds:SetSystemDateAndTimeResponse/>'
    if action == 'GetHostname':
        return ('<tds:GetHostnameResponse><tds:HostnameInformation>'
                '<tt:FromDHCP>false</tt:FromDHCP><tt:Name>cosmo-edge</tt:Name>'
                '</tds:HostnameInformation></tds:GetHostnameResponse>')
    if action == 'GetNetworkInterfaces':
        return ('<tds:GetNetworkInterfacesResponse>'
                '<tds:NetworkInterfaces token="eth0">'
                '<tt:Enabled>true</tt:Enabled>'
                '<tt:Info><tt:Name>eth0</tt:Name>'
                '<tt:HwAddress>00:11:22:33:44:55</tt:HwAddress>'
                '<tt:MTU>1500</tt:MTU></tt:Info>'
                '<tt:IPv4><tt:Enabled>true</tt:Enabled><tt:Config>'
                f'<tt:Manual><tt:Address>{ip}</tt:Address>'
                '<tt:PrefixLength>24</tt:PrefixLength></tt:Manual>'
                '<tt:DHCP>false</tt:DHCP></tt:Config></tt:IPv4>'
                '</tds:NetworkInterfaces></tds:GetNetworkInterfacesResponse>')
    if action == 'GetNetworkProtocols':
        return ('<tds:GetNetworkProtocolsResponse>'
                '<tds:NetworkProtocols><tt:Name>RTSP</tt:Name>'
                '<tt:Enabled>true</tt:Enabled>'
                f'<tt:Port>{RTSP_PORT}</tt:Port></tds:NetworkProtocols>'
                '<tds:NetworkProtocols><tt:Name>HTTP</tt:Name>'
                '<tt:Enabled>true</tt:Enabled>'
                f'<tt:Port>{HTTP_PORT}</tt:Port></tds:NetworkProtocols>'
                '</tds:GetNetworkProtocolsResponse>')
    if action == 'GetNetworkDefaultGateway':
        return ('<tds:GetNetworkDefaultGatewayResponse>'
                '<tds:NetworkGateway><tt:IPv4Address>0.0.0.0</tt:IPv4Address>'
                '</tds:NetworkGateway></tds:GetNetworkDefaultGatewayResponse>')
    if action == 'GetDNS':
        return ('<tds:GetDNSResponse><tds:DNSInformation>'
                '<tt:FromDHCP>false</tt:FromDHCP>'
                '</tds:DNSInformation></tds:GetDNSResponse>')
    if action == 'GetNTP':
        return ('<tds:GetNTPResponse><tds:NTPInformation>'
                '<tt:FromDHCP>false</tt:FromDHCP>'
                '</tds:NTPInformation></tds:GetNTPResponse>')
    if action == 'GetWsdlUrl':
        return (f'<tds:GetWsdlUrlResponse><tds:WsdlUrl>'
                f'http://{ip}:{HTTP_PORT}/onvif/</tds:WsdlUrl>'
                '</tds:GetWsdlUrlResponse>')
    if action == 'GetProfiles':
        names = stream_names()
        print('GetProfiles', names, 'ip', ip, flush=True)
        return ('<trt:GetProfilesResponse>'
                + ''.join(profile_xml(n) for n in names)
                + '</trt:GetProfilesResponse>')
    if action == 'GetProfile':
        token = profile_token(body) or stream_names()[0]
        return f'<trt:GetProfileResponse>{profile_xml(token)}</trt:GetProfileResponse>'
    if action == 'GetStreamUri':
        token = profile_token(body)
        names = stream_names()
        if token not in names:
            token = names[0]
        uri = f'rtsp://{ip}:{RTSP_PORT}/live/{token}'
        print('GetStreamUri', uri, 'host', host_header, flush=True)
        return ('<trt:GetStreamUriResponse><trt:MediaUri>'
                f'<tt:Uri>{xml_escape(uri)}</tt:Uri>'
                '<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>'
                '<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>'
                '<tt:Timeout>PT60S</tt:Timeout>'
                '</trt:MediaUri></trt:GetStreamUriResponse>')
    if action == 'GetVideoEncoderConfigurationOptions':
        # Hikvision NVR/DVR always queries encoder options (go2rtc #1299).
        return ('<trt:GetVideoEncoderConfigurationOptionsResponse>'
                '<trt:Options>'
                '<tt:QualityRange><tt:Min>1</tt:Min><tt:Max>6</tt:Max></tt:QualityRange>'
                '<tt:H264>'
                '<tt:ResolutionsAvailable><tt:Width>1280</tt:Width>'
                '<tt:Height>720</tt:Height></tt:ResolutionsAvailable>'
                '<tt:GovLengthRange><tt:Min>1</tt:Min><tt:Max>100</tt:Max></tt:GovLengthRange>'
                '<tt:FrameRateRange><tt:Min>1</tt:Min><tt:Max>25</tt:Max></tt:FrameRateRange>'
                '<tt:EncodingIntervalRange><tt:Min>1</tt:Min><tt:Max>100</tt:Max>'
                '</tt:EncodingIntervalRange>'
                '<tt:H264ProfilesSupported>Main</tt:H264ProfilesSupported>'
                '</tt:H264></trt:Options>'
                '</trt:GetVideoEncoderConfigurationOptionsResponse>')
    if action in ('GetVideoEncoderConfiguration', 'GetVideoEncoderConfigurations'):
        return vec_xml() if action.endswith('tion') else (
            '<trt:GetVideoEncoderConfigurationsResponse>'
            '<trt:Configurations token="vec"><tt:Name>VEC</tt:Name>'
            '<tt:UseCount>1</tt:UseCount><tt:Encoding>H264</tt:Encoding>'
            '<tt:Resolution><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:Resolution>'
            '<tt:Quality>5</tt:Quality>'
            '<tt:RateControl><tt:FrameRateLimit>10</tt:FrameRateLimit>'
            '<tt:EncodingInterval>1</tt:EncodingInterval>'
            '<tt:BitrateLimit>2048</tt:BitrateLimit></tt:RateControl>'
            '<tt:H264><tt:GovLength>25</tt:GovLength>'
            '<tt:H264Profile>Main</tt:H264Profile></tt:H264>'
            '<tt:SessionTimeout>PT60S</tt:SessionTimeout>'
            '</trt:Configurations></trt:GetVideoEncoderConfigurationsResponse>')
    if action in ('GetVideoSourceConfiguration', 'GetVideoSourceConfigurations'):
        inner = ('<tt:Name>VSC_1</tt:Name><tt:UseCount>1</tt:UseCount>'
                 '<tt:SourceToken>VSrc_1</tt:SourceToken>'
                 '<tt:Bounds x="0" y="0" width="1280" height="720"/>')
        tag = 'Configuration' if action.endswith('tion') else 'Configurations'
        return (f'<trt:{action}Response><trt:{tag} token="VSC_1">{inner}'
                f'</trt:{tag}></trt:{action}Response>')
    if action == 'GetVideoSources':
        return ('<trt:GetVideoSourcesResponse>'
                '<trt:VideoSources token="VSrc_1">'
                '<tt:Framerate>10</tt:Framerate>'
                '<tt:Resolution><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:Resolution>'
                '</trt:VideoSources></trt:GetVideoSourcesResponse>')
    if action == 'GetSnapshotUri':
        token = profile_token(body) or stream_names()[0]
        return ('<trt:GetSnapshotUriResponse><trt:MediaUri>'
                f'<tt:Uri>http://{ip}/</tt:Uri>'
                '<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>'
                '<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>'
                '<tt:Timeout>PT60S</tt:Timeout>'
                '</trt:MediaUri></trt:GetSnapshotUriResponse>')
    if action == 'GetServiceCapabilities':
        if 'media' in body.lower() or 'trt' in body.lower():
            return ('<trt:GetServiceCapabilitiesResponse>'
                    '<trt:Capabilities SnapshotUri="false" Rotation="false" '
                    'OSD="false">'
                    '<trt:StreamingCapabilities RTPMulticast="false" '
                    'RTP_TCP="true" RTP_RTSP_TCP="true" '
                    'NonAggregateControl="false" NoRTSPStreaming="false"/>'
                    '</trt:Capabilities></trt:GetServiceCapabilitiesResponse>')
        return ('<tds:GetServiceCapabilitiesResponse><tds:Capabilities/>'
                '</tds:GetServiceCapabilitiesResponse>')
    if action == 'GetOSDOptions':
        return '<trt:GetOSDOptionsResponse/>'
    if action in ('GetAudioSources', 'GetAudioSourceConfigurations',
                  'GetAudioEncoderConfigurations', 'GetOSDs',
                  'GetVideoAnalyticsConfigurations'):
        return f'<trt:{action}Response></trt:{action}Response>'
    if action in ('GetUsers', 'GetRemoteUser', 'GetCertificates',
                  'GetCertificatesStatus', 'GetPkcs10Request',
                  'GetClientCertificateMode', 'GetAccessPolicy',
                  'GetDot11Capabilities', 'GetDot11Status',
                  'GetSystemUris', 'GetSystemLog', 'GetSystemSupportInformation',
                  'GetRelayOutputs', 'GetDigitalInputs', 'SystemReboot'):
        return f'<tds:{action}Response></tds:{action}Response>'
    # Hikvision NVR aborts on SOAP Fault / HTTP 500 for unknown ops.
    # Empty success is what go2rtc DVR testers used.
    print('unsupported action (empty ok)', action, flush=True)
    ns = 'trt' if any(k in action for k in ('Video', 'Audio', 'OSD', 'Stream', 'Profile')) else 'tds'
    return f'<{ns}:{action}Response/>'


def fault_xml(code, subcode, text):
    return ('<s:Fault><s:Code><s:Value>s:%s</s:Value><s:Subcode>'
            '<s:Value>%s</s:Value></s:Subcode></s:Code>'
            '<s:Reason><s:Text xml:lang="en">%s</s:Text></s:Reason></s:Fault>'
            % (code, subcode, xml_escape(text)))


def parse_action(headers, body):
    m = re.search(r'<(?:[\w]+:)?Action[^>]*>([^<]+)</', body)
    if m:
        return m.group(1).rsplit('/', 1)[-1].rsplit(':', 1)[-1]
    sa = headers.get('SOAPAction', '') or headers.get('Action', '')
    if sa:
        return sa.strip('"\'').rsplit('/', 1)[-1].rsplit(':', 1)[-1]
    # go2rtc: Body[^<]+<([^ />]+) — Hikvision often has no whitespace after Body>
    m = re.search(r'Body[^<]*<([^ />]+)', body)
    if m:
        tag = m.group(1)
        return tag.split(':', 1)[-1]
    return ''


class OnvifHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, fmt, *args):
        print('%s %s' % (self.address_string(), fmt % args), flush=True)

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'application/soap+xml; charset=utf-8')
        self.send_header('Content-Length', '0')
        self.end_headers()

    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0) or 0)
        body = self.rfile.read(length).decode('utf-8', 'replace') if length else ''
        action = parse_action(self.headers, body)
        print('SOAP', self.path, action or '(empty)', flush=True)
        payload = handle_soap(action, body, self.headers.get('Host', ''))
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
    while True:
        s = None
        try:
            ip = board_ip()
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            except OSError:
                pass
            s.bind(('', 3702))
            iface = ip if ip and not ip.startswith('127.') else '0.0.0.0'
            mreq = socket.inet_aton(mcast) + socket.inet_aton(iface)
            s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            if iface != '0.0.0.0':
                s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF,
                             socket.inet_aton(iface))
            print('WS-Discovery joined', iface, flush=True)
            while True:
                try:
                    data, addr = s.recvfrom(65535)
                except OSError:
                    continue
                if b'Probe' not in data or b'MessageID' not in data:
                    continue
                m = re.search(rb'<(?:[\w]+:)?MessageID[^>]*>([^<]+)<', data)
                mid = m.group(1).decode() if m else 'urn:uuid:0'
                ip = board_ip()
                reply = (
                    '<?xml version="1.0" encoding="UTF-8"?>'
                    '<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" '
                    'xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" '
                    'xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery" '
                    'xmlns:dn="http://www.onvif.org/ver10/network/wsdl" '
                    'xmlns:tds="http://www.onvif.org/ver10/device/wsdl">'
                    '<soap:Header>'
                    f'<a:MessageID>urn:uuid:{uuid.uuid4()}</a:MessageID>'
                    f'<a:RelatesTo>{xml_escape(mid)}</a:RelatesTo>'
                    '<a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:To>'
                    '<a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</a:Action>'
                    '<d:AppSequence InstanceId="1" MessageNumber="1"/>'
                    '</soap:Header><soap:Body><d:ProbeMatches><d:ProbeMatch>'
                    '<a:EndpointReference><a:Address>'
                    f'urn:uuid:{uuid.uuid5(uuid.NAMESPACE_DNS, ip)}'
                    '</a:Address></a:EndpointReference>'
                    '<d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>'
                    '<d:Scopes>onvif://www.onvif.org/type/NetworkVideoTransmitter '
                    'onvif://www.onvif.org/Profile/Streaming '
                    'onvif://www.onvif.org/name/CosmoEdge '
                    'onvif://www.onvif.org/hardware/RK3576</d:Scopes>'
                    f'<d:XAddrs>http://{ip}:{HTTP_PORT}/onvif/device_service</d:XAddrs>'
                    '<d:MetadataVersion>1</d:MetadataVersion>'
                    '</d:ProbeMatch></d:ProbeMatches></soap:Body></soap:Envelope>'
                )
                s.sendto(reply.encode(), addr)
        except OSError as e:
            print('discovery retry:', e, flush=True)
            time.sleep(5)
        finally:
            if s is not None:
                try:
                    s.close()
                except OSError:
                    pass


def main():
    threading.Thread(target=discovery_loop, daemon=True).start()
    print(f'ONVIF serving http://0.0.0.0:{HTTP_PORT} (any path) + WS-Discovery :3702',
          flush=True)
    ThreadingHTTPServer(('0.0.0.0', HTTP_PORT), OnvifHandler).serve_forever()


if __name__ == '__main__':
    main()
