import ctypes
import numpy as np
import os

# 加载动态库
lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "lib_hand_eval.dylib"))

# 正确的 numpy 数组转 ctypes 指针方法
def np_2d_arr_to_c(np_2d_arr):
    # 确保数组是连续的内存布局
    if not np_2d_arr.flags['C_CONTIGUOUS']:
        np_2d_arr = np.ascontiguousarray(np_2d_arr)
    # 直接返回 ctypes 指针
    return np_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))

def np_2d_arr_to_c_old(np_2d_arr):
        return (np_2d_arr.__array_interface__['data'][0]
                + np.arange(np_2d_arr.shape[0]) * np_2d_arr.strides[0]).astype(np.intp)

# 定义参数类型
lib.get_hand_rank_holdem.argtypes = [
    ctypes.POINTER(ctypes.c_int8),  # hand_2d: int* (2*2)
    ctypes.POINTER(ctypes.c_int8)   # board_2d: int* (5*2)
]
lib.get_hand_rank_holdem.restype = ctypes.c_int

# 也可以定义批量接口
lib.get_hand_rank_all_hands_on_given_boards_holdem.argtypes = [
    ctypes.POINTER(ctypes.c_int8),  # int* hand_ranks
    ctypes.POINTER(ctypes.c_int8),  # int* boards_2d
    ctypes.c_int,                 # int n_boards
    ctypes.POINTER(ctypes.c_int8),  # int* lut_hole_cards
    ctypes.POINTER(ctypes.c_int8)   # int* lut_1dcard_2d
]
lib.get_hand_rank_all_hands_on_given_boards_holdem.restype = None

# 牌的描述
def rank_to_string(rank):
    if rank == 1:  # 最强牌
        return "皇家同花顺 (Royal Flush)"
    elif rank < 11:  # 同花顺
        return f"同花顺 (Straight Flush) - 排名 {rank}"
    elif rank < 167:  # 四条
        return f"四条 (Four of a Kind) - 排名 {rank}"
    elif rank < 323:  # 葫芦
        return f"葫芦 (Full House) - 排名 {rank}"
    elif rank < 1600:  # 同花
        return f"同花 (Flush) - 排名 {rank}"
    elif rank < 1610:  # 顺子
        return f"顺子 (Straight) - 排名 {rank}"
    elif rank < 2468:  # 三条
        return f"三条 (Three of a Kind) - 排名 {rank}"
    elif rank < 3326:  # 两对
        return f"两对 (Two Pair) - 排名 {rank}"
    elif rank < 6186:  # 一对
        return f"一对 (One Pair) - 排名 {rank}"
    else:  # 高牌
        return f"高牌 (High Card) - 排名 {rank}"

# 构造手牌和公共牌
# 例如: 手牌 [[12,0],[11,0]] (A♣, K♣), 公共牌 [[10,0],[9,0],[8,0],[0,1],[1,2]] (Q♣, J♣, 10♣, 2♦, 3♥)
hand = np.array([[1, 2], [5, 0]], dtype=np.int32)
board = np.array([[8, 0], [12, 3], [10, 3], [3, 3], [2, 3]], dtype=np.int32)

# 用函数简化调用
rank = lib.get_hand_rank_holdem(np_2d_arr_to_c(hand), np_2d_arr_to_c(board))
print("手牌排名:", rank)
print("牌型:", rank_to_string(rank))

# 测试最强牌和最弱牌
strongest_hand = np.array([[1, 2], [10, 2]], dtype=np.int32)  # A♣ K♣
strongest_board = np.array([[11, 2], [1, 0], [6, 0], [5, 2], [9, 0]], dtype=np.int32)  # Q♣ J♣ 10♣ 9♣ 8♣

weakest_hand = np.array([[0, 0], [2, 1]], dtype=np.int32)  # 2♣ 4♦
weakest_board = np.array([[3, 2], [5, 3], [7, 0], [11, 1], [12, 2]], dtype=np.int32)  # 5♥ 7♠ 9♣ K♦ A♥

rank1 = lib.get_hand_rank_holdem(np_2d_arr_to_c(strongest_hand), np_2d_arr_to_c(strongest_board))
rank2 = lib.get_hand_rank_holdem(np_2d_arr_to_c(weakest_hand), np_2d_arr_to_c(weakest_board))

print("\n最强牌排名:", rank1)
print("牌型:", rank_to_string(rank1))

print("\n最弱牌排名:", rank2)
print("牌型:", rank_to_string(rank2))

print("\n提示: 数字越小牌力越强，1最强，7462最弱")

# gcc -dynamiclib hand_eval.c -I../include -L../build -lpheval -o lib_hand_eval.dylib