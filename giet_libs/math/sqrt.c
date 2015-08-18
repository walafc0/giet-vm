
double sqrt(double x)
{
  double z;
  __asm__ ("sqrt.d %0,%1" : "=f" (z) : "f" (x));
  return z;
}

