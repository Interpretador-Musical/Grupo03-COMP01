#include "interpreter/operacoes.h"

#include <string>

#include "core/pitch.h"

namespace mus {

double paraNumero(const Valor& valor) {
    if (std::holds_alternative<double>(valor)) {
        return std::get<double>(valor);
    }
    if (std::holds_alternative<bool>(valor)) {
        return std::get<bool>(valor) ? 1.0 : 0.0;
    }
    return 0.0;
}

bool paraBooleano(const Valor& valor) {
    if (std::holds_alternative<bool>(valor)) {
        return std::get<bool>(valor);
    }
    if (std::holds_alternative<double>(valor)) {
        return std::get<double>(valor) != 0.0;
    }
    return false;
}

double midiDaNota(int semitom, int oitava) {
    return static_cast<double>((oitava + 1) * kSemitonesPerOctave + semitom);
}

Valor aplicarUnario(ast::OperadorUnario operador, const Valor& operando) {
    if (operador == ast::OperadorUnario::Negacao) {
        return -paraNumero(operando);
    }
    return !paraBooleano(operando);
}

std::optional<Valor> aplicarBinario(ast::OperadorBinario operador,
                                    const Valor& esquerda, const Valor& direita) {
    // Comparacoes de igualdade entre strings.
    if (std::holds_alternative<std::string>(esquerda) &&
        std::holds_alternative<std::string>(direita)) {
        const std::string& a = std::get<std::string>(esquerda);
        const std::string& b = std::get<std::string>(direita);

        if (operador == ast::OperadorBinario::Igual) {
            return Valor{a == b};
        }
        if (operador == ast::OperadorBinario::Diferente) {
            return Valor{a != b};
        }
        return std::nullopt;
    }

    switch (operador) {
        case ast::OperadorBinario::Soma:
            return Valor{paraNumero(esquerda) + paraNumero(direita)};
        case ast::OperadorBinario::Subtracao:
            return Valor{paraNumero(esquerda) - paraNumero(direita)};
        case ast::OperadorBinario::Multiplicacao:
            return Valor{paraNumero(esquerda) * paraNumero(direita)};
        case ast::OperadorBinario::Divisao:
            return Valor{paraNumero(esquerda) / paraNumero(direita)};
        case ast::OperadorBinario::Igual:
            return Valor{paraNumero(esquerda) == paraNumero(direita)};
        case ast::OperadorBinario::Diferente:
            return Valor{paraNumero(esquerda) != paraNumero(direita)};
        case ast::OperadorBinario::Menor:
            return Valor{paraNumero(esquerda) < paraNumero(direita)};
        case ast::OperadorBinario::Maior:
            return Valor{paraNumero(esquerda) > paraNumero(direita)};
        case ast::OperadorBinario::MenorOuIgual:
            return Valor{paraNumero(esquerda) <= paraNumero(direita)};
        case ast::OperadorBinario::MaiorOuIgual:
            return Valor{paraNumero(esquerda) >= paraNumero(direita)};
        case ast::OperadorBinario::ELogico:
            return Valor{paraBooleano(esquerda) && paraBooleano(direita)};
        case ast::OperadorBinario::OuLogico:
            return Valor{paraBooleano(esquerda) || paraBooleano(direita)};
    }
    return Valor{0.0};  // inatingível: switch cobre todo OperadorBinario
}

}  // namespace mus
