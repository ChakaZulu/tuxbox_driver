
/*****************************************************************************
  void * dboxide_insw_loop(void * dest, void * end, void * src, void * level)

  main data transfer loop to read data

  dest is pointer to RAM
  end is the pointer to the last address where this loop should write to.
  return value is dest after reading

  src  is the address of the fifo in the CPLD
  level is the address of the level register
*****************************************************************************/
.text
.globl dboxide_insw_loop
dboxide_insw_loop:
  nop
  nop
  /* not yet implemented */
  blr

/*****************************************************************************
  void * dboxide_outsw_loop(void * src, void * end, void * dest, void * level)

  main data transfer loop to write data

  src is pointer to RAM
  end is the pointer for the last word to address where this loop should 
      read from
  return value is src after writing

  dest  is the address of the fifo in the CPLD
  level is the address of the level register
*****************************************************************************/
.text
.globl dboxide_outsw_loop
dboxide_outsw_loop:
  nop
  nop
  /* not yet implemented */
  blr
