# Copyright (c) 2019 Eric Steinberger


import os
from os.path import join as ospj

import numpy as np

from PokerRL._.CppWrapper import CppWrapper
from PokerRL.game.Poker import Poker
from PokerRL.game._.rl_env.game_rules import HoldemRules


class CppLibHoldemLuts(CppWrapper):

    def __init__(self, n_boards_lut, n_cards_out_lut):
        super().__init__(path_to_dll=ospj(os.path.dirname(os.path.realpath(__file__)),
                                          "lib_luts." + self.CPP_LIB_FILE_ENDING))
        self._n_boards_lut = n_boards_lut
        self._n_cards_out_lut = n_cards_out_lut

        self._clib.get_hole_card_2_idx_lut.argtypes = [self.ARR_2D_ARG_TYPE]
        self._clib.get_hole_card_2_idx_lut.restype = None

        self._clib.get_idx_2_hole_card_lut.argtypes = [self.ARR_2D_ARG_TYPE]
        self._clib.get_idx_2_hole_card_lut.restype = None

        self._clib.get_idx_2_flop_lut.argtypes = [self.ARR_2D_ARG_TYPE]
        self._clib.get_idx_2_flop_lut.restype = None

        self._clib.get_idx_2_turn_lut.argtypes = [self.ARR_2D_ARG_TYPE]
        self._clib.get_idx_2_turn_lut.restype = None

        self._clib.get_idx_2_river_lut.argtypes = [self.ARR_2D_ARG_TYPE]
        self._clib.get_idx_2_river_lut.restype = None

    # __________________________________________________ LUTs __________________________________________________________
    def get_idx_2_hole_card_lut(self):
        lut = np.full(shape=(HoldemRules.RANGE_SIZE, 2), fill_value=-2, dtype=np.int8)
        self._clib.get_idx_2_hole_card_lut(self.np_2d_arr_to_c(lut))  # fills it
        return lut

    def get_hole_card_2_idx_lut(self):
        lut = np.full(shape=(HoldemRules.N_CARDS_IN_DECK, HoldemRules.N_CARDS_IN_DECK),
                      fill_value=-2, dtype=np.int16)
        self._clib.get_hole_card_2_idx_lut(self.np_2d_arr_to_c(lut))  # fills it
        return lut

    def get_idx_2_flop_lut(self):
        lut = np.full(shape=(
            self._n_boards_lut[Poker.FLOP],
            self._n_cards_out_lut[Poker.FLOP]),
            fill_value=-2, dtype=np.int8)
        self._clib.get_idx_2_flop_lut(self.np_2d_arr_to_c(lut))  # fills it
        return lut

    def get_idx_2_turn_lut(self):
        lut = np.full(shape=(
            self._n_boards_lut[Poker.TURN],
            self._n_cards_out_lut[Poker.TURN]),
            fill_value=-2, dtype=np.int8)
        self._clib.get_idx_2_turn_lut(self.np_2d_arr_to_c(lut))  # fills it
        return lut

    def get_idx_2_river_lut(self):
        lut = np.full(shape=(
            self._n_boards_lut[Poker.RIVER],
            self._n_cards_out_lut[Poker.RIVER]),
            fill_value=-2, dtype=np.int8)
        self._clib.get_idx_2_river_lut(self.np_2d_arr_to_c(lut))  # fills it
        return lut

    def get_1d_card(self, card_2d):
        """
        Args:
            card_2d (np.ndarray):    array of 2 int8s. [rank, suit]

        Returns:
            int8: 1d representation of card_2d

        """
        return self._clib.get_1d_card(self.np_1d_arr_to_c(card_2d))

    def get_2d_card(self, card_1d):
        """
        Args:
            card_1d (int):

        Returns:
            np.ndarray(shape=2, dtype=np.int8): 2d representation of card_1d
        """
        card_2d = np.empty(shape=2, dtype=np.int8)
        self._clib.get_2d_card(card_1d, self.np_1d_arr_to_c(card_2d))
        return card_2d

def convert_hole_cards_to_index(card1, card2, lut):
    """
    将两张底牌转换为唯一索引值

    参数:
        card1 (int): 第一张牌的ID (0-51)
        card2 (int): 第二张牌的ID (0-51)
        lut (numpy.ndarray): 通过get_hole_card_2_idx_lut()获取的查找表

    返回:
        int: 底牌组合的唯一索引，如果是无效组合则返回-2
    """
    # 确保较小的牌索引在前，保证查找正确的上三角区域
    if card1 > card2:
        card1, card2 = card2, card1

    # 直接从查找表获取索引
    return lut[card1, card2]
def index_to_hole_cards(index, lut):
    ace_spades = 0
    ace_hearts = 13
    king_spades = 12
    queen_clubs = 50

    # 获取不同底牌组合的索引
    pocket_aces_index = convert_hole_cards_to_index(ace_spades, ace_hearts, luts.get_hole_card_2_idx_lut())
    ak_suited_index = convert_hole_cards_to_index(ace_spades, king_spades, luts.get_hole_card_2_idx_lut())
    random_cards_index = convert_hole_cards_to_index(ace_hearts, queen_clubs, luts.get_hole_card_2_idx_lut())

    print("AA索引:", pocket_aces_index)
    print("A♠K♠索引:", ak_suited_index)
    print("A♥Q♣索引:", random_cards_index)

    # 演示索引的一致性 - 牌的顺序不影响索引
    index1 = convert_hole_cards_to_index(ace_spades, king_spades, luts.get_hole_card_2_idx_lut())
    index2 = convert_hole_cards_to_index(king_spades, ace_spades, luts.get_hole_card_2_idx_lut())
    print("顺序无关性测试:", index1 == index2, "索引值:", index1)

if __name__ == "__main__":
    from PokerRL.game._.look_up_table import _LutGetterHoldem
    from PokerRL.game.games import DiscretizedNLHoldem
    lut_getter = _LutGetterHoldem(env_cls=DiscretizedNLHoldem)
    luts = CppLibHoldemLuts(n_boards_lut=lut_getter.get_n_boards_LUT(), n_cards_out_lut=lut_getter.get_n_cards_out_at_LUT())
    # print(luts.get_idx_2_hole_card_lut())
    # print(luts.get_hole_card_2_idx_lut())
    print(luts.get_idx_2_flop_lut())
    # print(luts.get_idx_2_turn_lut())
    # print(luts.get_idx_2_river_lut())
    # print(luts.get_1d_card(np.array([0, 0])))
    # print(luts.get_2d_card(0))

    # 示例卡牌 (假设 0=A♠, 13=A♥, 26=A♦, 39=A♣)
