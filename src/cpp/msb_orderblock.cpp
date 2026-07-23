//+------------------------------------------------------------------+
//| msb_orderblock.cpp                                               |
//| Porte C++ do "Market Structure Break & Order Block" (c) EmreKb   |
//|                                                                  |
//| Mapa Pine -> C++:                                                |
//|   linhas 52-57    to_up/to_down/trend      -> bloco 1            |
//|   linhas 59-65    low_val/high_val + index -> bloco 2            |
//|   linhas 67-73    push dos pivos           -> bloco 3            |
//|   linhas 104-109  market (o MSB)           -> bloco 4            |
//|   linhas 111-145  varredura dos OB/BB      -> bloco 5            |
//+------------------------------------------------------------------+
#include "msb_orderblock.h"
#include <cmath>

namespace {

constexpr int kMaxPivots = 512;   // historico de pivos guardado

inline bool is_na(double x) { return x != x; }

//--- ta.highest(high, len) terminando em i
double highest_at(const double *x, int i, int len)
{
   int from = i - len + 1;
   if(from < 0) from = 0;
   double m = x[from];
   for(int k = from + 1; k <= i; ++k) if(x[k] > m) m = x[k];
   return m;
}
double lowest_at(const double *x, int i, int len)
{
   int from = i - len + 1;
   if(from < 0) from = 0;
   double m = x[from];
   for(int k = from + 1; k <= i; ++k) if(x[k] < m) m = x[k];
   return m;
}

struct PivotList
{
   double val[kMaxPivots];
   int    bar[kMaxPivots];
   int    n;
   void reset() { n = 0; }
   void push(double v, int b)
   {
      if(n < kMaxPivots) { val[n] = v; bar[n] = b; ++n; }
      else
      {
         for(int k = 1; k < kMaxPivots; ++k) { val[k-1] = val[k]; bar[k-1] = bar[k]; }
         val[kMaxPivots-1] = v; bar[kMaxPivots-1] = b;
      }
   }
   //--- f_get_high/f_get_low do Pine: ind=0 e o mais recente
   bool get(int ind, double &v, int &b) const
   {
      const int idx = n - 1 - ind;
      if(idx < 0) return false;
      v = val[idx]; b = bar[idx];
      return true;
   }
};

} // namespace

//+------------------------------------------------------------------+
int __stdcall MsbVersion(void) { return MSB_ABI_VERSION; }

