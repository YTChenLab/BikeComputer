#!/usr/bin/env python3
"""Windows 原生串口资源安装器；无第三方依赖，不下载固件，不修改 V1 文件。"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import secrets
import struct
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
HEADER = struct.Struct('<4sBBHIII')
HELLO, STATUS, BEGIN, DATA, COMMIT, ABORT, DIAG, MAINTENANCE, REBOOT = range(1, 10)
ERRORS = ('OK', 'FORMAT', 'CRC', 'STATE', 'SESSION', 'SEQUENCE', 'LENGTH', 'STORAGE',
          'LOCKED', 'BUSY', 'TIMEOUT', 'CONFLICT', 'UNSUPPORTED', 'TRANSPORT', 'NO_SAFE_AREA')


def encode(cmd, payload=b'', session=0, seq=0):
    if len(payload) > 260:
        raise ValueError('帧负载超过 260 Byte')
    head = struct.pack('<4sBBHII', b'BCRI', 1, cmd, len(payload), session, seq)
    return head + struct.pack('<I', zlib.crc32(head + payload)) + payload


class Decoder:
    """滑动重同步容忍上电日志、噪声、分段输入和 CRC 错误。"""
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while len(self.buffer) >= 4:
            if self.buffer[:4] != b'BCRI':
                del self.buffer[0]
                continue
            if len(self.buffer) < HEADER.size:
                break
            _, version, cmd, n, session, seq, crc = HEADER.unpack_from(self.buffer)
            if version != 1 or n > 260:
                del self.buffer[0]
                continue
            if len(self.buffer) < HEADER.size + n:
                break
            packet = bytes(self.buffer[:HEADER.size + n])
            del self.buffer[:len(packet)]
            if zlib.crc32(packet[:16] + packet[20:]) == crc:
                frames.append((cmd, session, seq, packet[20:]))
        return frames


class DeviceError(RuntimeError):
    def __init__(self, status):
        self.status = status
        code = status['code']
        detail = status.get('diagnostics', {}).get('diagnostic_detail', status['detail'])
        super().__init__(f"设备拒绝：{ERRORS[code] if code < len(ERRORS) else code}，底层码={detail}")


def decode_status(payload):
    if len(payload) != 128:
        raise ValueError('设备回复长度不是协议 V1 的 128 Byte')
    code, detail, mode, active, received, total, version, size = struct.unpack_from('<8I', payload)
    tail = struct.unpack_from('<13I', payload, 76)
    return dict(code=code, detail=detail, mode=mode, active_slot=active,
                received=received, total=total, package_version=version, package_size=size,
                payload_sha256=payload[32:64].hex(), uid=payload[64:76].hex(),
                session=tail[0], next_sequence=tail[1], target_slot=tail[2],
                last_error=tail[3], last_detail=tail[4], locked=tail[5], committed=tail[7],
                running=tail[8], product_id=tail[9], firmware_compat=tail[10],
                chunk_size=tail[11], session_timeout_ms=tail[12], raw_tail=tail)


class Client:
    def __init__(self, transport, timeout=20, retries=4):
        self.transport, self.timeout, self.retries = transport, timeout, retries
        self.decoder = Decoder()
        self.query_seq = secrets.randbits(32)

    def request(self, cmd, payload=b'', session=0, seq=None, timeout=None):
        if seq is None:
            self.query_seq = (self.query_seq + 1) & 0xffffffff
            seq = self.query_seq
        packet = encode(cmd, payload, session, seq)
        for attempt in range(self.retries + 1):
            self.transport.write(packet)
            deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
            while time.monotonic() < deadline:
                for c, sid, number, body in self.decoder.feed(self.transport.read(512)):
                    if (c, sid, number) == (cmd | 0x80, session, seq):
                        status = decode_status(body)
                        if cmd == DIAG:
                            names = ('result', 'passed_mask', 'skipped_mask', 'jedec_le', 'sr1', 'sr2',
                                     'indirect_status', 'mapped_status', 'mpu_error', 'safe_scratch_bytes',
                                     'sample_bytes', 'sample_xor', 'diagnostic_detail')
                            status['diagnostics'] = dict(zip(names, status['raw_tail']))
                            for key in ('session', 'next_sequence', 'target_slot', 'last_error',
                                        'last_detail', 'locked', 'committed', 'running', 'product_id',
                                        'firmware_compat', 'chunk_size', 'session_timeout_ms'):
                                status.pop(key, None)
                        if status['code']:
                            raise DeviceError(status)
                        return status
            print(f'等待 ACK 超时，重传 {attempt + 1}/{self.retries}', file=sys.stderr)
        raise TimeoutError(f'命令 {cmd} 超时；不得据此认定设备尚未提交，请重新查询状态')

    def ensure_maintenance(self):
        status = self.request(HELLO)
        if (status['product_id'], status['firmware_compat'], status['chunk_size']) != (0x42494B45, 1, 256):
            raise RuntimeError('设备产品/兼容版本/分包能力与本工具不匹配')
        uid = status['uid']
        if status['mode'] != 1 or status['locked']:
            self.request(MAINTENANCE)
            # 软件复位不枚举 USB；PC 串口适配器保持打开，等待设备启动并消费维护标记。
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                time.sleep(.5)
                try:
                    status = self.request(STATUS, timeout=2)
                    if status['uid'] != uid:
                        raise RuntimeError('复位后设备 UID 发生变化')
                    if status['mode'] == 1 and not status['locked']:
                        return status
                except TimeoutError:
                    continue
            raise TimeoutError('未进入维护模式；检查供电和复位后启动日志')
        return status

    def install(self, image, manifest, progress=lambda n, total: None, reboot=True):
        initial = self.ensure_maintenance()
        if initial['running']:
            raise RuntimeError('设备已有未完成会话；请明确执行 abort 或等待 30 秒会话超时后重试')
        print(f"设备 UID={initial['uid']}，活动槽={initial['active_slot']}，维护模式已确认。")
        session = secrets.randbits(32) or 1
        sequence = 0
        committed = False
        try:
            reply = self.request(BEGIN, struct.pack('<I', len(image)), session, sequence)
            target = reply['target_slot']
            for offset in range(0, len(image), 256):
                sequence += 1
                chunk = image[offset:offset + 256]
                reply = self.request(DATA, struct.pack('<I', offset) + chunk, session, sequence)
                if reply['received'] != offset + len(chunk):
                    raise RuntimeError('设备已写入长度与本地偏移不一致')
                progress(reply['received'], len(image))
            print('\n传输及逐包写入已确认；等待整包验证与原子提交。')
            sequence += 1
            self.request(COMMIT, session=session, seq=sequence, timeout=120)
            committed = True
            # 最终确认不依赖缓存 COMMIT ACK，发独立 STATUS 查询已提交槽的摘要。
            status = self.request(STATUS)
            if (status['active_slot'] != target or status['package_size'] != len(image) or
                    status['package_version'] != manifest['package_version'] or
                    status['payload_sha256'] != manifest['payload_sha256'].lower()):
                raise RuntimeError('已收到 COMMIT ACK，但独立状态确认不匹配，不能宣称安装成功')
            print(f"资源提交及独立查询确认成功：槽 {'AB'[target]}，SHA256={status['payload_sha256']}")
            if reboot:
                self.request(REBOOT)
                print('设备已确认正常启动复位请求；LVGL 上屏仍需观察实机。')
            return status
        except BaseException:
            if not committed:
                try:
                    # 发送当前设备所期待的序号；若 ACK 丢失导致不确定，先 STATUS。
                    state = self.request(STATUS, timeout=2)
                    if state['session'] == session:
                        self.request(ABORT, session=session, seq=state['next_sequence'], timeout=5)
                except Exception:
                    print('无法确认中止；设备会在 30 秒会话超时后撤销未提交会话。请查询活动槽。', file=sys.stderr)
            raise


class WindowsSerial:
    """同步 Win32 串口；禁用 DTR/RTS 流控，无 pyserial 依赖，不执行固件下载。"""
    def __init__(self, port):
        if os.name != 'nt':
            raise OSError('实机串口后端仅用于 Windows；测试使用内存传输适配器')
        from ctypes import wintypes as W
        self.k = C.WinDLL('kernel32', use_last_error=True)
        self.k.CreateFileW.argtypes = [W.LPCWSTR, W.DWORD, W.DWORD, C.c_void_p, W.DWORD, W.DWORD, W.HANDLE]
        self.k.CreateFileW.restype = W.HANDLE
        for name in ('GetCommState', 'SetCommState', 'SetCommTimeouts', 'ReadFile', 'WriteFile', 'CloseHandle'):
            getattr(self.k, name).restype = W.BOOL
        self.k.GetCommState.argtypes = [W.HANDLE, C.c_void_p]
        self.k.SetCommState.argtypes = [W.HANDLE, C.c_void_p]
        self.k.SetCommTimeouts.argtypes = [W.HANDLE, C.c_void_p]
        self.k.ReadFile.argtypes = self.k.WriteFile.argtypes = [W.HANDLE, C.c_void_p, W.DWORD, C.POINTER(W.DWORD), C.c_void_p]
        self.k.CloseHandle.argtypes = [W.HANDLE]
        self.handle = self.k.CreateFileW('\\\\.\\' + port, 0xC0000000, 0, None, 3, 0, None)
        if self.handle == C.c_void_p(-1).value:
            raise C.WinError(C.get_last_error())
        class DCB(C.Structure):
            _fields_ = [('length', C.c_uint32), ('baud', C.c_uint32), ('flags', C.c_uint32),
                        ('reserved', C.c_uint16), ('xonlim', C.c_uint16), ('xofflim', C.c_uint16),
                        ('size', C.c_ubyte), ('parity', C.c_ubyte), ('stop', C.c_ubyte),
                        ('xon', C.c_byte), ('xoff', C.c_byte), ('error', C.c_byte),
                        ('eof', C.c_byte), ('evt', C.c_byte), ('reserved1', C.c_uint16)]
        dcb = DCB(); dcb.length = C.sizeof(dcb)
        try:
            self.check(self.k.GetCommState(self.handle, C.byref(dcb)))
            dcb.baud, dcb.flags, dcb.size, dcb.parity, dcb.stop = 115200, 1, 8, 0, 0
            self.check(self.k.SetCommState(self.handle, C.byref(dcb)))
            timeouts = (C.c_uint32 * 5)(0xFFFFFFFF, 0, 50, 0, 5000)
            self.check(self.k.SetCommTimeouts(self.handle, timeouts))
        except BaseException:
            self.close(); raise

    @staticmethod
    def check(ok):
        if not ok:
            raise C.WinError(C.get_last_error())

    def write(self, data):
        n = C.c_uint32()
        self.check(self.k.WriteFile(self.handle, C.c_char_p(data), len(data), C.byref(n), None))
        if n.value != len(data):
            raise OSError('串口未完整发送本帧')

    def read(self, count):
        buf = C.create_string_buffer(count); n = C.c_uint32()
        self.check(self.k.ReadFile(self.handle, buf, count, C.byref(n), None))
        return buf.raw[:n.value]

    def close(self):
        self.k.CloseHandle(self.handle)


def load_package(bin_path, json_path):
    # 复用既有独立 V1 验收器，完整验证 Header/Index/CRC/SHA 和 JSON，不另写解析器。
    sys.path.insert(0, str(ROOT.parent / 'Lvgl_Project/tools'))
    from validate_resource_package import verify
    image = Path(bin_path).read_bytes()
    manifest = json.loads(Path(json_path).read_text(encoding='utf-8'))
    verify(image, manifest)
    if len(manifest['resources']) != 37:
        raise ValueError('本阶段只接收已经固定的 37 项资源包')
    return image, manifest


def main():
    default = ROOT.parent / 'Lvgl_Project/build/resource_package/bike_ui_resources_v1'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('status', 'diagnose', 'install', 'abort', 'maintenance', 'reboot', 'validate'))
    parser.add_argument('--port', help='明确填写实际 USART1 USB-TTL 对应的 COM 端口，例如 COM7')
    parser.add_argument('--bin', type=Path, default=default.with_suffix('.bin'))
    parser.add_argument('--json', type=Path, default=default.with_suffix('.json'))
    parser.add_argument('--no-reboot', action='store_true')
    args = parser.parse_args()
    # 在打开端口、复位或写入前做完整本地校验。
    image = manifest = None
    if args.command in ('install', 'validate'):
        image, manifest = load_package(args.bin, args.json)
        print(f'本地 V1 完整校验通过：{len(image)} Byte，37 项资源。')
        if args.command == 'validate':
            return
    if not args.port:
        parser.error('必须显式指定 --port；工具不猜测已连接的 UART')
    transport = WindowsSerial(args.port)
    try:
        client = Client(transport)
        if args.command == 'install':
            last = [-1]
            def progress(n, total):
                percent = n * 100 // total
                if percent != last[0]:
                    last[0] = percent
                    print(f'\r已确认写入 {n}/{total} Byte ({percent}%)', end='', flush=True)
            client.install(image, manifest, progress, not args.no_reboot)
        elif args.command == 'diagnose':
            status = client.request(DIAG, struct.pack('<I', 0))
            print(json.dumps(status, ensure_ascii=False, indent=2))
        elif args.command == 'maintenance':
            print(json.dumps(client.ensure_maintenance(), ensure_ascii=False, indent=2))
        elif args.command == 'abort':
            state = client.request(STATUS)
            if state['running']:
                state = client.request(ABORT, session=state['session'], seq=state['next_sequence'])
            print(json.dumps(state, ensure_ascii=False, indent=2))
        else:
            print(json.dumps(client.request(REBOOT if args.command == 'reboot' else STATUS), ensure_ascii=False, indent=2))
    finally:
        transport.close()


if __name__ == '__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt) as exc:
        if isinstance(exc, DeviceError):
            print(json.dumps(exc.status, ensure_ascii=False, indent=2), file=sys.stderr)
        print(f'操作失败：{exc}', file=sys.stderr)
        sys.exit(1)
