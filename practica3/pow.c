/**
 * @file pow.h
 * @author SOPER teaching team.
 * @brief Computation of the POW.
 * @version 2.0
 * @date 2024-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "pow.h"

#define PRIME POW_LIMIT //El numero de modulo
#define BIG_X 435679812 //El numero para obtener un resultado hash
#define BIG_Y 100001819 //El numero para obtener un resultado hash

/**
 * @brief Computes the following hash function:
 * f(x) = (X x + Y) % P.
 *
 * @param x Argument of the hash function, x.
 * @return Result of the hash function, f(x).
 */
long int pow_hash(long int x) {
  long int result = (x * BIG_X + BIG_Y) % PRIME;
  return result;
}
