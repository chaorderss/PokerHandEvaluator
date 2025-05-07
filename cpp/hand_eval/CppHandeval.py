# Copyright (c) 2019 Eric Steinberger


import ctypes
import os
from os.path import join as ospj
import platform  # Import platform module

import numpy as np

from CppWrapper import CppWrapper
from PokerRL.game._.rl_env.game_rules import HoldemRules

def is_colab():
    try:
        import google.colab # type: ignore
        return True
    except ImportError:
        return False

def is_linux():
    """Checks if the current operating system is Ubuntu."""
    return platform.system() == 'Linux' or 'wsl' in platform.release().lower()

class CppHandeval(CppWrapper):

    def __init__(self):
        super().__init__(path_to_dll=ospj(os.path.dirname(os.path.realpath(__file__)),
                                          "lib_hand_eval." + self.CPP_LIB_FILE_ENDING))
        if is_colab() or is_linux():
            self._clib.get_hand_rank_holdem.argtypes = [
                self.ARR_2D_ARG_TYPE,
                self.ARR_2D_ARG_TYPE
            ]
            self._clib.get_hand_rank_holdem.restype = ctypes.c_int32
            self._clib.get_hand_rank_all_hands_on_given_boards_holdem.argtypes = [
            self.ARR_2D_ARG_TYPE,
            self.ARR_2D_ARG_TYPE,
            ctypes.c_int32,
            self.ARR_2D_ARG_TYPE,
            self.ARR_2D_ARG_TYPE
            ]
            self._clib.get_hand_rank_all_hands_on_given_boards_holdem.restype = None



    def get_hand_rank_holdem(self, hand_2d, board_2d):
        """
        Args:
            hand_2d (np.ndarray(shape=[2,2], dtype=int8)):      [rank, suit], [rank, suit]]
            board_2d (np.ndarray(shape=[5,2], dtype=int8)):     [rank, suit], [rank, suit], ...]

        Returns:
            int: integer representing strength of the strongest 5card hand in the 7 cards. higher is better.
        """
        if is_colab() or  is_linux():
            return self._clib.get_hand_rank_holdem(self.np_2d_arr_to_c(hand_2d), self.np_2d_arr_to_c(board_2d))
        else:
            return self._clib.get_hand_rank_52_holdem(self.np_2d_arr_to_c(hand_2d), self.np_2d_arr_to_c(board_2d))

    def get_hand_rank_fhp3(self, hand_2d, board_2d):
        """
        Args:
            hand_2d (np.ndarray(shape=[2,2], dtype=int8)):      [rank, suit], [rank, suit]]
            board_2d (np.ndarray(shape=[3,2], dtype=int8)):     [rank, suit], [rank, suit], ...]

        Returns:
            int: integer representing strength of the strong 5card hand according to standard Texas Hold'em rules.
        """
        return self._clib.get_hand_rank_fhp3(self.np_2d_arr_to_c(hand_2d), self.np_2d_arr_to_c(board_2d))

    def get_hand_rank_all_hands_on_given_boards_holdem(self, boards_1d, lut_holder):
        """
        Args:
            boards_1d (np.ndarray(shape=[N, 5], dtype=int8)):   [[c1, c2, c3, c4, c5], [c1, c2, .., c5], ...}

        Returns:
            np.ndarray(shape=[N, RANGE_SIZE], dtype=int32):     hand_rank for each possible hand; -1 for
                                                                blocked on each of the given boards

        """
        assert len(boards_1d.shape) == 2
        assert boards_1d.shape[1] == 5
        hand_ranks = np.full(shape=(boards_1d.shape[0], HoldemRules.RANGE_SIZE), fill_value=-1, dtype=np.int32)
        self._clib.get_hand_rank_all_hands_on_given_boards_holdem(
            self.np_2d_arr_to_c(hand_ranks),  # int32**
            self.np_2d_arr_to_c(boards_1d),  # int8**
            boards_1d.shape[0],  # int (number of boards)
            self.np_2d_arr_to_c(lut_holder.LUT_IDX_2_HOLE_CARDS),  # int8**
            self.np_2d_arr_to_c(lut_holder.LUT_1DCARD_2_2DCARD)  # int8**
        )
        return hand_ranks

if __name__ == "__main__":
    handeval = CppHandeval()
    from PokerRL.game._.look_up_table import _LutGetterHoldem
    from PokerRL.game.games import DiscretizedNLHoldem
    from PokerRL.game._.look_up_table import CppLibHoldemLuts
    from PokerRL.game._.look_up_table import LutHolderHoldem
    lut_getter = _LutGetterHoldem(env_cls=DiscretizedNLHoldem)

    # luts = CppLibHoldemLuts(n_boards_lut=lut_getter.get_n_boards_LUT(), n_cards_out_lut=lut_getter.get_n_cards_out_at_LUT())
    lut_holder = LutHolderHoldem(env_cls=DiscretizedNLHoldem)
    boards = np.array([
    [0, 1, 2, 3, 4],  # 第一个牌面
    [5, 6, 7, 8, 9]   # 第二个牌面
    ], dtype=np.int8)
    # print(boards.shape)
    # r=handeval.get_hand_rank_all_hands_on_given_boards_holdem(boards, lut_holder)
    # print(r,r.shape)

    # 使用正确的牌面格式
    # 在扑克中，牌的表示通常是：
    # rank: 0-12 (2-A)
    # suit: 0-3 (梅花、方块、红桃、黑桃)
    hand = np.array([[11, 3], [6, 0]], dtype=np.int8)  # 例如：梅花2和梅花3
    board = np.array([[8, 0], [9, 0], [10, 0], [11, 0], [1, 3]], dtype=np.int8)
    # 测试最强手牌（A、K、Q、J、10同花）
    strongest_hand = np.array([[12, 0], [11, 0]], dtype=np.int8)  # A、K同花色
    strongest_board = np.array([[10, 0], [9, 0], [8, 0], [0, 1], [1, 2]], dtype=np.int8)  # Q、J、10同花色

    # 测试最弱手牌（2、3、4、5、7不同花色）
    weakest_hand = np.array([[0, 0], [1, 1]], dtype=np.int8)  # 2、3不同花色
    weakest_board = np.array([[2, 2], [3, 3], [5, 0], [12, 1], [11, 2]], dtype=np.int8)  # 4、5、7不同花色
    # 调用单手牌评估函数
    rank = handeval.get_hand_rank_holdem(hand, board)
    rank1 = handeval.get_hand_rank_holdem(strongest_hand, strongest_board)
    rank2 = handeval.get_hand_rank_holdem(weakest_hand, weakest_board)

    print(f"手牌 {hand} 在 {board} 上的排名是: {rank}")
    print(f"手牌 {strongest_hand} 在 {strongest_board} 上的排名是: {rank1}")
    print(f"手牌 {weakest_hand} 在 {weakest_board} 上的排名是: {rank2}")
