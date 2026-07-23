//+------------------------------------------------------------------+
//| test_msb.cpp — teste de mesa da DLL msb_orderblock.dll          |
//+------------------------------------------------------------------+
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <vector>

typedef int (__stdcall *fn_ver_t)(void);
typedef int (__stdcall *fn_calc_t)(
   const double*, const double*, const double*, const double*,
   double*, double*, double*,
   double*, double*, double*, double*,
   double*, double*, double*, double*,
   double*, double*, double*,
   int, int, double);

static fn_ver_t  MsbVersion   = nullptr;
static fn_calc_t MsbCalculate = nullptr;
static int g_fail = 0;

static void check(bool ok, const char *name, const char *detail = "")
{
   std::printf("%-6s %s %s\n", ok ? "[ OK ]" : "[FAIL]", name, detail);
   if(!ok) ++g_fail;
}
static bool is_na(double x) { return x != x; }

struct B
{
   std::vector<double> tr, mk, ev, h0,h1,l0,l1, h0i,h1i,l0i,l1i, ob, bb, brk;
   explicit B(int n) : tr(n,0),mk(n,0),ev(n,0),h0(n,0),h1(n,0),l0(n,0),l1(n,0),
                       h0i(n,0),h1i(n,0),l0i(n,0),l1i(n,0),ob(n,0),bb(n,0),brk(n,0) {}
};

static int call(const std::vector<double>&o,const std::vector<double>&h,
                const std::vector<double>&l,const std::vector<double>&c,
                B &b, int n, int zl = 9, double fib = 0.33)
{
   return MsbCalculate(o.data(),h.data(),l.data(),c.data(),
      b.tr.data(),b.mk.data(),b.ev.data(),
      b.h0.data(),b.h1.data(),b.l0.data(),b.l1.data(),
      b.h0i.data(),b.h1i.data(),b.l0i.data(),b.l1i.data(),
      b.ob.data(),b.bb.data(),b.brk.data(),
      n, zl, fib);
}

