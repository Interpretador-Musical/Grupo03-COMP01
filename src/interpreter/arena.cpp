/**
 * @file arena.cpp
 * @brief Blocos, registro de destrutores e reinício da arena da AST (MAT-10, #55).
 */

#include "interpreter/arena.h"

#include <algorithm>
#include <cstring>
#include <functional>

namespace mus {
namespace ast {

Arena::Arena(std::size_t primeiroBloco)
    : primeiroBloco_(std::max<std::size_t>(primeiroBloco, 1)),
      proximoBloco_(primeiroBloco_) {
    abrirBloco(primeiroBloco_);
}

Arena::~Arena() {
    destruirObjetos();
    for (const Bloco& bloco : blocos_) {
        devolver(bloco);
    }
}

std::string_view Arena::copiarTexto(std::string_view texto) {
    if (texto.empty()) {
        return {};
    }
    char* copia = static_cast<char*>(alocar(texto.size(), alignof(char)));
    std::memcpy(copia, texto.data(), texto.size());
    return {copia, texto.size()};
}

void Arena::reiniciar() {
    destruirObjetos();
    for (std::size_t i = 1; i < blocos_.size(); ++i) {
        devolver(blocos_[i]);
    }
    blocos_.erase(blocos_.begin() + 1, blocos_.end());

    // "Resetar o ponteiro do buffer", como pede a issue: o cursor volta ao
    // começo do primeiro bloco, e o que estava lá passa a ser memória livre.
    const Bloco& primeiro = blocos_.front();
    cursor_ = primeiro.inicio;
    fim_ = primeiro.inicio + primeiro.tamanho;
    // O mesmo estado de logo depois do construtor: o próximo bloco é o dobro
    // do primeiro.
    proximoBloco_ = std::min(primeiroBloco_ * 2, std::max(kMaiorBloco, primeiroBloco_));
    bytesUsados_ = 0;
    MUS_ARENA_ENVENENA(primeiro.inicio, primeiro.tamanho);
}

std::size_t Arena::bytesReservados() const {
    std::size_t total = 0;
    for (const Bloco& bloco : blocos_) {
        total += bloco.tamanho;
    }
    return total;
}

bool Arena::contem(const void* endereco) const {
    // `std::less` dá uma ordem total entre ponteiros de alocações diferentes;
    // o `<` embutido não garante isso.
    const std::less<const void*> antes;
    for (const Bloco& bloco : blocos_) {
        if (!antes(endereco, bloco.inicio) && antes(endereco, bloco.inicio + bloco.tamanho)) {
            return true;
        }
    }
    return false;
}

void* Arena::alocarEmBlocoNovo(std::size_t tamanho, std::size_t alinhamento) {
    // `alinhamento - 1` a mais cobre o pior caso de ajuste no começo do bloco,
    // embora o `operator new` já entregue memória com alinhamento fundamental.
    abrirBloco(std::max(proximoBloco_, tamanho + alinhamento - 1));

    void* inicio = cursor_;
    std::size_t espaco = static_cast<std::size_t>(fim_ - cursor_);
    return std::align(alinhamento, tamanho, inicio, espaco);
}

void Arena::abrirBloco(std::size_t tamanho) {
    auto* inicio = static_cast<std::byte*>(::operator new(tamanho));
    try {
        blocos_.push_back(Bloco{inicio, tamanho});
    } catch (...) {
        ::operator delete(inicio);
        throw;
    }
    MUS_ARENA_ENVENENA(inicio, tamanho);
    cursor_ = inicio;
    fim_ = inicio + tamanho;
    // Só um bloco do tamanho normal faz o próximo dobrar: um pedido gigante
    // não deve inflar todos os blocos seguintes.
    if (tamanho == proximoBloco_) {
        proximoBloco_ = std::min(proximoBloco_ * 2, std::max(kMaiorBloco, primeiroBloco_));
    }
}

void Arena::reservarDestrutor() {
    // Crescimento geométrico feito à mão: `reserve(size() + 1)` a cada objeto
    // realocaria o registro inteiro toda vez.
    if (destrutores_.size() == destrutores_.capacity()) {
        destrutores_.reserve(std::max<std::size_t>(16, destrutores_.capacity() * 2));
    }
}

void Arena::destruirObjetos() {
    for (auto it = destrutores_.rbegin(); it != destrutores_.rend(); ++it) {
        it->destruir(it->objeto);
    }
    destrutores_.clear();
}

void Arena::devolver(const Bloco& bloco) {
    // O bloco volta ao sistema sem marcas: um trecho ainda envenenado
    // confundiria o próximo dono dessa memória sob o ASan.
    MUS_ARENA_DESENVENENA(bloco.inicio, bloco.tamanho);
    ::operator delete(bloco.inicio);
}

}  // namespace ast
}  // namespace mus
