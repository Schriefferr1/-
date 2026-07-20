# 作业一：比特币测试网 SegWit 交易与区块逐字节解析报告

## 1. 目标交易概况
* **交易 Hash (TXID):** `e60deca623a4886c8db9c45d68e2cf36f99185e79714208ff1b038c59bb5d9ad`
* **地址类型:** Native SegWit (Bech32, `tb1q...`)
* **网络:** Bitcoin Testnet

---

## 2. 原始 Hex 数据与逐字节切分说明

原始 Hex 字节流如下：
> `02000000000101986ef0d5fd554d4c185f9aacf3c08f8185af8b41657771339c70c604d125950c0000000000fdffffff016c9405000000000016001405be528604d5fb537c561d64a8051d4893c191e20247304402202dffbafc5008502ef7b975b58125f13918142541feb4d4976d181d122226df2f02206de595e66807f8d6fdc0e625eba6af230bd6d504c26084ae258518d3197f3160012103e5e32978febc6426e2e54d9fe65f1a2522de807f45dc6fca418255205fc2392c00000000`

### 字节映射拆解表

| 字段名称 | 字节大小 | Hex 数据段 | 解析说明 |
| :--- | :--- | :--- | :--- |
| **Version** | 4 Bytes | `02000000` | 版本号 2（小端序） |
| **Marker & Flag** | 2 Bytes | `0001` | SegWit 隔离见证专属标识 (Marker: 0x00, Flag: 0x01) |
| **Input Count** | VarInt | `01` | 包含 1 个输入 |
| **Prev TX Hash** | 32 Bytes | `986ef0d5fd554d4c185f9aacf3c08f8185af8b41657771339c70c604d125950c` | 前驱交易哈希（小端序存储，反转后为实际 TXID） |
| **Prev Vout** | 4 Bytes | `00000000` | 前驱输出索引 `0` |
| **ScriptSig Size** | VarInt | `00` | SegWit 交易的 ScriptSig 长度为 0 |
| **ScriptSig** | 0 Bytes | *(Empty)* | 签名信息转移至 Witness 字段中 |
| **Sequence** | 4 Bytes | `fdffffff` | 序列号，用于相对时间锁或 RBF 功能 |
| **Output Count** | VarInt | `01` | 包含 1 个输出 |
| **Value** | 8 Bytes | `6c94050000000000` | 金额：`365,676` 聪（Satoshi，小端序转换为十进制） |
| **ScriptPubKey Size**| VarInt | `16` | 锁定脚本长度：22 字节 (`0x16`) |
| **ScriptPubKey** | 22 Bytes | `001405be528604d5fb537c561d64a8051d4893c191e2` | P2WPKH 锁定脚本 (`OP_0` + 20字节公钥哈希) |
| **Witness Count** | VarInt | `02` | 该输入包含 2 项见证数据 (Witness Items) |
| **Witness Item 0** | 71 Bytes | `304402202dffbafc...01` | ECDSA DER 编码签名数据（包含 `SIGHASH_ALL`） |
| **Witness Item 1** | 33 Bytes | `03e5e32978f...392c` | 压缩格式公钥 (33 字节，以 `03` 开头) |
| **Locktime** | 4 Bytes | `00000000` | 锁定时间 `0`（立即生效） |

---

## 3. 区块头 (Block Header) 结构解析

比特币区块头固定大小为 80 字节，其结构规范如下：

```text
+-------------------------------------------------------------------------+
|                              BLOCK HEADER                               |
+-------------------+--------------------+--------------------------------+
| Field             | Size               | Description                    |
+-------------------+--------------------+--------------------------------+
| Version           | 4 Bytes            | 区块版本号                     |
| Previous Block    | 32 Bytes           | 前一区块的 SHA256d 哈希        |
| Merkle Root       | 32 Bytes           | 区块内所有交易组成的默克尔树根 |
| Timestamp         | 4 Bytes            | 创块 UNIX 时间戳               |
| Bits              | 4 Bytes            | 目标难度系数 (nBits)           |
| Nonce             | 4 Bytes            | 工作量证明计算随机数           |
+-------------------+--------------------+--------------------------------+