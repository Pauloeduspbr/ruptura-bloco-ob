//+------------------------------------------------------------------+
//|                                    TV_13_MSB_OrderBlock.mq5      |
//|   Porte MQL5 do "Market Structure Break & Order Block" (MSB-OB)  |
//|   Pine v5 original: (c) EmreKb — MPL-2.0                         |
//|                                                                  |
//| Toda a estrutura (zigzag, MSB, localizacao dos Order Blocks)     |
//| vive em msb_orderblock.dll (C++). O MQL5 desenha as caixas e as  |
//| gerencia — estender enquanto validas, remover quando rompidas —   |
//| reproduzindo o comportamento das linhas 163-201 do Pine.          |
//|                                                                  |
//| Requer "Permitir importacoes de DLL" HABILITADO.                 |
//+------------------------------------------------------------------+
#property copyright "Porte C++/MQL5 — original (c) EmreKb (MPL-2.0)"
#property link      "https://br.tradingview.com/script/"
#property version   "1.00"
#property description "MSB-OB — Market Structure Break & Order Block, engine C++"

#property indicator_chart_window
#property indicator_buffers 15
#property indicator_plots   1

//--- unico plot: o zigzag da estrutura
#property indicator_label1  "Zigzag"
#property indicator_type1   DRAW_SECTION
#property indicator_color1  clrDodgerBlue
#property indicator_width1  1

#define EXPECTED_ABI_VERSION 1
#define OBJ_PREFIX "TV13_MSB_"

//--- Carimbo de build (nome curto + log): distingue codigo novo de
//    .ex5 velho em memoria (o MT5 nao recarrega binario externo).
#define MSB_BUILD "0723a"

#import "msb_orderblock.dll"
int MsbVersion(void);
int MsbCalculate(const double &open[], const double &high[], const double &low[], const double &close[],
                 double &trend[], double &market[], double &msbEvent[],
                 double &h0[], double &h1[], double &l0[], double &l1[],
                 double &h0i[], double &h1i[], double &l0i[], double &l1i[],
                 double &obBar[], double &bbBar[], double &bbIsBreaker[],
                 int rates_total, int zigzagLen, double fibFactor);
#import

//--- ATENCAO: nada de `input group` (desloca os parametros de iCustom)
input int    InpZigzagLen  = 9;      // ZigZag Length
input bool   InpShowZigzag = true;   // Show Zigzag
input double InpFibFactor  = 0.33;   // Fib Factor para confirmar o rompimento
input bool   InpDeleteOld  = true;   // Delete Old/Broken Boxes
//--- No Pine as caixas sao color.new(cor, 70) — 70% transparentes.
//    O MT5 nao tem canal alfa em OBJ_RECTANGLE, mas sobre fundo ESCURO
//    o resultado de uma cor com alfa e identico a mesma cor com o
//    brilho reduzido na mesma proporcao. InpOpacity faz esse calculo:
//    30 reproduz o color.new(...,70) do original.
input bool   InpFillBoxes  = true;   // [MT5] Preencher as caixas
input int    InpOpacity    = 30;     // [MT5] Opacidade do preenchimento % (Pine usa 30)
input int    InpMaxBoxes   = 8;      // [MT5] Maximo de caixas mantidas
input int    InpExtendBars = 10;     // [MT5] Barras de extensao da caixa (Pine: +10)
input color  InpBuColor    = clrGreen;  // Cor Bu-OB / Bu-BB (alta)
input color  InpBeColor    = clrRed;    // Cor Be-OB / Be-BB (baixa)

//--- buffer plotado
double BufZig[];   // 0
//--- calculo
double BufTrend[]; // 1
double BufMkt[];   // 2
double BufEv[];    // 3
double BufH0[];    // 4
double BufH1[];    // 5
double BufL0[];    // 6
double BufL1[];    // 7
double BufH0i[];   // 8
double BufH1i[];   // 9
double BufL0i[];   // 10
double BufL1i[];   // 11
double BufOb[];    // 12
double BufBb[];    // 13
double BufBrk[];   // 14 — 1 = rotular BB (breaker); 0 = MB (mitigation)

int g_boxes = 0;

