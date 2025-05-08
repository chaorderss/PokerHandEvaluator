import ctypes
import numpy as np
import os

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

        # 设置函数参数和返回值类型 - 原始需要预分配内存的函数
        self.lib.get_hole_card_2_idx_lut.argtypes = [ctypes.POINTER(ctypes.c_int16)]
        self.lib.get_hole_card_2_idx_lut.restype = None

        self.lib.get_idx_2_hole_card_lut.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_idx_2_hole_card_lut.restype = None

        # 旧版函数
        self.lib.get_idx_2_flop_lut.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_idx_2_flop_lut.restype = None

        self.lib.get_idx_2_turn_lut.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_idx_2_turn_lut.restype = None

        self.lib.get_idx_2_river_lut.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_idx_2_river_lut.restype = None

        # 设置新版函数的参数和返回值类型
        self.lib.get_idx_2_flop_lut_v2.argtypes = []
        self.lib.get_idx_2_flop_lut_v2.restype = ctypes.POINTER(ctypes.c_int8)

        self.lib.get_idx_2_turn_lut_v2.argtypes = []
        self.lib.get_idx_2_turn_lut_v2.restype = ctypes.POINTER(ctypes.c_int8)

        self.lib.get_idx_2_river_lut_v2.argtypes = []
        self.lib.get_idx_2_river_lut_v2.restype = ctypes.POINTER(ctypes.c_int8)

        self.lib.get_1d_card.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_1d_card.restype = ctypes.c_int8

        self.lib.get_2d_card.argtypes = [ctypes.c_int8, ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_2d_card.restype = None

    def get_hole_card_2_idx_lut(self):
        """获取从底牌到索引的查找表（52x52）"""
        lut = np.full((self.N_CARDS_IN_DECK, self.N_CARDS_IN_DECK),
                      fill_value=-2, dtype=np.int16)
        self.lib.get_hole_card_2_idx_lut(lut.ctypes.data_as(ctypes.POINTER(ctypes.c_int16)))
        return lut
    def np_2d_arr_to_c(self,np_2d_arr):
        # 确保数组是连续的内存布局
        if not np_2d_arr.flags['C_CONTIGUOUS']:
            np_2d_arr = np.ascontiguousarray(np_2d_arr)
        # 直接返回 ctypes 指针
        return np_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))

    def get_idx_2_hole_card_lut(self):
        """获取从索引到底牌的查找表（1326x2）"""
        lut = np.full((self.RANGE_SIZE, 2), fill_value=-2, dtype=np.int8)
        self.lib.get_idx_2_hole_card_lut(lut.ctypes.data_as(ctypes.POINTER(ctypes.c_int8)))
        return lut

    def get_idx_2_hole_card_lut2(self):
        lut = np.full(shape=(self.RANGE_SIZE, 2), fill_value=-2, dtype=np.int8)
        self.lib.get_idx_2_hole_card_lut(self.np_2d_arr_to_c(lut))  # fills it
        return lut

    def get_idx_2_flop_lut(self):
        """获取从索引到翻牌的查找表"""
        # 使用新版无需预分配内存的函数
        n_flops = 22100  # 52选3 = 22100种可能的翻牌

        # 直接调用C函数获取指针
        ptr = self.lib.get_idx_2_flop_lut_v2()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 创建新的NumPy数组并复制数据
        result = np.zeros((n_flops, 3), dtype=np.int8)
        for i in range(n_flops):
            for j in range(3):
                result[i, j] = ptr[i * 3 + j]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

    def get_idx_2_turn_lut(self):
        """获取从索引到转牌的查找表（示例）"""
        # 使用新版无需预分配内存的函数
        n_turns = 1000  # 这里只获取1000个示例

        # 直接调用C函数获取指针
        ptr = self.lib.get_idx_2_turn_lut_v2()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 创建新的NumPy数组并复制数据
        result = np.zeros((n_turns, 4), dtype=np.int8)
        for i in range(n_turns):
            for j in range(4):
                result[i, j] = ptr[i * 4 + j]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

    def get_idx_2_river_lut(self):
        """获取从索引到河牌的查找表（示例）"""
        # 使用新版无需预分配内存的函数
        n_rivers = 100  # 这里只获取100个示例

        # 直接调用C函数获取指针
        ptr = self.lib.get_idx_2_river_lut_v2()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 创建新的NumPy数组并复制数据
        result = np.zeros((n_rivers, 5), dtype=np.int8)
        for i in range(n_rivers):
            for j in range(5):
                result[i, j] = ptr[i * 5 + j]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

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

    def convert_hole_cards_to_index(self, card1, card2):
        """将两张底牌转换为索引（0-1325）"""
        lut = self.get_hole_card_2_idx_lut()
        return lut[card1, card2]

    def index_to_hole_cards(self, index):
        """将索引转换为两张底牌"""
        lut = self.get_idx_2_hole_card_lut()
        return lut[index]
if __name__ == "__main__":
    lut = PokerLUT()

    print(lut.get_idx_2_hole_card_lut2())
    # print(lut.get_hole_card_2_idx_lut())
    # print(lut.get_idx_2_flop_lut())
    # print(lut.get_idx_2_turn_lut())
    # print(lut.get_idx_2_river_lut())

    # print(lut.get_1d_card(np.array([12, 0], dtype=np.int8)))
    # print(lut.get_2d_card(12))

    # gcc -dynamiclib luts.c -o lib_luts.dylib
    # gcc -shared -o lib_luts.so -fPIC luts.c
