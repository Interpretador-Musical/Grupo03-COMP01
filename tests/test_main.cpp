// Único ponto do projeto que gera o main() do doctest. Mantê-lo sozinho num
// arquivo evita recompilar a implementação inteira do framework toda vez que
// um caso de teste muda.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