//+------------------------------------------------------------------+
int __stdcall MsbCalculate(
   const double *open, const double *high, const double *low, const double *close,
   double *trend, double *market, double *msbEvent,
   double *h0b, double *h1b, double *l0b, double *l1b,
   double *h0ib, double *h1ib, double *l0ib, double *l1ib,
   double *obBar, double *bbBar, double *bbIsBreaker,
   int rates_total, int zigzagLen, double fibFactor)
{
   const void *ptrs[] = { open, high, low, close, trend, market, msbEvent,
                          h0b, h1b, l0b, l1b, h0ib, h1ib, l0ib, l1ib,
                          obBar, bbBar, bbIsBreaker };
   for(unsigned n = 0; n < sizeof(ptrs)/sizeof(ptrs[0]); ++n)
      if(ptrs[n] == nullptr) return -1;

   if(rates_total <= 0)                 return -1;
   if(zigzagLen < 2)                    return -1;
   if(fibFactor < 0.0 || fibFactor > 1.0) return -1;   // Pine: 0..1

   PivotList highs, lows;
   highs.reset(); lows.reset();

   int    curTrend  = 1;
   int    curMarket = 1;
   double lastL0 = NAN, lastH0 = NAN;   // ta.valuewhen(change(market), l0/h0, 0)

   //--- historico de l0i/h0i para resolver `l0i[zigzagLen]` do Pine
   //    (o autor usa o valor do indice de zigzagLen barras atras)
   static const int kLagMax = 4096;
   int l0iHist[kLagMax], h0iHist[kLagMax];
   for(int k = 0; k < kLagMax; ++k) { l0iHist[k] = -1; h0iHist[k] = -1; }

   //--- barssince(to_up[1]) e barssince(to_down[1])
   int sinceUpPrev = -1, sinceDownPrev = -1;
   bool prevToUp = false, prevToDown = false;

   for(int i = 0; i < rates_total; ++i)
   {
      //--- 1. zigzag [Pine 52-57]
      const bool to_up   = (high[i] >= highest_at(high, i, zigzagLen));
      const bool to_down = (low[i]  <= lowest_at (low,  i, zigzagLen));

      if(curTrend == 1 && to_down)       curTrend = -1;
      else if(curTrend == -1 && to_up)   curTrend =  1;

      //--- barssince sobre o valor da barra ANTERIOR
      if(sinceUpPrev   >= 0) ++sinceUpPrev;
      if(sinceDownPrev >= 0) ++sinceDownPrev;
      if(prevToUp)   sinceUpPrev   = 0;
      if(prevToDown) sinceDownPrev = 0;

      //--- 2. extremos da perna corrente [Pine 59-65]
      const int upLen = (sinceUpPrev   > 0) ? sinceUpPrev   : 1;
      const int dnLen = (sinceDownPrev > 0) ? sinceDownPrev : 1;

      const double low_val  = lowest_at (low,  i, upLen);
      const double high_val = highest_at(high, i, dnLen);

      //--- barssince(low_val == low): quantas barras desde a ocorrencia
      int lowIdx = i, highIdx = i;
      for(int k = i; k >= 0 && k > i - kLagMax; --k)
         if(low[k] == low_val) { lowIdx = k; break; }
      for(int k = i; k >= 0 && k > i - kLagMax; --k)
         if(high[k] == high_val) { highIdx = k; break; }

      //--- 3. empilha o pivo quando o trend vira [Pine 67-73]
      const bool trendChanged = (i > 0) && ((int)trend[i-1] != curTrend);
      if(trendChanged)
      {
         if(curTrend == 1)       lows.push (low_val,  lowIdx);
         else if(curTrend == -1) highs.push(high_val, highIdx);
      }

      double h0=NAN,h1=NAN,l0=NAN,l1=NAN;
      int    h0i=-1,h1i=-1,l0i=-1,l1i=-1;
      highs.get(0,h0,h0i); highs.get(1,h1,h1i);
      lows .get(0,l0,l0i); lows .get(1,l1,l1i);

      //--- guarda o historico de indices para o lag do Pine
      l0iHist[i % kLagMax] = l0i;
      h0iHist[i % kLagMax] = h0i;
      const int lagIdx = i - zigzagLen;
      const int l0iLag = (lagIdx >= 0) ? l0iHist[lagIdx % kLagMax] : -1;
      const int h0iLag = (lagIdx >= 0) ? h0iHist[lagIdx % kLagMax] : -1;

      //--- 4. market / MSB [Pine 104-109]
      int ev = 0;
      const bool havePivots = (h0i >= 0 && h1i >= 0 && l0i >= 0 && l1i >= 0);
      if(havePivots)
      {
         //--- o Pine trava a mudanca enquanto o pivo for o mesmo da
         //    ultima virada (ta.valuewhen)
         const bool locked = ((!is_na(lastL0) && lastL0 == l0) ||
                              (!is_na(lastH0) && lastH0 == h0));
         if(!locked)
         {
            const int before = curMarket;
            if(curMarket == 1 && l0 < l1 &&
               l0 < l1 - std::fabs(h0 - l1) * fibFactor)
               curMarket = -1;
            else if(curMarket == -1 && h0 > h1 &&
                    h0 > h1 + std::fabs(h1 - l0) * fibFactor)
               curMarket = 1;

            if(curMarket != before)
            {
               ev = curMarket;
               lastL0 = l0; lastH0 = h0;
            }
         }
      }

      //--- 5. Order Block / Breaker Block [Pine 111-145]
      //    procura a ULTIMA vela contraria dentro do intervalo
      int ob = -1, bb = -1, isBreaker = 0;
      if(ev != 0)
      {
         if(ev == 1)
         {
            //--- Bu-OB: ultima vela de BAIXA entre h1i e l0i[zigzagLen]
            const int from = h1i, to = (l0iLag >= 0) ? l0iLag : l0i;
            if(from >= 0 && to >= from)
               for(int k = from; k <= to && k < rates_total; ++k)
                  if(open[k] > close[k]) ob = k;

            //--- Bu-BB: ultima vela de ALTA entre l1i-zigzagLen e h1i
            const int f2 = l1i - zigzagLen, t2 = h1i;
            if(f2 >= 0 && t2 >= f2)
               for(int k = f2; k <= t2 && k < rates_total; ++k)
                  if(open[k] < close[k]) bb = k;

            isBreaker = (l0 < l1) ? 1 : 0;      // "Bu-BB" : "Bu-MB"
         }
         else
         {
            //--- Be-OB: ultima vela de ALTA entre l1i e h0i[zigzagLen]
            const int from = l1i, to = (h0iLag >= 0) ? h0iLag : h0i;
            if(from >= 0 && to >= from)
               for(int k = from; k <= to && k < rates_total; ++k)
                  if(open[k] < close[k]) ob = k;

            //--- Be-BB: ultima vela de BAIXA entre h1i-zigzagLen e l1i
            const int f2 = h1i - zigzagLen, t2 = l1i;
            if(f2 >= 0 && t2 >= f2)
               for(int k = f2; k <= t2 && k < rates_total; ++k)
                  if(open[k] > close[k]) bb = k;

            isBreaker = (h0 > h1) ? 1 : 0;      // "Be-BB" : "Be-MB"
         }
      }

      trend[i]    = (double)curTrend;
      market[i]   = (double)curMarket;
      msbEvent[i] = (double)ev;
      h0b[i]=h0; h1b[i]=h1; l0b[i]=l0; l1b[i]=l1;
      h0ib[i]=(double)h0i; h1ib[i]=(double)h1i;
      l0ib[i]=(double)l0i; l1ib[i]=(double)l1i;
      obBar[i]=(double)ob; bbBar[i]=(double)bb;
      bbIsBreaker[i]=(double)isBreaker;

      prevToUp   = to_up;
      prevToDown = to_down;
   }

   return rates_total;
}
