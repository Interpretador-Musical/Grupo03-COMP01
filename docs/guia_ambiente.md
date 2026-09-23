---
title: Guia de Ambiente (Windows)
layout: default
parent: Documentação
nav_order: 5
---

# Guia de Configuração de Ambiente para Windows

O Interpretador Musical (Grupo 03) depende de ferramentas clássicas de compilação C/C++. Para utilizadores de Windows, a abordagem recomendada e homologada para o projeto é a utilização do **Windows Subsystem for Linux (WSL)**.

Abaixo encontra-se o passo a passo para configurar o ambiente de desenvolvimento, instalar as dependências e clonar o repositório.

---

## 1. Instalação e Ativação do WSL

1. Abra o **PowerShell** como Administrador no Windows.
2. Execute o comando de instalação automática:
   ```bash
   wsl --install
   ```
3. Reinicie o computador caso o instalador solicite.
4. Após reiniciar, abra o menu Iniciar e procure por "Ubuntu" (ou "WSL") para iniciar o terminal Linux.

## 2. Instalação das Dependências (Flex, Bison e C++)

Com o terminal do Linux (Ubuntu) aberto, é necessário instalar as ferramentas de compilação (build-essential), os analisadores léxico e sintático (Flex e Bison), e o controlo de versões (Git).

1. Atualize a lista de pacotes do sistema:
   ```bash
   sudo apt update
   ```
2. Instale as ferramentas necessárias:
   ```bash
   sudo apt install flex bison build-essential git -y
   ```
3. Verifique se a instalação foi bem-sucedida confirmando as versões:

   ```bash
   flex --version
   bison --version
   git --version
   ```

## 3. Configuração do Git e Clonagem do Repositório

1. Antes de contribuir com o código, configure a sua identidade no Git para que os seus commits fiquem devidamente registrados:
   ```bash
   git config --global user.name "Seu Nome"
   git config --global user.email "seu.email@exemplo.com"
   ```
2. Por fim, clone o repositório da equipe para a sua máquina:
   ```bash
   git clone [https://github.com/Interpretador-Musical/Grupo03-COMP01.git](https://github.com/Interpretador-Musical/Grupo03-COMP01.git)
   ```







