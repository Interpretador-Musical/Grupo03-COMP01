---
title: Documentação
layout: default
nav_order: 2
has_children: true
---

# Documentação

Tudo que o grupo produz de documentação do projeto vive aqui. Por enquanto são as atas
de reunião; conforme a linguagem for sendo definida, as demais seções entram nesta mesma
lista.

## Atas de reunião

Registro cronológico das decisões de projeto — o que foi decidido, quando, e por quê.

| Data | Assunto principal |
|---|---|
| [19/08/2026]({{ site.baseurl }}{% link atas/2026-08-19-ata-reuniao.md %}) | Concepção do projeto, aprovação do professor, escolha da stack e definição do pipeline |
| [26/08/2026]({{ site.baseurl }}{% link atas/2026-08-26-ata-reuniao.md %}) | Mudança para Flex + Bison, fluxo entre repositórios e panorama de pendências |

## Em breve

Documentos previstos, ainda não escritos:

- **Gramática EBNF** — a formalização completa da linguagem, base tanto do `parser.y`
  quanto da entrega documental.
- **Guia da linguagem** — referência de sintaxe, com todas as construções e exemplos.
- **Arquitetura** — detalhamento do pipeline, das classes da AST e do contrato entre
  interpretador e motor de áudio.
- **Exemplos `.mus`** — programas completos, que servem também como testes de aceitação.

Cada um desses vira uma página nesta seção quando a decisão correspondente for tomada.
Acompanhe o [roadmap]({{ site.baseurl }}{% link roadmap.md %}) para saber o que está
travando o quê.
