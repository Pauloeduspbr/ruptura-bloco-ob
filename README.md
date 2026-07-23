# Ruptura & Bloco

Porte C++/MQL5 de um indicador técnico de price action / estrutura de mercado (market structure), para uso no MetaTrader 5.

![Ruptura & Bloco](screenshot.png)

## O que é

Este projeto é um porte do indicador Pine Script "Market Structure Break & Order Block", de autoria de EmreKb, publicado originalmente na TradingView. A lógica foi reimplementada do zero em C++ (motor de cálculo, compilado como DLL) e em MQL5 (indicador nativo para o MetaTrader 5), sem depender do runtime Pine.

O cálculo parte de um ZigZag com comprimento configurável (`zigzag_len`) para identificar topos e fundos alternados do preço. A partir dessas pernas, o indicador detecta uma Market Structure Break (MSB):

- **MSB bearish**: o preço rompe um topo anterior e, em seguida, forma fundos mais baixos.
- **MSB bullish**: o preço rompe um fundo anterior e, em seguida, forma topos mais altos.

A confirmação do rompimento usa um fator de Fibonacci (`fib_factor`): a nova máxima (ou mínima) só é considerada breakout válido se ultrapassar o nível `1 + fib_factor` (ou o equivalente do lado da mínima) calculado sobre a perna anterior do ZigZag.

Após identificar a MSB, o indicador plota o Order Block (OB) correspondente como uma box no gráfico:

- Em MSB bearish, o OB é o último candle de alta antes da máxima que originou o rompimento.
- Em MSB bullish, o OB é o último candle de baixa antes da mínima que originou o rompimento.

Boxes antigas ou já rompidas podem ser removidas automaticamente (`delete_boxes`), há inputs separados de cor/borda/texto para OB de alta e de baixa, e a exibição das linhas do ZigZag pode ser ligada ou desligada.

## Instalação — versão pré-compilada

1. Copie `msb_orderblock.dll` para a pasta `MQL5/Libraries` do seu terminal MetaTrader 5.
2. Copie `TV_13_MSB_OrderBlock.ex5` para a pasta `MQL5/Indicators` do mesmo terminal.
3. Reinicie o MetaTrader 5 (ou clique com o botão direito em "Indicadores" no Navegador e escolha "Atualizar").
4. Arraste o indicador `TV_13_MSB_OrderBlock` do Navegador para o gráfico desejado.

## Build a partir do código-fonte

1. Compile o motor C++ (`msb_orderblock`) com g++/MinGW-w64, usando o `build.sh` incluso em `src/cpp/` — isso gera o `.dll`.
2. Abra `src/mql5/TV_13_MSB_OrderBlock.mq5` no MetaEditor do MetaTrader 5.
3. Compile com F7 para gerar o `.ex5`.

## Licença

Este repositório é licenciado sob MPL-2.0. A lógica original em Pine Script é de autoria de EmreKb.

## Aviso

Uso educacional e de análise técnica. Não constitui recomendação de investimento.
