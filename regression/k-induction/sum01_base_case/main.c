#define a (2)
unsigned int nondet_uint();
int main() { 
  int i, n=12, sn=0;
  for(i=1; i<=n; i++) {
    if (i<10) //first case
    sn = sn + a; //second case OK
    //if (i==4) sn=-10; //third case OK
  }
  //__ESBMC_assume(i>n);
  assert(sn==n*a || sn == 0);
}
