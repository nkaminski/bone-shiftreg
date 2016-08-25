#include "registers.h"
#include "shiftreg_pru.h"

void shiftreg_clear(shiftreg_t *sr){
 	//set clear and make sure clock is zero
  //Clear logic is inverted
	R30_CLR(sr->clear);
	R30_CLR(sr->serclk);

	//latch it
	R30_SET(sr->latch);
	__delay_cycles(BIT_TIME_CYC);
	R30_CLR(sr->latch);

	//unset clear
	R30_SET(sr->clear);
	__delay_cycles(BIT_TIME_CYC);

}

void shiftreg_iterate(shiftreg_t *sr, char *ser0_val, char *ser1_val, char ser2_val, unsigned char count){
  int i, end;
  end = min(sr->nbits, MAX_BITS);
	R30_CLR(sr->serclk);
  
  for(i=end-1; i>=0; i--){
    if(count <= ser0_val[i]){
        //Shift out a 1
        R30_SET(sr->ser0);
    }
    else{
    //shift out a 0
        R30_CLR(sr->ser0);
    }
    if(count <= ser1_val[i]){
        //Shift out a 1
        R30_SET(sr->ser1);
    }
    else{
    //shift out a 0
        R30_CLR(sr->ser1);
    }
     if(count <= ser2_val[i]){
        //Shift out a 1
        R30_SET(sr->ser2);
    }
    else{
    //shift out a 0
        R30_CLR(sr->ser2);
    } 
    //Serclk pulse
	  __delay_cycles(BIT_TIME_CYC);
    R30_SET(sr->serclk);
	  __delay_cycles(BIT_TIME_CYC);
    R30_CLR(sr->serclk);
	  __delay_cycles(BIT_TIME_CYC);
  } 
  //full round of data output, latch it
	R30_SET(sr->latch);
	__delay_cycles(BIT_TIME_CYC);
	R30_CLR(sr->latch);
}
