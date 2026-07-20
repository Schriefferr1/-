# 作业二：ECDSA 签名伪造数学推导与 secp256k1 源码解析

## 1. 未校验消息结构下的 ECDSA 签名伪造原理

### 1.1 正常 ECDSA 验证逻辑
在标准 ECDSA 中，验证签名 $(r, s)$ 需要计算：
$$e = \text{hash}(m)$$
$$u_1 = e \cdot s^{-1} \bmod n, \quad u_2 = r \cdot s^{-1} \bmod n$$
$$R' = u_1 G + u_2 P = (x', y')$$
当且仅当 $x' \bmod n = r$ 时，签名校验通过。

### 1.2 伪造签名推导（当系统直接接收 $e$ 时）
如果验证端**只校验哈希 $e$ 而不校验原始消息 $m$**，攻击者无需知道私钥 $d$，可按如下步骤伪造：

1. 随机选择标量 $u, v \in \mathbb{F}_n^*$。
2. 计算曲线上对应的点：
   $$R' = (x', y') = uG + vP$$
3. 取出 $r'$：
   $$r' = x' \bmod n$$
4. 逆向构造签名项 $s'$ 与对应的虚假哈希 $e'$：
   $$s' = r' \cdot v^{-1} \bmod n$$
   $$e' = u \cdot s' \bmod n$$

### 1.3 验证代入
将三元组 $(r', s', e')$ 带入验证等式：
$$s'^{-1} e' G + s'^{-1} r' P = (s'^{-1} u s') G + (s'^{-1} r') P = uG + vP = R'$$
对应 $x$ 坐标同样满足 $x' \bmod n = r'$，校验必定通过。

---

## 2. bitcoin-core/secp256k1 核心分析

### 2.1 Bug Fixes & 安全漏洞防御
* **签名可变性 (Signature Malleability / BIP 62)：**
  ECDSA 中 $(r, s)$ 与 $(r, n-s)$ 均合法。`secp256k1` 库引入 **Low-S Normalization**，若 $s > n/2$，则强制替换为 $n-s$，消除了第三方修改交易 TXID 的漏洞。
* **常数时间执行 (Constant-Time Execution)：**
  库中标量乘法与模逆运算均采用无分支、常数内存访问路径实现，完全规避了时序侧信道攻击 (Timing Attack)。

### 2.2 性能优化机制
* **GLV 内同态加速 (GLV Endomorphism)：**
  利用 secp256k1 曲线的代数结构，将 256 位标量点乘拆分为两个 128 位标量点乘：$kP = k_1 P + k_2 (\lambda P)$，将标量乘法性能提升约 30%。
* **雅可比投影坐标系 (Jacobian Coordinates)：**
  将点从仿射坐标 $(x, y)$ 映射至 $(X, Y, Z)$，将耗时的模逆运算转化为点加法中的模乘法，仅在最终阶段执行一次模逆。