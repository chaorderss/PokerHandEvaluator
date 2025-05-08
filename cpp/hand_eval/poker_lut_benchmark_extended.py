import numpy as np
import time
from poker_lut_extended import PokerLUT

def test_function(func_c, func_numpy, times=10, description=""):
    """测试C库与NumPy版本的性能差异"""

    # 测试C库版本
    start_time = time.time()
    for _ in range(times):
        result_c = func_c()
    c_time = time.time() - start_time

    # 测试NumPy版本
    start_time = time.time()
    for _ in range(times):
        result_numpy = func_numpy()
    numpy_time = time.time() - start_time

    # 打印结果
    print(f"\n--- {description} ---")
    print(f"C库版本：{c_time:.6f}秒")
    print(f"NumPy版本：{numpy_time:.6f}秒")
    print(f"速度比：{numpy_time/c_time:.2f}x {'(NumPy更快)' if numpy_time < c_time else '(C库更快)'}")

    # 对结果进行比较
    if isinstance(result_c, dict):
        is_equal = all(result_c[k] == result_numpy[k] for k in result_c.keys())
    elif isinstance(result_c, list):
        is_equal = len(result_c) == len(result_numpy)
        if is_equal:
            for i in range(len(result_c)):
                if not np.array_equal(result_c[i], result_numpy[i]):
                    is_equal = False
                    break
    else:
        is_equal = np.array_equal(result_c, result_numpy)

    print(f"结果一致性: {'✓' if is_equal else '✗'}")
    return result_c, result_numpy

def main():
    try:
        print("加载扑克查找表工具...")
        lut = PokerLUT()

        print("开始测试...")

        # 1. 测试 get_card_in_what_range_idxs_lut
        result_c, result_numpy = test_function(
            lut.get_card_in_what_range_idxs_lut,
            lut.get_card_in_what_range_idxs_lut_numpy,
            times=100,
            description="获取每张牌所在的手牌索引"
        )

        # 打印一些结果示例
        print(f"\n示例：A♠ (索引51) 所在的前5个手牌: {result_c[51][:5]}")

        # 2. 测试 get_n_boards_lut
        result_c, result_numpy = test_function(
            lut.get_n_boards_lut,
            lut.get_n_boards_lut_numpy,
            times=100,
            description="获取每个轮次的可能公共牌数量"
        )

        # 打印结果
        print(f"\n可能的公共牌数量：")
        print(f"Preflop: {result_c[lut.PREFLOP]}")
        print(f"Flop: {result_c[lut.FLOP]}")
        print(f"Turn: {result_c[lut.TURN]}")
        print(f"River: {result_c[lut.RIVER]}")

        # 3. 测试 get_n_cards_out_at_lut
        result_c, result_numpy = test_function(
            lut.get_n_cards_out_at_lut,
            lut.get_n_cards_out_at_lut_numpy,
            times=100,
            description="获取每个轮次已经发出的牌数"
        )

        # 打印结果
        print(f"\n每个轮次已经发出的牌数：")
        print(f"Preflop: {result_c[lut.PREFLOP]} 张")
        print(f"Flop: {result_c[lut.FLOP]} 张")
        print(f"Turn: {result_c[lut.TURN]} 张")
        print(f"River: {result_c[lut.RIVER]} 张")

        # 4. 测试 get_n_cards_dealt_in_transition_to_lut
        result_c, result_numpy = test_function(
            lut.get_n_cards_dealt_in_transition_to_lut,
            lut.get_n_cards_dealt_in_transition_to_lut_numpy,
            times=100,
            description="获取进入每个轮次时额外发出的牌数"
        )

        # 打印结果
        print(f"\n进入每个轮次时额外发出的牌数：")
        print(f"Preflop: {result_c[lut.PREFLOP]} 张")
        print(f"Flop: {result_c[lut.FLOP]} 张")
        print(f"Turn: {result_c[lut.TURN]} 张")
        print(f"River: {result_c[lut.RIVER]} 张")

        # 5. 测试 get_n_board_branches_lut
        result_c, result_numpy = test_function(
            lut.get_n_board_branches_lut,
            lut.get_n_board_branches_lut_numpy,
            times=100,
            description="获取每个轮次可能的公共牌分支数"
        )

        # 打印结果
        print(f"\n每个轮次可能的公共牌分支数：")
        print(f"Preflop: {result_c[lut.PREFLOP]}")
        print(f"Flop: {result_c[lut.FLOP]}")
        print(f"Turn: {result_c[lut.TURN]}")
        print(f"River: {result_c[lut.RIVER]}")

        # 综合性能总结
        print("\n----- 性能总结 -----")
        print("由于简单计算任务中 ctypes 调用开销较大：")
        print("1. 对于简单的固定数值计算（如轮次牌数），NumPy 版本可能更快")
        print("2. 对于复杂的组合计算（如每张牌的索引），C 库版本可能有优势")
        print("3. 函数多次调用时，可考虑缓存结果以避免重复计算")

    except Exception as e:
        print(f"测试过程中发生错误: {e}")
        import traceback
        traceback.print_exc()

        # 检查动态库是否存在
        import os
        lib_path = os.path.join(os.path.dirname(__file__), "lib_luts.dylib")
        if os.path.exists(lib_path):
            print(f"\n库文件存在: {lib_path}")
        else:
            print(f"\n库文件不存在: {lib_path}")

if __name__ == "__main__":
    main()