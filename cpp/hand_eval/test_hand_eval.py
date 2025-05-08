import ctypes
import numpy as np
import os

# 加载动态库
print(os.name)
if os.name == 'posix':
    import platform
    if platform.system() == 'Darwin':
        lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "lib_hand_eval.dylib"))
    else:
        lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "lib_hand_eval.so"))
else:
    lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "lib_hand_eval.dll"))

# 定义参数类型
lib.get_hand_rank_holdem.argtypes = [
    ctypes.POINTER(ctypes.c_int),  # hand_2d: int* (2*2)
    ctypes.POINTER(ctypes.c_int)   # board_2d: int* (5*2)
]
lib.get_hand_rank_holdem.restype = ctypes.c_int

def np_2d_arr_to_c(np_2d_arr):
    return (np_2d_arr.__array_interface__['data'][0]
            + np.arange(np_2d_arr.shape[0]) * np_2d_arr.strides[0]).astype(np.intp)
# 构造手牌和公共牌
# 例如: 手牌 [[12,0],[11,0]] (A♣, K♣), 公共牌 [[10,0],[9,0],[8,0],[0,1],[1,2]] (Q♣, J♣, 10♣, 2♦, 3♥)
hand = np.array([[7, 0], [11, 0]], dtype=np.int32)
board = np.array([[10, 0], [9, 0], [8, 0], [0, 1], [1, 2]], dtype=np.int32)

# 直接传递 numpy 指针
rank = lib.get_hand_rank_holdem(hand.ctypes.data_as(ctypes.POINTER(ctypes.c_int)),
                                board.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
print("手牌排名:", rank)

# gcc -dynamiclib hand_eval.c -I../include -L../build -lpheval -o lib_hand_eval.dylib