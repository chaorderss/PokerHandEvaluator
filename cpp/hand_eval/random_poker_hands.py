import ctypes
import numpy as np
import os
import random
from collections import Counter
import time

def np_2d_arr_to_c_int8(np_2d_arr):
    """将NumPy二维数组转换为C指针 (int8)"""
    # 确保数组是连续的内存布局
    if not np_2d_arr.flags['C_CONTIGUOUS']:
        np_2d_arr = np.ascontiguousarray(np_2d_arr)
    # 直接返回 ctypes 指针
    return np_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))

def np_2d_arr_to_c_int(np_2d_arr):
    """将NumPy二维数组转换为C指针 (int)"""
    # 确保数组是连续的内存布局
    if not np_2d_arr.flags['C_CONTIGUOUS']:
        np_2d_arr = np.ascontiguousarray(np_2d_arr)
    # 直接返回 ctypes 指针
    return np_2d_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int))

def rank_to_string(rank):
    """将手牌等级转换为可读的牌型描述"""
    if rank <= 10:
        return "皇家同花顺"
    elif rank <= 166:
        return "同花顺"
    elif rank <= 322:
        return "四条"
    elif rank <= 1599:
        return "葫芦"
    elif rank <= 1609:
        return "同花"
    elif rank <= 2467:
        return "顺子"
    elif rank <= 3325:
        return "三条"
    elif rank <= 6185:
        return "两对"
    elif rank <= 7462:
        return "一对"
    else:
        return "高牌"

def generate_random_hand(dtype=np.int32):
    """生成随机的两张底牌"""
    cards = []
    while len(cards) < 2:
        # 牌面值: 0-12 (对应 2-A)，花色: 0-3 (梅花、方块、红桃、黑桃)
        rank = random.randint(0, 12)
        suit = random.randint(0, 3)
        card = [rank, suit]
        # 确保不重复
        if card not in cards:
            cards.append(card)
    return np.array(cards, dtype=dtype)

def generate_random_board(dtype=np.int32):
    """生成随机的5张公共牌"""
    cards = []
    while len(cards) < 5:
        rank = random.randint(0, 12)
        suit = random.randint(0, 3)
        card = [rank, suit]
        if card not in cards:
            cards.append(card)
    return np.array(cards, dtype=dtype)

def generate_random_hand_board_pair(dtype=np.int32):
    """生成不冲突的随机底牌和公共牌组合"""
    hand = generate_random_hand(dtype)
    board = []

    hand_list = hand.tolist()

    while len(board) < 5:
        rank = random.randint(0, 12)
        suit = random.randint(0, 3)
        card = [rank, suit]
        if card not in hand_list and card not in board:
            board.append(card)

    return hand, np.array(board, dtype=dtype)

def card_to_string(card):
    """将牌的数值表示转换为可读字符串"""
    ranks = ['2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K', 'A']
    suits = ['♣', '♦', '♥', '♠']
    return f"{ranks[card[0]]}{suits[card[1]]}"

def hand_to_string(hand):
    """将手牌数组转换为可读字符串"""
    return ' '.join(card_to_string(card) for card in hand)

def test_single_hand(lib, dtype=np.int32, use_int8_version=False):
    """测试单手牌，检查C库是否能正确处理"""
    # 生成一手测试牌
    hand = np.array([[0, 0], [1, 1]], dtype=dtype)  # 2♣ 3♦
    board = np.array([[2, 2], [3, 3], [4, 0], [5, 1], [6, 2]], dtype=dtype)  # 4♥ 5♠ 6♣ 7♦ 8♥

    print(f"测试单手牌 - 使用数据类型: {dtype.__name__}, {'使用int8_t版本' if use_int8_version else '使用标准版本'}")
    print(f"底牌: {hand_to_string(hand)}")
    print(f"公共牌: {hand_to_string(board)}")

    try:
        if use_int8_version:
            # 使用专为int8_t类型设计的函数
            rank = lib.get_hand_rank_holdem_int8(np_2d_arr_to_c_int8(hand), np_2d_arr_to_c_int8(board))
        else:
            # 使用标准的int类型函数
            if dtype == np.int8:
                # 如果使用np.int8且没有int8版本函数，通常会出错
                rank = lib.get_hand_rank_holdem(np_2d_arr_to_c_int8(hand), np_2d_arr_to_c_int8(board))
            else:
                rank = lib.get_hand_rank_holdem(np_2d_arr_to_c_int(hand), np_2d_arr_to_c_int(board))

        print(f"结果: {rank_to_string(rank)} (排名: {rank})")
        print("测试成功！\n")
        return True
    except Exception as e:
        print(f"测试失败: {e}\n")
        return False

