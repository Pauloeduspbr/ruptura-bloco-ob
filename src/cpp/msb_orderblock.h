//+------------------------------------------------------------------+
//| msb_orderblock.h                                                 |
//| Engine C++ do "Market Structure Break & Order Block"             |
//| (c) EmreKb — MPL-2.0, Pine v5                                    |
//|                                                                  |
//| Porte de:                                                        |
//|   indicadores_tradingview/13_Market_Structure_Break_Order_Block.../|
//|   source.pine                                                    |
//|                                                                  |
//| CONTRATO DLL <-> MQL5 (x64): mesmo padrao dos demais portes.      |
//|                                                                  |
//| POR QUE RECALCULA TUDO A CADA CHAMADA                             |
//| O indicador mantem arrays de pivos que crescem ao longo do        |
//| historico e usa ta.valuewhen/ta.barssince sobre eles. Um calculo  |
//| incremental teria de versionar esses arrays por barra. Recalcular |
//| e O(N*zigzagLen) e fica naturalmente stateless — mesma decisao    |
//| tomada no #10.                                                    |
//|                                                                  |
//| O QUE O INDICADOR FAZ                                             |
//| 1. Zigzag simples: to_up quando high toca a maxima de N barras;   |
//|    to_down quando low toca a minima. `trend` alterna entre 1 e -1.|
//| 2. A cada virada de trend, empilha o pivo (valor + barra).        |
//| 3. `market` (a ESTRUTURA) vira quando o novo pivo rompe o         |
//|    anterior com folga de fib_factor — e o "Market Structure       |
//|    Break". Note que o autor deixou a versao antiga comentada na   |
//|    linha 106 e usa a da linha 109, que compara l0<l1 / h0>h1.     |
//| 4. Na virada, procura a ultima vela CONTRARIA dentro de um        |
//|    intervalo de barras: essa vela e o Order Block (OB) ou o       |
//|    Breaker Block (BB/MB).                                         |
//|                                                                   |
//| SAIDAS: alem de trend/market, devolve as 4 barras de OB/BB da     |
//| virada, para o MQL5 desenhar as caixas.                           |
//+------------------------------------------------------------------+
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MSB_ABI_VERSION 1

__declspec(dllexport) int __stdcall MsbVersion(void);

//+------------------------------------------------------------------+
//| MsbCalculate — recalcula [0, rates_total-1]                       |
//|                                                                   |
//| ENTRADA: open, high, low, close (indice 0 = barra mais antiga)    |
//|                                                                   |
//| SAIDA (um valor por barra)                                        |
//|   trend      +1 / -1 (zigzag)                                     |
//|   market     +1 / -1 (estrutura). Muda => houve MSB               |
//|   msbEvent   0 = nada; +1 = MSB de alta; -1 = MSB de baixa        |
//|   h0,h1,l0,l1          pivos correntes (preco)                    |
//|   h0i,h1i,l0i,l1i      barras dos pivos                           |
//|   obBar     barra do Order Block da virada    (-1 se nenhum)      |
//|   bbBar     barra do Breaker/Mitigation Block (-1 se nenhum)      |
//|   bbIsBreaker  1 = rotular "BB"; 0 = rotular "MB"                 |
//|                                                                   |
//| PARAMETROS: zigzagLen, fibFactor (0..1)                           |
//|                                                                   |
//| Retorno: rates_total em sucesso; -1 em argumento invalido.        |
//+------------------------------------------------------------------+
__declspec(dllexport) int __stdcall MsbCalculate(
   const double *open, const double *high, const double *low, const double *close,
   double *trend, double *market, double *msbEvent,
   double *h0, double *h1, double *l0, double *l1,
   double *h0i, double *h1i, double *l0i, double *l1i,
   double *obBar, double *bbBar, double *bbIsBreaker,
   int rates_total, int zigzagLen, double fibFactor);

#ifdef __cplusplus
}
#endif