//+------------------------------------------------------------------+
int OnInit()
{
   const int v = MsbVersion();
   if(v != EXPECTED_ABI_VERSION)
   {
      PrintFormat("[TV_13_MSB] ABI incompativel: msb_orderblock.dll=%d, esperado=%d. "
                  "Recompile a DLL (cpp/13_msb_orderblock/build.sh).", v, EXPECTED_ABI_VERSION);
      return INIT_FAILED;
   }
   if(InpZigzagLen < 2)
   { Print("[TV_13_MSB] ZigZag Length deve ser >= 2."); return INIT_PARAMETERS_INCORRECT; }
   if(InpFibFactor < 0.0 || InpFibFactor > 1.0)
   { PrintFormat("[TV_13_MSB] Fib Factor fora de [0,1]: %.4f", InpFibFactor); return INIT_PARAMETERS_INCORRECT; }

   SetIndexBuffer(0,  BufZig,   INDICATOR_DATA);
   SetIndexBuffer(1,  BufTrend, INDICATOR_CALCULATIONS);
   SetIndexBuffer(2,  BufMkt,   INDICATOR_CALCULATIONS);
   SetIndexBuffer(3,  BufEv,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(4,  BufH0,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(5,  BufH1,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(6,  BufL0,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(7,  BufL1,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(8,  BufH0i,   INDICATOR_CALCULATIONS);
   SetIndexBuffer(9,  BufH1i,   INDICATOR_CALCULATIONS);
   SetIndexBuffer(10, BufL0i,   INDICATOR_CALCULATIONS);
   SetIndexBuffer(11, BufL1i,   INDICATOR_CALCULATIONS);
   SetIndexBuffer(12, BufOb,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(13, BufBb,    INDICATOR_CALCULATIONS);
   SetIndexBuffer(14, BufBrk,   INDICATOR_CALCULATIONS);

   ArraySetAsSeries(BufZig,false);   ArraySetAsSeries(BufTrend,false);
   ArraySetAsSeries(BufMkt,false);   ArraySetAsSeries(BufEv,false);
   ArraySetAsSeries(BufH0,false);    ArraySetAsSeries(BufH1,false);
   ArraySetAsSeries(BufL0,false);    ArraySetAsSeries(BufL1,false);
   ArraySetAsSeries(BufH0i,false);   ArraySetAsSeries(BufH1i,false);
   ArraySetAsSeries(BufL0i,false);   ArraySetAsSeries(BufL1i,false);
   ArraySetAsSeries(BufOb,false);    ArraySetAsSeries(BufBb,false);
   ArraySetAsSeries(BufBrk,false);

   PlotIndexSetDouble(0, PLOT_EMPTY_VALUE, EMPTY_VALUE);
   PlotIndexSetInteger(0, PLOT_DRAW_TYPE, InpShowZigzag ? DRAW_SECTION : DRAW_NONE);

   IndicatorSetString(INDICATOR_SHORTNAME,
                      StringFormat("MSB-OB b%s (%d, %.2f)", MSB_BUILD, InpZigzagLen, InpFibFactor));
   IndicatorSetInteger(INDICATOR_DIGITS, _Digits);

   PrintFormat("[TV_13_MSB] build %s | zigzagLen=%d showZigzag=%s fib=%.2f deleteOld=%s",
               MSB_BUILD, InpZigzagLen, InpShowZigzag?"on":"OFF",
               InpFibFactor, InpDeleteOld?"on":"OFF");
   return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   ObjectsDeleteAll(0, OBJ_PREFIX);
   ChartRedraw();
}

//+------------------------------------------------------------------+
//| Simula transparencia sobre fundo escuro.                         |
//| Uma cor com alfa A sobre preto resulta em cor*A. Como o MT5 nao   |
//| tem alfa em OBJ_RECTANGLE, aplicamos o fator direto no RGB.       |
//| MQL5 guarda color como 0x00BBGGRR — por isso a ordem abaixo.      |
//+------------------------------------------------------------------+
color Fade(const color base, const int pct)
{
   int p = pct;
   if(p < 0)   p = 0;
   if(p > 100) p = 100;

   const int r = (int)((base        & 0xFF) * p / 100);
   const int g = (int)(((base >> 8) & 0xFF) * p / 100);
   const int b = (int)(((base >> 16)& 0xFF) * p / 100);

   return (color)((b << 16) | (g << 8) | r);
}

//+------------------------------------------------------------------+
//| A caixa foi rompida em ALGUM momento desde que nasceu?           |
//| Pine (163-201): assim que o close atravessa a caixa, ela e        |
//| deletada — nao volta se o preco retornar depois.                  |
//+------------------------------------------------------------------+
bool BoxBroken(const double &close[], const int rates_total, const int born,
               const double top, const double bot, const bool bull)
{
   for(int j = born; j < rates_total; ++j)
   {
      if(bull) { if(close[j] < bot) return true; }
      else     { if(close[j] > top) return true; }
   }
   return false;
}

//+------------------------------------------------------------------+
//| Caixa de Order Block / Breaker Block                             |
//+------------------------------------------------------------------+
void MakeBox(const string name, const datetime t1, const double p1,
             const datetime t2, const double p2, const color bg,
             const color border, const string text)
{
   if(ObjectFind(0, name) < 0)
   {
      ObjectCreate(0, name, OBJ_RECTANGLE, 0, t1, p1, t2, p2);
      ObjectSetInteger(0, name, OBJPROP_FILL, InpFillBoxes);
      ObjectSetInteger(0, name, OBJPROP_BACK, true);
      ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false);
      ObjectSetInteger(0, name, OBJPROP_WIDTH, 1);
   }
   ObjectSetInteger(0, name, OBJPROP_COLOR, border);
   if(InpFillBoxes) ObjectSetInteger(0, name, OBJPROP_BGCOLOR, bg);
   ObjectMove(0, name, 0, t1, p1);
   ObjectMove(0, name, 1, t2, p2);

   const string lb = name + "_T";
   if(ObjectFind(0, lb) < 0)
   {
      ObjectCreate(0, lb, OBJ_TEXT, 0, t2, p1);
      ObjectSetInteger(0, lb, OBJPROP_FONTSIZE, 7);
      ObjectSetInteger(0, lb, OBJPROP_ANCHOR, ANCHOR_RIGHT);
      ObjectSetInteger(0, lb, OBJPROP_SELECTABLE, false);
   }
   ObjectSetInteger(0, lb, OBJPROP_COLOR, border);
   ObjectSetString(0, lb, OBJPROP_TEXT, text);
   ObjectMove(0, lb, 0, t2, p1);
}

//+------------------------------------------------------------------+
int OnCalculate(const int rates_total,
                const int prev_calculated,
                const datetime &time[],
                const double &open[],
                const double &high[],
                const double &low[],
                const double &close[],
                const long &tick_volume[],
                const long &volume[],
                const int &spread[])
{
   if(rates_total <= InpZigzagLen + 5)
      return 0;

   //--- o engine recalcula o historico inteiro (ver .h)
   const int rc = MsbCalculate(open, high, low, close,
                               BufTrend, BufMkt, BufEv,
                               BufH0, BufH1, BufL0, BufL1,
                               BufH0i, BufH1i, BufL0i, BufL1i,
                               BufOb, BufBb, BufBrk,
                               rates_total, InpZigzagLen, InpFibFactor);
   if(rc < 0)
   {
      PrintFormat("[TV_13_MSB] MsbCalculate falhou (rc=%d, rates_total=%d)", rc, rates_total);
      return 0;
   }

   //--- zigzag: marca so as barras de pivo, DRAW_SECTION liga os pontos.
   //    ARMADILHA 14: com "Show Zigzag" desligado o buffer fica VAZIO —
   //    o DRAW_NONE do OnInit nao basta, porque o MT5 persiste cor e
   //    estilo do plot no grafico e os restaura por cima; enquanto o
   //    buffer tiver dado, ele desenha. BufZig nao e estado de nada
   //    (nem e passado a DLL), entao o gate e seguro.
   for(int i = 0; i < rates_total; ++i) BufZig[i] = EMPTY_VALUE;
   if(InpShowZigzag)
   for(int i = 1; i < rates_total; ++i)
   {
      if(BufTrend[i] == BufTrend[i-1]) continue;
      //--- virou para alta => o pivo fechado e o fundo (l0); e vice-versa
      if(BufTrend[i] > 0)
      {
         const int b = (int)BufL0i[i];
         if(b >= 0 && b < rates_total) BufZig[b] = low[b];
      }
      else
      {
         const int b = (int)BufH0i[i];
         if(b >= 0 && b < rates_total) BufZig[b] = high[b];
      }
   }

   //--- caixas de Order Block nos eventos de MSB
   ObjectsDeleteAll(0, OBJ_PREFIX);
   g_boxes = 0;

   const datetime tEnd = time[rates_total-1] +
                         (datetime)PeriodSeconds(_Period)*InpExtendBars;

   for(int i = rates_total-1; i >= 0 && g_boxes < InpMaxBoxes; --i)
   {
      const int ev = (int)BufEv[i];
      if(ev == 0) continue;

      const int ob = (int)BufOb[i], bb = (int)BufBb[i];
      const bool bull = (ev > 0);
      //--- preenchimento esmaecido (simula o alfa do Pine); a BORDA fica
      //    na cor cheia, como no original
      const color bg  = Fade(bull ? InpBuColor : InpBeColor, InpOpacity);
      const color bd  = bull ? InpBuColor : InpBeColor;

      //--- Pine linhas 163-201: a caixa e DELETADA no instante em que o
      //    preco a rompe, e nao volta. Testar so a barra atual (como eu
      //    fazia) fazia caixas antigas reaparecerem quando o preco
      //    retornava — dai o acumulo na tela.
      if(ob >= 0 && ob < rates_total)
      {
         const double top = high[ob], bot = low[ob];
         const bool broken = BoxBroken(close, rates_total, i, top, bot, bull);
         if(!broken || !InpDeleteOld)
         {
            MakeBox(OBJ_PREFIX+"OB"+IntegerToString(i), time[ob], top, tEnd, bot,
                    bg, bd, bull ? "Bu-OB" : "Be-OB");
            ++g_boxes;
         }
      }

      if(bb >= 0 && bb < rates_total && g_boxes < InpMaxBoxes)
      {
         const double top = high[bb], bot = low[bb];
         const bool broken = BoxBroken(close, rates_total, i, top, bot, bull);
         if(!broken || !InpDeleteOld)
         {
            const bool isBreaker = (BufBrk[i] != 0.0);
            MakeBox(OBJ_PREFIX+"BB"+IntegerToString(i), time[bb], top, tEnd, bot,
                    bg, bd,
                    bull ? (isBreaker ? "Bu-BB" : "Bu-MB")
                         : (isBreaker ? "Be-BB" : "Be-MB"));
            ++g_boxes;
         }
      }

      //--- linha do MSB (Pine 149/156) + rotulo
      const int from = bull ? (int)BufH1i[i] : (int)BufL1i[i];
      const double lvl = bull ? BufH1[i] : BufL1[i];
      if(from >= 0 && from < rates_total && MathIsValidNumber(lvl))
      {
         const string ln = OBJ_PREFIX+"MSB"+IntegerToString(i);
         if(ObjectFind(0, ln) < 0)
         {
            ObjectCreate(0, ln, OBJ_TREND, 0, time[from], lvl, time[i], lvl);
            ObjectSetInteger(0, ln, OBJPROP_WIDTH, 2);
            ObjectSetInteger(0, ln, OBJPROP_RAY_RIGHT, false);
            ObjectSetInteger(0, ln, OBJPROP_SELECTABLE, false);
         }
         ObjectSetInteger(0, ln, OBJPROP_COLOR, bd);

         const string lb = OBJ_PREFIX+"MSBT"+IntegerToString(i);
         if(ObjectFind(0, lb) < 0)
         {
            ObjectCreate(0, lb, OBJ_TEXT, 0, time[i], lvl);
            ObjectSetInteger(0, lb, OBJPROP_FONTSIZE, 8);
            ObjectSetInteger(0, lb, OBJPROP_SELECTABLE, false);
         }
         ObjectSetInteger(0, lb, OBJPROP_COLOR, bd);
         ObjectSetString(0, lb, OBJPROP_TEXT, "MSB");
      }
   }

   ChartRedraw();
   return rates_total;
}
//+------------------------------------------------------------------+
