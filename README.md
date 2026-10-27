# VZX CHEAT BS

Projeto C++17 para Blood Strike no Windows x64, com interface Dear ImGui e renderização DirectX 11.

## Vídeo de demonstração

[![Assistir à demonstração do VZX CHEAT BS](https://img.youtube.com/vi/FMSWW6XWOlM/hqdefault.jpg)](https://youtu.be/FMSWW6XWOlM)

[Assistir no YouTube](https://youtu.be/FMSWW6XWOlM)

## Recursos

- ESP com caixas completas ou de canto, linhas, esqueleto, nome, distância e vida quando os dados estiverem disponíveis.
- Barra de vida com posição selecionável e transição de verde para amarelo e vermelho.
- Destaque vermelho para jogadores caídos.
- Filtros de aliados e mortos, com tratamento específico para o campo de treino.
- Aim Target: cabeça, pescoço, peito ou pelve, usando os ossos identificados no modelo.
- Compensação de recuo vertical e horizontal ajustável.
- Reconexão e redescoberta de câmera e entidades após reiniciar o processo do jogo.
- Offsets centralizados e solução do Visual Studio incluída.

## Requisitos

- Windows x64.
- Visual Studio 2022 ou Build Tools com **Desenvolvimento para desktop com C++**, toolset **v143** e Windows SDK.
- DirectX 11.

Os fontes necessários do Dear ImGui 1.91.9b estão incluídos em `vendor/imgui`.

## Compilar no Visual Studio

1. Abra `VZX-CHEAT-BS.sln`.
2. Selecione **Debug** ou **Release**, plataforma **x64**.
3. Compile com **Ctrl+Shift+B**.

O executável e `layout.ini` serão gerados em:

```text
build/vs/x64/Debug/
build/vs/x64/Release/
```

Também é possível executar `build.bat`, que gera os arquivos em `build/bin`.

## Controles

| Controle | Ação |
| --- | --- |
| INSERT | Abrir ou fechar o menu |
| END | Encerrar o programa |
| Botão direito | Acionar a mira, quando habilitada e com o menu fechado |
| Mirar + atirar | Acionar a compensação de recuo, quando habilitada |

Os ajustes ficam nas abas **Visual**, **Mira** e **Ajustes**. A compensação de recuo começa desligada e precisa ser ajustada conforme a arma e a sensibilidade. Os movimentos só são enviados com o jogo em primeiro plano, o menu fechado e uma leitura válida.

## Estrutura

```text
src/
  core/       Tipos, opções e filtros
  game/       Leitura do jogo, perfis e offsets.h
  input/      Seleção da região de mira e compensação de recuo
  platform/   Eventos da janela Windows
  render/     DirectX e composição alfa
  ui/         Menu, tema e fontes
  visuals/    Projeção e desenho do ESP
config/       Configuração de leitura
tests/        Testes automatizados
vendor/imgui/ Dependência e backends utilizados
```

## Offsets e compatibilidade

Os offsets do engine e do runtime Python ficam em [`src/game/offsets.h`](src/game/offsets.h). Alterações nesse arquivo exigem recompilação. A configuração de leitura fica em [`config/layout.ini`](config/layout.ini). O processo alvo continua sendo `BloodStrike.exe`.

O leitor valida a assinatura do runtime e os ponteiros das vtables antes de usar o layout. Ao detectar uma nova sessão, descarta as referências antigas e redescobre câmera e entidades. A compatibilidade foi verificada na compilação do jogo usada durante o desenvolvimento; atualizações e outros mapas podem exigir ajustes.

No treino, o filtro de equipe fica inativo apenas quando o modo é identificado. Nome e vida dos alvos de treino podem estar indisponíveis. Regiões de mira sem osso identificado não são selecionadas. A altura das caixas e a recuperação da câmera ainda possuem componentes estimados ou heurísticos.

## Validação

Execute:

```bat
test.bat
```

Há **35 casos automatizados** para projeção, filtros de vida/equipe, seleção de região e cálculo de recuo. Eles passaram na validação local. As configurações **Debug x64** e **Release x64** também compilaram com sucesso.

A opção `--render-test` do executável verifica a composição alfa e os glifos sem iniciar a leitura do jogo. A reconexão foi conferida com um processo novo e seis alvos no campo de treino. A eficácia da mira e da compensação de recuo em disparos reais não foi validada de forma abrangente.

## Dependências e atribuição

Dear ImGui 1.91.9b é distribuído sob a licença MIT. A integração da janela e do DirectX foi adaptada dos exemplos da biblioteca. A licença e os avisos dos fontes de terceiros foram preservados em [`vendor/imgui/LICENSE.txt`](vendor/imgui/LICENSE.txt).
