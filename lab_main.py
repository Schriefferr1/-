import struct
import requests
import random
from io import BytesIO
from ecdsa import SECP256k1
from ecdsa.numbertheory import inverse_mod

TX_HASH = "e60deca623a4886c8db9c45d68e2cf36f99185e79714208ff1b038c59bb5d9ad"

def read_varint(stream):
    b = stream.read(1)
    if not b: return 0
    prefix = b[0]
    if prefix < 0xfd: return prefix
    elif prefix == 0xfd: return struct.unpack('<H', stream.read(2))[0]
    elif prefix == 0xfe: return struct.unpack('<I', stream.read(4))[0]
    else: return struct.unpack('<Q', stream.read(8))[0]

def parse_my_tx():
    print("================ 1. 比特币 SegWit 交易逐字节解析 ================")
    url = f"https://mempool.space/testnet/api/tx/{TX_HASH}/hex"
    try:
        hex_str = requests.get(url).text.strip()
    except Exception as e:
        print(f"拉取 Hex 失败: {e}")
        return

    print(f"目标 TXID: {TX_HASH}")
    print(f"原始 Hex (前 64 字符): {hex_str[:64]}...\n")
    
    stream = BytesIO(bytes.fromhex(hex_str))
    
    version = struct.unpack('<I', stream.read(4))[0]
    print(f"[Version] 版本号: {version} (4 bytes)")
    
    marker_flag = stream.read(2)
    is_segwit = False
    if marker_flag == b'\x00\x01':
        is_segwit = True
        print("[SegWit Flag] 检测到隔离见证标识: 00 01 (2 bytes)")
    else:
        stream.seek(-2, 1)

    in_count = read_varint(stream)
    print(f"[Input Count] 输入数量: {in_count} (VarInt)")
    for i in range(in_count):
        prev_tx = stream.read(32)[::-1].hex()
        vout = struct.unpack('<I', stream.read(4))[0]
        script_len = read_varint(stream)
        script_sig = stream.read(script_len).hex() if script_len > 0 else "Empty (SegWit)"
        sequence = stream.read(4).hex()
        print(f"  Input {i}: Prev TX={prev_tx}, Vout={vout}, ScriptSig={script_sig}, Sequence={sequence}")

    out_count = read_varint(stream)
    print(f"[Output Count] 输出数量: {out_count} (VarInt)")
    for i in range(out_count):
        value = struct.unpack('<q', stream.read(8))[0]
        script_len = read_varint(stream)
        script_pubkey = stream.read(script_len).hex()
        print(f"  Output {i}: Amount={value} Sats, ScriptPubKey={script_pubkey}")

    if is_segwit:
        print("[Witness Data] 隔离见证数据区:")
        for i in range(in_count):
            witness_count = read_varint(stream)
            print(f"  Input {i} Witness 数量: {witness_count}")
            for j in range(witness_count):
                w_len = read_varint(stream)
                w_data = stream.read(w_len).hex()
                print(f"    Item {j} ({w_len} Bytes): {w_data}")

    locktime = struct.unpack('<I', stream.read(4))[0]
    print(f"[Locktime] 锁定时间: {locktime} (4 bytes)\n")

def run_ecdsa_forgery():
    print("================ 2. ECDSA 任意消息签名伪造实验 ================")
    curve = SECP256k1.curve
    G = SECP256k1.generator
    n = G.order()
    
    d = random.SystemRandom().randrange(1, n)
    P = d * G
    
    u = random.SystemRandom().randrange(1, n)
    v = random.SystemRandom().randrange(1, n)
    
    R_prime = (u * G) + (v * P)
    r_prime = R_prime.x() % n
    v_inv = inverse_mod(v, n)
    s_prime = (r_prime * v_inv) % n
    e_prime = (u * s_prime) % n
    
    s_prime_inv = inverse_mod(s_prime, n)
    R_check = ((e_prime * s_prime_inv % n) * G) + ((r_prime * s_prime_inv % n) * P)
    
    print(f"伪造计算得到的 r': {hex(r_prime)[:30]}...")
    print(f"伪造计算得到的 s': {hex(s_prime)[:30]}...")
    print(f"对应的虚假 Hash e': {hex(e_prime)[:30]}...")
    
    if R_check.x() % n == r_prime:
        print("[+] 验证成功: 构造的三元组 (r', s', e') 完美通过椭圆曲线方程校验！")

if __name__ == "__main__":
    parse_my_tx()
    run_ecdsa_forgery()