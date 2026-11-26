/**
 * @file ring_buffer.h
 * @brief Fila circular lock-free Single-Producer/Single-Consumer (SCH-03, #17).
 */

#ifndef MUS_CORE_RING_BUFFER_H
#define MUS_CORE_RING_BUFFER_H

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace mus {

/**
 * @brief Buffer circular SPSC sem travas nem alocações em tempo real.
 *
 * Faz a ponte entre a thread produtora (principal) e a thread consumidora
 * (callback de áudio da miniaudio ou gerador offline). Toda a memória é
 * alocada uma única vez na construção.
 *
 * @tparam T Tipo dos itens armazenados (exige `std::is_trivially_copyable`).
 * @warning Seguro apenas com um único produtor chamando `push()` e um único
 *          consumidor chamando `pop()`.
 */
template <typename T>
class SpscRingBuffer {
public:
    // Sem isto, std::atomic poderia ser implementado com um mutex escondido
    // na plataforma, e o consumidor travaria a thread de áudio sem que nada no
    // código mostrasse isso (NFR da SCH-05, #29). Em x86-64 e ARM64 é sempre
    // verdade; a garantia fica aqui para não depender de confiança.
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "SpscRingBuffer exige índices atômicos sem lock: a thread de "
                  "áudio não pode esperar por mutex nenhum");

    static_assert(std::is_trivially_copyable<T>::value,
                  "SpscRingBuffer só aceita tipos trivialmente copiáveis: um "
                  "construtor de cópia não-trivial poderia alocar memória "
                  "escondida dentro da thread de áudio, violando o NFR da "
                  "SCH-03 (#17). É a mesma garantia que SoundEvent já promete "
                  "em core/sound_event.h.");

    /**
     * @brief Constrói o ring buffer pré-alocando `capacity + 1` posições.
     *
     * O slot extra distingue buffer cheio de vazio apenas com `writeIndex_` e
     * `readIndex_`, sem contador atômico adicional.
     *
     * @param capacity Número máximo de itens armazenados simultaneamente.
     */
    explicit SpscRingBuffer(std::size_t capacity) : buffer_(capacity + 1) {}

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    /**
     * @brief Insere um item na fila sem bloquear nem alocar (apenas produtor).
     * @param item Item a enfileirar.
     * @return `true` se inserido; `false` se a fila estiver cheia (sem modificar estado).
     */
    bool push(const T& item) {
        const std::size_t writeIndex = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t nextWriteIndex = advance(writeIndex);

        // acquire: sincroniza com o release de pop(), garantindo que o slot
        // prestes a ser sobrescrito já não está sendo lido pelo consumidor.
        if (nextWriteIndex == readIndex_.load(std::memory_order_acquire)) {
            return false;  // cheio
        }

        buffer_[writeIndex] = item;

        // release: publica o item para o consumidor — só depois deste store
        // é seguro pop() enxergar esta posição como disponível.
        writeIndex_.store(nextWriteIndex, std::memory_order_release);
        return true;
    }

    /**
     * @brief Remove o próximo item da fila sem bloquear nem alocar (apenas consumidor).
     * @param[out] item Ponteiro que recebe o item lido (inalterado se vazia).
     * @return `true` se um item foi removido; `false` se a fila estiver vazia.
     */
    bool pop(T* item) {
        const std::size_t readIndex = readIndex_.load(std::memory_order_relaxed);

        // acquire: sincroniza com o release de push(), garantindo que o item
        // já está totalmente escrito antes de lermos.
        if (readIndex == writeIndex_.load(std::memory_order_acquire)) {
            return false;  // vazio
        }

        *item = buffer_[readIndex];

        // release: publica a posição de volta para o produtor reutilizar.
        readIndex_.store(advance(readIndex), std::memory_order_release);
        return true;
    }

    /**
     * @brief Retorna a capacidade útil da fila em número de itens.
     * @return Capacidade configurada (exclui o slot sentinela interno).
     */
    std::size_t capacity() const { return buffer_.size() - 1; }

private:
    std::size_t advance(std::size_t index) const {
        return (index + 1) % buffer_.size();
    }

    std::vector<T> buffer_;
    std::atomic<std::size_t> writeIndex_{0};
    std::atomic<std::size_t> readIndex_{0};
};

}  // namespace mus

#endif  // MUS_CORE_RING_BUFFER_H
