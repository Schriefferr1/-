import tenseal as ts
import numpy as np

def extract_2x2_from_flat(decrypted_vector):
    return np.array([
        decrypted_vector[0], decrypted_vector[1], 
        decrypted_vector[4], decrypted_vector[5]
    ]).reshape(2, 2)

def main():
    print("=== 作业6：打包→旋转→累加 策略优化 (BSGS 4次旋转极小值) ===")
    
    # 1. 初始化上下文
    context = ts.context(
        ts.SCHEME_TYPE.CKKS,
        poly_modulus_degree=8192,
        coeff_mod_bit_sizes=[60, 40, 40, 60]
    )
    context.global_scale = 2**40
    context.generate_galois_keys()

    # 2. 准备数据
    X = np.arange(1, 17, dtype=np.float64).reshape(4, 4)
    W = np.array([
        [ 1,  2,  1],
        [ 0,  1,  0],
        [-1, -2, -1]
    ], dtype=np.float64)

    expected_Y = np.zeros((2, 2))
    for i in range(2):
        for j in range(2):
            expected_Y[i, j] = np.sum(X[i:i+3, j:j+3] * W)
    print("明文期望结果 (2x2):\n", expected_Y)

    # 3. 加密数据
    x_flat = X.flatten().tolist()
    enc_x = ts.ckks_vector(context, x_flat)

    # 4. BSGS 优化卷积计算 (4次旋转)
    rotations = 0
    
    # [Baby-Steps]: 构建 dx 方向的偏移 X_0, X_1, X_2
    X_dx = [enc_x.copy()]
    for dx in range(1, 3):
        shifted = enc_x.copy()
        shifted.rotate(dx)
        X_dx.append(shifted)
        rotations += 1
        
    # 构建行中间结果
    T_dy = []
    for dy in range(3):
        T = None
        for dx in range(3):
            term = X_dx[dx] * W[dy, dx]
            if T is None:
                T = term
            else:
                T += term
        T_dy.append(T)
        
    # [Giant-Steps]: 在 dy 方向上旋转中间结果并累加
    enc_result_bsgs = T_dy[0].copy()
    for dy in range(1, 3):
        shift = dy * 4
        t_shifted = T_dy[dy].copy()
        t_shifted.rotate(shift)
        enc_result_bsgs += t_shifted
        rotations += 1

    # 5. 解密与验证
    res_bsgs = extract_2x2_from_flat(enc_result_bsgs.decrypt())
    
    print(f"\n[BSGS 策略] 旋转次数: {rotations}")
    print("[BSGS 策略] 解密结果:\n", np.round(res_bsgs, 4))
    
    assert np.allclose(expected_Y, res_bsgs, atol=1e-3), "验证失败！"
    print("✅ 验证成功：达到理论极小值(4次)，且密文卷积结果与明文预期一致！")

if __name__ == "__main__":
    main()
