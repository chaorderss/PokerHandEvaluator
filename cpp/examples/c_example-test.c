#include <assert.h>
#include <phevaluator/phevaluator.h>
#include <phevaluator/rank.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * This C code is a demonstration of how to calculate the card id, which will
 * be used as the parameter in the evaluator. It also shows how to use the
 * return value to determine which hand is the stronger one.
 */
int main() {
  /*
   * In this example we use a scenario in the game Texas Holdem:
   * Community cards: 9c 4c 4s 9d 4h (both players share these cards)
   * Player 1: Qc 6c
   * Player 2: 2c 9h
   *
   * Both players have full houses, but player 1 has only a four full house
   * while player 2 has a nine full house.
   *
   * The result is player 2 has a stronger hand than player 1.
   */

  /*
   * To calculate the value of each card, we can either use the Card Id
   * mapping table, or use the formula rank * 4 + suit to get the value
   *
   * More specifically, the ranks are:
   *
   * deuce = 0, trey = 1, four = 2, five = 3, six = 4, seven = 5, eight = 6,
   * nine = 7, ten = 8, jack = 9, queen = 10, king = 11, ace = 12.
   *
   * And the suits are:
   * club = 0, diamond = 1, heart = 2, spade = 3
   */
  // Community cards
  int a = 12 * 4 + 2; // Ah (A=12, h=2)
  int b = 11 * 4 + 0; // Kd (K=11, d=1)
  int c = 10 * 4 + 0; // Qc (Q=10, c=0)
  int d = 9 * 4 + 0;  // Js (J=9, s=3)
  int e = 7 * 4 + 2;  // 9h (9=7, h=2)

  // Player 1
  int f = 12 * 4 + 0;  // 2c (2=0, c=0)
  int g = 8 * 4 + 0;  // 3d (3=1, d=1)

  // Player 2
  int h = 0 * 4 + 3; // As (A=12, s=3)
  int i = 1 * 4 + 0; // Ac (A=12, c=0)

  // Evaluating the hand of player 1
  int rank1 = evaluate_7cards(a, b, c, d, e, f, g);
  // Evaluating the hand of player 2
  int rank2 = evaluate_7cards(a, b, c, d, e, h, i);

  printf("rank1: %d\n", rank1);
  printf("rank2: %d\n", rank2);



  return 0;
}
