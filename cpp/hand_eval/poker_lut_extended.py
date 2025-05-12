import ctypes
import numpy as np
import os
import time

class PokerLUT:
    """扑克牌查找表工具 - 扩展版本包含更多德州扑克相关函数"""

    # 扑克轮次定义
    PREFLOP = 0
    FLOP = 1
    TURN = 2
    RIVER = 3

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
        self.N_ROUNDS = 4  # preflop, flop, turn, river

        # 设置函数参数和返回值类型 - 基本函数
        self.lib.get_hole_card_2_idx_lut.argtypes = [ctypes.POINTER(ctypes.c_int16)]
        self.lib.get_hole_card_2_idx_lut.restype = None

        self.lib.get_idx_2_hole_card_lut.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_idx_2_hole_card_lut.restype = None

        self.lib.get_1d_card.argtypes = [ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_1d_card.restype = ctypes.c_int8

        self.lib.get_2d_card.argtypes = [ctypes.c_int8, ctypes.POINTER(ctypes.c_int8)]
        self.lib.get_2d_card.restype = None

        # 设置扩展函数的参数和返回值类型 - 新版无参数函数
        self.lib.get_card_in_what_range_idxs_lut.argtypes = []
        self.lib.get_card_in_what_range_idxs_lut.restype = ctypes.POINTER(ctypes.c_int16)

        self.lib.get_n_idxs_per_card.argtypes = []
        self.lib.get_n_idxs_per_card.restype = ctypes.POINTER(ctypes.c_int16)

        self.lib.get_n_boards_lut.argtypes = []
        self.lib.get_n_boards_lut.restype = ctypes.POINTER(ctypes.c_int32)

        self.lib.get_n_cards_out_at_lut.argtypes = []
        self.lib.get_n_cards_out_at_lut.restype = ctypes.POINTER(ctypes.c_int8)

        self.lib.get_n_cards_dealt_in_transition_to_lut.argtypes = []
        self.lib.get_n_cards_dealt_in_transition_to_lut.restype = ctypes.POINTER(ctypes.c_int8)

        self.lib.get_n_board_branches_lut.argtypes = []
        self.lib.get_n_board_branches_lut.restype = ctypes.POINTER(ctypes.c_int32)

    def np_2d_arr_to_c(np_2d_arr):
        # 确保数组是连续的内存布局
        if not np_2d_arr.flags['C_CONTIGUOUS']:
            np_2d_arr = np.ascontiguousarray(np_2d_arr)
        # 直接返回 ctypes 指针
        return np_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int))

    # 基本函数
    def get_hole_card_2_idx_lut(self):
        """获取从底牌到索引的查找表（52x52）"""
        lut = np.full((self.N_CARDS_IN_DECK, self.N_CARDS_IN_DECK),
                      fill_value=-2, dtype=np.int16)
        self.lib.get_hole_card_2_idx_lut(lut.ctypes.data_as(ctypes.POINTER(ctypes.c_int16)))
        return lut

    def get_idx_2_hole_card_lut(self):
        """获取从索引到底牌的查找表（1326x2）"""
        lut = np.full((self.RANGE_SIZE, 2), fill_value=-2, dtype=np.int8)
        self.lib.get_idx_2_hole_card_lut(lut.ctypes.data_as(ctypes.POINTER(ctypes.c_int8)))
        return lut

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

    # 扩展函数 - C库版本（修改为直接调用无参数C函数）
    def get_card_in_what_range_idxs_lut(self):
        """
        获取每张牌所在的所有手牌索引
        返回: list[52] 每个元素是一个np.array，包含该牌所在的所有手牌索引
        """
        # 获取每张牌的索引数量
        n_idxs_ptr = self.lib.get_n_idxs_per_card()
        if not n_idxs_ptr:
            raise MemoryError("C库内存分配失败")

        n_idxs = np.array([n_idxs_ptr[i] for i in range(self.N_CARDS_IN_DECK)], dtype=np.int16)

        # 获取索引数据
        data_ptr = self.lib.get_card_in_what_range_idxs_lut()
        if not data_ptr:
            ctypes.CDLL('libc.dylib').free(n_idxs_ptr)
            raise MemoryError("C库内存分配失败")

        # 转换为列表，每个元素是该牌所在的所有手牌索引数组
        result = []
        offset = 0
        for i in range(self.N_CARDS_IN_DECK):
            size = n_idxs[i]
            # 从C指针复制数据到NumPy数组
            card_indices = np.array([data_ptr[offset + j] for j in range(size)], dtype=np.int16)
            result.append(card_indices)
            offset += size

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(n_idxs_ptr)
            ctypes.CDLL('libc.dylib').free(data_ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(n_idxs_ptr))
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(data_ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(n_idxs_ptr)
            ctypes.CDLL('libc.so.6').free(data_ptr)

        return result

    def get_n_boards_lut(self):
        """
        获取每个轮次的可能公共牌数量
        返回: dict {round_idx: n_boards}
        """
        # 直接调用C函数
        ptr = self.lib.get_n_boards_lut()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 转换为字典
        result = {}
        for i in range(self.N_ROUNDS):
            result[i] = ptr[i]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

    def get_n_cards_out_at_lut(self):
        """
        获取每个轮次（包含轮次本身）已经发出的牌数
        返回: dict {round_idx: n_cards_out}
        """
        # 直接调用C函数
        ptr = self.lib.get_n_cards_out_at_lut()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 转换为字典
        result = {}
        for i in range(self.N_ROUNDS):
            result[i] = ptr[i]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

    def get_n_cards_dealt_in_transition_to_lut(self):
        """
        获取进入每个轮次时额外发出的牌数
        返回: dict {round_idx: n_cards_dealt}
        """
        # 直接调用C函数
        ptr = self.lib.get_n_cards_dealt_in_transition_to_lut()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 转换为字典
        result = {}
        for i in range(self.N_ROUNDS):
            result[i] = ptr[i]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

    def get_n_board_branches_lut(self):
        """
        获取每个轮次可能的公共牌分支数
        返回: dict {round_idx: n_branches}
        """
        # 直接调用C函数
        ptr = self.lib.get_n_board_branches_lut()
        if not ptr:
            raise MemoryError("C库内存分配失败")

        # 转换为字典
        result = {}
        for i in range(self.N_ROUNDS):
            result[i] = ptr[i]

        # 释放C库分配的内存
        if 'darwin' in os.sys.platform:  # MacOS
            ctypes.CDLL('libc.dylib').free(ptr)
        elif os.name == 'nt':  # Windows
            ctypes.windll.kernel32.HeapFree(ctypes.c_void_p(ptr))
        else:  # Linux和其他POSIX系统
            ctypes.CDLL('libc.so.6').free(ptr)

        return result

    # NumPy 版本实现 - 纯Python/NumPy实现，可与C库版本比较性能
    def get_card_in_what_range_idxs_lut_numpy(self):
        """NumPy版本实现 - 获取每张牌所在的所有手牌索引"""
        result = [[] for _ in range(self.N_CARDS_IN_DECK)]

        # 遍历所有可能的手牌组合
        idx = 0
        for c1 in range(self.N_CARDS_IN_DECK):
            for c2 in range(c1 + 1, self.N_CARDS_IN_DECK):
                result[c1].append(idx)
                result[c2].append(idx)
                idx += 1

        # 转换为numpy数组
        for i in range(self.N_CARDS_IN_DECK):
            result[i] = np.array(result[i], dtype=np.int16)

        return result

    def get_n_boards_lut_numpy(self):
        """NumPy版本实现 - 获取每个轮次的可能公共牌数量"""
        def nCk(n, k):
            """组合数计算"""
            if k < 0 or k > n:
                return 0
            if k > n - k:
                k = n - k

            result = 1
            for i in range(k):
                result *= (n - i)
                result //= (i + 1)
            return result

        result = {}
        result[self.PREFLOP] = 1  # 没有公共牌 = 1种可能
        result[self.FLOP] = nCk(self.N_CARDS_IN_DECK - 2, 3)  # 从50张牌中选3张
        result[self.TURN] = nCk(self.N_CARDS_IN_DECK - 2, 4)  # 从50张牌中选4张
        result[self.RIVER] = nCk(self.N_CARDS_IN_DECK - 2, 5)  # 从50张牌中选5张

        return result

    def get_n_cards_out_at_lut_numpy(self):
        """NumPy版本实现 - 获取每个轮次已经发出的牌数"""
        result = {
            self.PREFLOP: 2,  # 2张底牌
            self.FLOP: 5,     # 2底牌 + 3翻牌
            self.TURN: 6,     # 5张 + 1转牌
            self.RIVER: 7     # 6张 + 1河牌
        }
        return result

    def get_n_cards_dealt_in_transition_to_lut_numpy(self):
        """NumPy版本实现 - 获取进入每个轮次时额外发出的牌数"""
        result = {
            self.PREFLOP: 2,  # 发2张底牌
            self.FLOP: 3,     # 发3张翻牌
            self.TURN: 1,     # 发1张转牌
            self.RIVER: 1     # 发1张河牌
        }
        return result

    def get_n_board_branches_lut_numpy(self):
        """NumPy版本实现 - 获取每个轮次可能的公共牌分支数"""
        def nCk(n, k):
            """组合数计算"""
            if k < 0 or k > n:
                return 0
            if k > n - k:
                k = n - k

            result = 1
            for i in range(k):
                result *= (n - i)
                result //= (i + 1)
            return result

        result = {}
        result[self.PREFLOP] = 1  # 没有分支
        result[self.FLOP] = nCk(self.N_CARDS_IN_DECK - 2, 3)  # 从50张牌中选3张
        result[self.TURN] = self.N_CARDS_IN_DECK - 5  # 从47张牌中选1张 = 47
        result[self.RIVER] = self.N_CARDS_IN_DECK - 6  # 从46张牌中选1张 = 46

        return result

def np_2d_arr_to_c(np_2d_arr):
    # 确保数组是连续的内存布局
    if not np_2d_arr.flags['C_CONTIGUOUS']:
        np_2d_arr = np.ascontiguousarray(np_2d_arr)
    # 直接返回 ctypes 指针
    return np_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int))


if __name__ == "__main__":
    lut = PokerLUT()

    print(lut.get_n_boards_lut())
    print(lut.get_n_cards_out_at_lut())
    print(lut.get_n_cards_dealt_in_transition_to_lut())

