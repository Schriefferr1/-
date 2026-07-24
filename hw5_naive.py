import tenseal as ts
import numpy as np

def extract_2x2_from_flat(decrypted_vector):
    return np.array([
        decrypted_vector[0], decrypted_vector[1], 
        decrypted_vector[4], decrypted_vector[5]
    ]).reshape(2, 2)

def main():
    print("=== 作业5：单输入单输出 4x4 输入和 3x3 卷积核密文卷积 (朴素实现) ===")
    
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
    
    print("输入矩阵 X (4x4):\n", X)
    print("卷积核 W (3x3):\n", W)

    expected_Y = np.zeros((2, 2))
    for i in range(2):
        for j in range(2):
            expected_Y[i, j] = np.sum(X[i:i+3, j:j+3] * W)
    print("\n[Ground Truth] 明文期望结果 (2x2):\n", expected_Y)

    # 3. 加密数据
    x_flat = X.flatten().tolist()
    enc_x = ts.ckks_vector(context, x_flat)

    # 4. 朴素卷积计算 (8次旋转)
    enc_result_naive = None
    rotations = 0
    
    for dy in range(3):
        for dx in range(3):
            weight = W[dy, dx]
            shift = dy * 4 + dx
            
            x_shifted = enc_x.copy()
            if shift > 0:
                x_shifted.rotate(shift)
                rotations += 1
                
            term = x_shifted * weight
            if enc_result_naive is None:
                enc_result_naive = term
            else:
                enc_result_naive += term

    # 5. 解密与验证
    res_naive = extract_2x2_from_flat(enc_result_naive.decrypt())
    
    print(f"\n[Naive 策略] 旋转次数: {rotations}")
    print("[Naive 策略] 解密结果:\n", np.round(res_naive, 4))
    
    assert np.allclose(expected_Y, res_naive, atol=1e-3), "验证失败！"
    print("✅ 验证成功：密文卷积结果与明文预期一致！")

if __name__ == "__main__":
    main()
