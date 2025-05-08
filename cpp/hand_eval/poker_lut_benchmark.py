import ctypes
import numpy as np
import os
import time

class PokerLUT:
    """扑克牌查找表工具"""

    def __init__(self, lib_path=None):
        if lib_path is None:
            if os.name == 'nt':  # Windows
                lib_name = "luts.dll"
            elif os.name == 'posix':  # Linux/Mac
                if 'darwin' in os.sys.platform:  # Mac
                    lib_name = "lib_luts.dylib"
                else:  # Linux
                    lib_name = "lib_luts.so"
            lib_path = os.path.join(os.path.dirname(__file__), lib_name)

        # 加载库
        self.lib = ctypes.CDLL(lib_path)

        # 定义常量
        self.N_CARDS_IN_DECK = 52
        self.RANGE_SIZE = 1326  # 52选2

        # 设置函数参数和返回值类型
        self.lib.get_1d_card.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_1d_card.restype = ctypes.c_int8

        self.lib.get_2d_card.argtypes = [ctypes.c_int8, ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_2d_card.restype = None

    def get_1d_card(self, card_2d):
        """将2D牌表示转换为1D索引"""
        card_2d_arr = np.ascontiguousarray(card_2d, dtype=np.int8)
        return self.lib.get_1d_card(card_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int8)))

    def get_2d_card(self, card_1d):
        """将1D牌索引转换为2D表示"""
        card_2d = np.zeros(2, dtype=np.int8)
        self.lib.get_2d_card(ctypes.c_int8(card_1d),
                            card_2d.ctypes.data_as(ctypes.POINTER(ctypes.c_int8)))
        return card_2d

# 纯Python实现，用于比较
def get_1d_card_py(card_2d):
    """Python实现的2D到1D转换"""
    return card_2d[0] * 4 + card_2d[1]

def get_2d_card_py(card_1d):
    """Python实现的1D到2D转换"""
    return np.array([card_1d // 4, card_1d % 4], dtype=np.int8)

if __name__ == "__main__":
    try:
        lut = PokerLUT()

        # 基本测试
        print("基本功能测试:")
        card_2d = np.array([12, 0], dtype=np.int8)  # A♣
        card_1d = lut.get_1d_card(card_2d)
        print(f"2D卡牌 {card_2d} 转换为1D: {card_1d}")

        card_2d_back = lut.get_2d_card(card_1d)
        print(f"1D卡牌 {card_1d} 转换为2D: {card_2d_back}")

        # 性能测试1: C库 2D->1D
        print("\n性能测试 - 100,000次卡牌转换:")

        num_tests = 1000000
        random_cards_2d = np.random.randint(0, [13, 4], size=(num_tests, 2), dtype=np.int8)

        start_time = time.time()
        for i in range(num_tests):
            lut.get_1d_card(random_cards_2d[i])
        c_time_1d = time.time() - start_time
        print(f"C库 2D->1D 转换 {num_tests}次用时: {c_time_1d:.6f}秒")

        # 性能测试2: C库 1D->2D
        random_cards_1d = np.random.randint(0, 52, size=num_tests, dtype=np.int8)

        start_time = time.time()
        for i in range(num_tests):
            lut.get_2d_card(random_cards_1d[i])
        c_time_2d = time.time() - start_time
        print(f"C库 1D->2D 转换 {num_tests}次用时: {c_time_2d:.6f}秒")

        # 性能测试3: Python 2D->1D
        start_time = time.time()
        for i in range(num_tests):
            get_1d_card_py(random_cards_2d[i])
        py_time_1d = time.time() - start_time
        print(f"Python 2D->1D 转换 {num_tests}次用时: {py_time_1d:.6f}秒")

        # 性能测试4: Python 1D->2D
        start_time = time.time()
        for i in range(num_tests):
            get_2d_card_py(random_cards_1d[i])
        py_time_2d = time.time() - start_time
        print(f"Python 1D->2D 转换 {num_tests}次用时: {py_time_2d:.6f}秒")

        # 总结
        print("\n性能比较:")
        print(f"C库总时间: {c_time_1d + c_time_2d:.6f}秒")
        print(f"Python总时间: {py_time_1d + py_time_2d:.6f}秒")
        print(f"C库比Python快 {(py_time_1d + py_time_2d) / (c_time_1d + c_time_2d):.2f}倍")

    except Exception as e:
        print(f"错误: {e}")
        print("\n可能的原因:")
        print("1. 动态库不存在或无法加载")
        print("2. 动态库中的函数签名不匹配")
        print("3. 编译错误")

        # 检查文件是否存在
        lib_path = os.path.join(os.path.dirname(__file__), "lib_luts.dylib")
        if os.path.exists(lib_path):
            print(f"\n库文件存在: {lib_path}")
        else:
            print(f"\n库文件不存在: {lib_path}")