def main():
    # 加载库文件
    lib_path = os.path.join(os.path.dirname(__file__), "lib_hand_eval.dylib")
    if not os.path.exists(lib_path):
        print(f"找不到库文件: {lib_path}")
        return

    lib = ctypes.CDLL(lib_path)

    # 设置函数参数和返回值类型 - 标准函数
    lib.get_hand_rank_holdem.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.get_hand_rank_holdem.restype = ctypes.c_int

    # 设置函数参数和返回值类型 - int8_t版本
    try:
        lib.get_hand_rank_holdem_int8.argtypes = [ctypes.POINTER(ctypes.c_int8), ctypes.POINTER(ctypes.c_int8)]
        lib.get_hand_rank_holdem_int8.restype = ctypes.c_int
        has_int8_version = True
        print("检测到int8_t版本函数，将优先使用它\n")
    except AttributeError:
        has_int8_version = False
        print("未检测到int8_t版本函数，将使用标准版本\n")

    # 测试不同的数据类型和函数版本
    success = False
    best_config = None

    test_configs = [
        # (数据类型, 是否使用int8版本)
        # (np.int32, False),  # 标准int版本 + np.int32
        (np.int8, has_int8_version),  # int8_t版本 + np.int8 (如果有)
        # (np.int32, has_int8_version),  # int8_t版本 + np.int32 (如果有)
        # (np.int8, False)  # 尝试标准版本 + np.int8 (可能会失败)
    ]

    for dtype, use_int8 in test_configs:
        # 如果配置要求int8版本但没有，则跳过
        if use_int8 and not has_int8_version:
            continue

        if test_single_hand(lib, dtype, use_int8):
            success = True
            best_config = (dtype, use_int8)
            print(f"找到有效组合: dtype={dtype.__name__}, use_int8={use_int8}")
            break

    if not success:
        print("所有组合都失败了，请检查C库接口定义和编译选项")
        return

    # 使用找到的成功组合生成1000手随机牌
    dtype, use_int8 = best_config
    print(f"\n使用配置: dtype={dtype.__name__}, use_int8={use_int8}")
    print("正在生成1000手随机牌并比较大小...")
    num_hands = 100000
    hands = []
    start_time = time.time()

    for i in range(num_hands):
        hand, board = generate_random_hand_board_pair(dtype)
        try:
            # 如果需要监控进度，可以每100手打印一次
            if i % 100 == 0:
                print(f"已处理 {i} 手...")

            if use_int8:
                rank = lib.get_hand_rank_holdem_int8(np_2d_arr_to_c_int8(hand), np_2d_arr_to_c_int8(board))
            else:
                if dtype == np.int8:
                    rank = lib.get_hand_rank_holdem(np_2d_arr_to_c_int8(hand), np_2d_arr_to_c_int8(board))
                else:
                    rank = lib.get_hand_rank_holdem(np_2d_arr_to_c_int(hand), np_2d_arr_to_c_int(board))

            hands.append((rank, hand, board))
        except Exception as e:
            print(f"处理第 {i+1} 手时出错: {e}")
            print(f"底牌: {hand}")
            print(f"公共牌: {board}")
            break

    if len(hands) == num_hands:
        end_time = time.time()

        # 按牌力排序（从强到弱）
        hands.sort(key=lambda x: x[0])

        # 统计各种牌型的数量
        hand_types = Counter([rank_to_string(rank) for rank, _, _ in hands])

        # 显示统计信息
        print(f"\n完成! 处理时间: {end_time - start_time:.4f} 秒")
        print(f"平均每手牌评估时间: {(end_time - start_time) / num_hands * 1000:.4f} 毫秒")

        print("\n牌型分布:")
        for hand_type, count in hand_types.most_common():
            print(f"{hand_type}: {count} 手 ({count/num_hands*100:.2f}%)")

        # 显示最强的5手牌
        print("\n最强的5手牌:")
        for i in range(5):
            if i < len(hands):
                rank, hand, board = hands[i]
                print(f"{i+1}. 底牌: {hand_to_string(hand)}, 公共牌: {hand_to_string(board)}, 牌型: {rank_to_string(rank)} (排名: {rank})")

        # 显示最弱的5手牌
        print("\n最弱的5手牌:")
        for i in range(1, 6):
            if i <= len(hands):
                rank, hand, board = hands[-i]
                print(f"{i}. 底牌: {hand_to_string(hand)}, 公共牌: {hand_to_string(board)}, 牌型: {rank_to_string(rank)} (排名: {rank})")

if __name__ == "__main__":
    main()