int main()
{
   HMODULE hm = LoadLibraryA("msb_orderblock.dll");
   if(!hm){ std::printf("[FAIL] LoadLibrary erro %lu\n", GetLastError()); return 1; }
   MsbVersion   = (fn_ver_t)(void*)GetProcAddress(hm,"MsbVersion");
   MsbCalculate = (fn_calc_t)(void*)GetProcAddress(hm,"MsbCalculate");
   if(!MsbVersion||!MsbCalculate){ std::printf("[FAIL] GetProcAddress\n"); return 1; }

   check(MsbVersion()==1, "T1 ABI version == 1");

   const int N = 1500;
   std::vector<double> o(N),h(N),l(N),c(N);
   for(int i=0;i<N;++i)
   {
      const double base = 100.0 + 10.0*std::sin(i/31.0) + 3.0*std::sin(i/7.0) + 0.02*i;
      const double rng  = 0.4 + 0.3*std::fabs(std::sin(i/5.0));
      c[i]=base; o[i]=base - 0.3*std::sin(i/2.0);
      h[i]=((c[i]>o[i])?c[i]:o[i])+rng;
      l[i]=((c[i]<o[i])?c[i]:o[i])-rng;
   }

   //--- T2 fail-fast
   {
      B b(N);
      const int r1 = MsbCalculate(nullptr,h.data(),l.data(),c.data(),
         b.tr.data(),b.mk.data(),b.ev.data(),b.h0.data(),b.h1.data(),b.l0.data(),b.l1.data(),
         b.h0i.data(),b.h1i.data(),b.l0i.data(),b.l1i.data(),
         b.ob.data(),b.bb.data(),b.brk.data(), N, 9, 0.33);
      const int r2 = call(o,h,l,c,b,N,1);          // zigzagLen < 2
      const int r3 = call(o,h,l,c,b,N,9,1.5);      // fib fora de [0,1]
      check(r1==-1 && r2==-1 && r3==-1, "T2 fail-fast em argumento invalido");
   }

   B ref(N);
   check(call(o,h,l,c,ref,N)==N, "T0 recalculo completo retorna rates_total");

   //--- T3 trend e market so assumem +1/-1
   {
      bool ok=true; int up=0,dn=0;
      for(int i=0;i<N;++i)
      {
         if(ref.tr[i]!=1.0 && ref.tr[i]!=-1.0) ok=false;
         if(ref.mk[i]!=1.0 && ref.mk[i]!=-1.0) ok=false;
         if(ref.tr[i]>0) ++up; else ++dn;
      }
      char d[96]; std::snprintf(d,sizeof(d),"(trend +1:%d -1:%d)",up,dn);
      check(ok && up>0 && dn>0, "T3 trend/market em {-1,+1} e alternam", d);
   }

   //--- T4 msbEvent so ocorre quando market muda
   {
      bool ok=true; int evs=0;
      for(int i=1;i<N;++i)
      {
         const bool changed = (ref.mk[i] != ref.mk[i-1]);
         if(ref.ev[i] != 0.0)
         {
            ++evs;
            if(!changed || ref.ev[i] != ref.mk[i]) ok=false;
         }
         else if(changed) ok=false;
      }
      char d[96]; std::snprintf(d,sizeof(d),"(%d eventos MSB)",evs);
      check(ok && evs>0, "T4 msbEvent coincide com a virada de market", d);
   }

   //--- T5 pivos em ordem temporal e valores coerentes com o preco
   {
      bool ok=true; int checked=0;
      for(int i=0;i<N;++i)
      {
         if(ref.h0i[i]<0||ref.h1i[i]<0||ref.l0i[i]<0||ref.l1i[i]<0) continue;
         const int a=(int)ref.h1i[i], b=(int)ref.h0i[i];
         if(a > b) ok=false;                       // h1 e mais antigo que h0
         const int x=(int)ref.l1i[i], y=(int)ref.l0i[i];
         if(x > y) ok=false;
         //--- o valor do pivo tem de bater com o high/low daquela barra
         if(std::fabs(ref.h0[i]-h[b]) > 1e-9) ok=false;
         if(std::fabs(ref.l0[i]-l[y]) > 1e-9) ok=false;
         ++checked;
      }
      char d[96]; std::snprintf(d,sizeof(d),"(%d barras conferidas)",checked);
      check(ok && checked>100, "T5 pivos ordenados e alinhados ao preco", d);
   }

   //--- T6 OB/BB apontam para velas do tipo certo
   {
      bool ok=true; int obs=0;
      for(int i=0;i<N;++i)
      {
         if(ref.ev[i]==0.0) continue;
         const int ob=(int)ref.ob[i], bb=(int)ref.bb[i];
         if(ob>=0)
         {
            ++obs;
            //--- MSB de alta usa vela de BAIXA como OB; de baixa, o inverso
            if(ref.ev[i]>0 && !(o[ob] > c[ob])) ok=false;
            if(ref.ev[i]<0 && !(o[ob] < c[ob])) ok=false;
         }
         if(bb>=0)
         {
            if(ref.ev[i]>0 && !(o[bb] < c[bb])) ok=false;
            if(ref.ev[i]<0 && !(o[bb] > c[bb])) ok=false;
         }
      }
      char d[96]; std::snprintf(d,sizeof(d),"(%d order blocks)",obs);
      check(ok, "T6 OB/BB apontam para vela do tipo correto", d);
   }

   //--- T7 determinismo
   {
      B again(N);
      call(o,h,l,c,again,N);
      double md=0;
      for(int i=0;i<N;++i) md = std::fmax(md, std::fabs(ref.mk[i]-again.mk[i]));
      char d[64]; std::snprintf(d,sizeof(d),"(max_diff=%.1f)",md);
      check(md==0.0, "T7 duas chamadas produzem o mesmo resultado", d);
   }

   FreeLibrary(hm);
   std::printf("\n%s  (%d falha(s))\n", g_fail==0?"TODOS OS TESTES PASSARAM":"TESTES FALHARAM", g_fail);
   return g_fail==0?0:1;
}
