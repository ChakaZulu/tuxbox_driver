
// Gibt die Bits eines Registers aus. Nett zum debuggen (fuer Enduser :-) )
// Nix optimiert, da eh nur fuers debuggen
static void dprintkRegBits(const char *regname, u32 reg, int numberOfBits)
{
  int i, j;
  // Nibbles ausblenden die nich angezeigt werden sollen (%0nx)
  int nibbles=numberOfBits/4+(numberOfBits%4 ? 1 : 0 ); // :-)
  for (i=7; i>=nibbles; i--)
    reg&=~(0xf<<i*4);
  // und die Ausgabe
  j=numberOfBits*3+1-strlen(regname)-7-nibbles;
  if(j<0)
    j=0;
  for(i=0; i<j/2; i++)
    dprintk("-");
  dprintk(" %s (0x%0*x) ", regname, nibbles, reg);
  for(; i<j; i++)
    dprintk("-");
  dprintk("\n");
  for(i=numberOfBits-1; i>=0; i--)
    dprintk("|%2d", i);
  dprintk("|\n");
  for(i=numberOfBits-1; i>=0; i--)
    dprintk("|%2d", (reg&(1<<i))>>i);
  dprintk("|\n");
  for(i=0; i<numberOfBits*3+1; i++)
    dprintk("-");
  dprintk("\n");
}

