#include "doctest.h"

#include <variant>
#include <vector>

#include "interpreter/ambiente.h"

using mus::Ambiente;
using mus::Closure;
using mus::EscopoGuard;
using mus::Valor;

namespace {

double numero(const Ambiente& ambiente, const char* nome) {
    const Valor* v = ambiente.buscar(nome);
    REQUIRE(v != nullptr);
    return std::get<double>(*v);
}

Valor leX(Ambiente& ambiente, const std::vector<Valor>&) {
    const Valor* x = ambiente.buscar("x");
    return x != nullptr ? *x : Valor{-1.0};
}

}  // namespace

TEST_CASE("o ambiente começa só com o escopo global e não o fecha") {
    Ambiente ambiente;
    CHECK(ambiente.nivelAtual() == 0);
    ambiente.fecharEscopo();
    CHECK(ambiente.nivelAtual() == 0);
}

TEST_CASE("variável local some ao fechar o escopo") {
    Ambiente ambiente;
    ambiente.abrirEscopo();
    ambiente.definirLocal("a", 1.0);
    CHECK(ambiente.nivelAtual() == 1);
    CHECK(ambiente.buscar("a") != nullptr);
    ambiente.fecharEscopo();
    CHECK(ambiente.buscar("a") == nullptr);
}

TEST_CASE("definirLocal sombreia o externo e fechar restaura") {
    Ambiente ambiente;
    ambiente.atribuir("x", 1.0);
    ambiente.abrirEscopo();
    ambiente.definirLocal("x", 2.0);
    CHECK(numero(ambiente, "x") == doctest::Approx(2.0));
    ambiente.fecharEscopo();
    CHECK(numero(ambiente, "x") == doctest::Approx(1.0));
}

TEST_CASE("atribuir atualiza o escopo mais próximo e, se não existe, declara no topo") {
    Ambiente ambiente;
    ambiente.atribuir("x", 1.0);
    ambiente.abrirEscopo();
    ambiente.atribuir("x", 5.0);   // existe fora: atualiza lá
    ambiente.atribuir("y", 7.0);   // não existe: declara neste escopo
    ambiente.fecharEscopo();
    CHECK(numero(ambiente, "x") == doctest::Approx(5.0));
    CHECK(ambiente.buscar("y") == nullptr);
}

TEST_CASE("capturarEstado respeita o sombreamento: o escopo interno vence") {
    Ambiente ambiente;
    ambiente.atribuir("x", 1.0);
    ambiente.atribuir("g", 9.0);
    ambiente.abrirEscopo();
    ambiente.definirLocal("x", 2.0);
    const auto captura = ambiente.capturarEstado();
    CHECK(std::get<double>(captura.at("x")) == doctest::Approx(2.0));
    CHECK(std::get<double>(captura.at("g")) == doctest::Approx(9.0));
}

TEST_CASE("a closure captura por valor: mudar depois não altera o capturado") {
    Ambiente ambiente;
    ambiente.atribuir("x", 1.0);
    const Closure c = Closure::capturarBloco(ambiente, {}, nullptr);
    ambiente.atribuir("x", 9.0);
    CHECK(std::get<double>(c.estadoCapturado.at("x")) == doctest::Approx(1.0));
}

TEST_CASE("operator() restaura o capturado, vincula parâmetros e fecha o escopo") {
    Ambiente ambiente;
    ambiente.atribuir("x", 1.0);
    const Closure sem = Closure::capturar(ambiente, {}, &leX);
    const Closure com = Closure::capturar(ambiente, {"x"}, &leX);
    ambiente.atribuir("x", 2.0);

    const int nivel = ambiente.nivelAtual();
    CHECK(std::get<double>(sem(ambiente)) == doctest::Approx(1.0));
    CHECK(std::get<double>(com(ambiente, {Valor{5.0}})) == doctest::Approx(5.0));
    CHECK(ambiente.nivelAtual() == nivel);
}

TEST_CASE("EscopoGuard fecha o escopo ao sair do bloco") {
    Ambiente ambiente;
    {
        EscopoGuard guarda(ambiente);
        CHECK(ambiente.nivelAtual() == 1);
    }
    CHECK(ambiente.nivelAtual() == 0);
}
