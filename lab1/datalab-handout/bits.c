/*
 * CS:APP Data Lab 
 * 
 * <Haoyang Yuan; haoyangy@andrew.cmu.edu>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
//1
/* 
 * bitXor - x^y using only ~ and & 
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y) {
    
    int temp1 = ~x&y;
    int temp2 = x&~y;
    int temp3 =  (~temp1)&(~temp2);
    return ~temp3;
}
/* 
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
    int temp = 1;
    temp = (temp<<31);
  return temp;
}
//2
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 2
 */
int isTmax(int x) {
    int temp = 0x0;
    int temp1 = 0x0;
    temp = x + x + 1 + 1;
    temp1 = ~x;
    return (!temp) & !(!temp1);

}
/* 
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x) {
    int temp = (x>>16)&x;
    int result =  0x0;
    int temp1 = (temp>>8)&temp;
    temp1 = temp1 & 0xAA;
    result = temp1 ^ 0xAA;
    return !result;
}
/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
    int temp = x;
    temp = ~temp;
    return temp+1;
}
//3
/* 
 * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
 *   Example: isAsciiDigit(0x35) = 1.
 *            isAsciiDigit(0x3a) = 0.
 *            isAsciiDigit(0x05) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int isAsciiDigit(int x) {
    int temp = 0x30 >> 4;
    int temp1 = x >> 4;
    int temp2 = !(temp^temp1);
    int temp3 = 0x0;
    temp = ~0x3a + 1;
    temp1 = temp + x;
    temp1 = temp1 >> 31;
    temp3 = !((temp1&0xff)^0xff);
    
    
    return temp2 & temp3;
}
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
    int temp1 = !x<<31;
    int temp2 = temp1>>31;
    
    int result1 = temp2&z;
    int result2 = ~temp2&y;
    
    return result1|result2 ;
}
/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y) {
    
    int sign1 = (x >> 31) & 0x1;
    int sign2 = (y >> 31) & 0x1;
    int special = sign1 & !sign2;
    int special2 = !sign1 & sign2;
    
    int same = !(x^y);
    int temp = ~(x)+0x1;
    int result = y+temp;
   
    result = result >> 31;
    
    return (!result | special | same) & !special2;
}
//4
/* 
 * logicalNeg - implement the ! operator, using all of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
    int temp = x>>31;
    int temp1 = (~x+1) >>31;
    int result = temp | temp1;
    return result+1;
}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
    int sign = x >> 31;
    int num = 0x0;
    int count = 0;
    num = sign&(~x+1) + (~sign&x);
    
    
    
    
    return count+!(sign&0x1);
    
  return 0;
}
//float
/* 
 * float_twice - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_twice(unsigned uf) {
    unsigned exp = 0x0;
    unsigned sign = 0x0;
    unsigned result = 0x0;
    int msb = 0x0;
    
    exp = uf & 0x7f800000;
    sign = uf&0x80000000;
    msb = uf&0x7fffff;
    
    if(exp ==0x7f800000) {
        return uf;
    }
    else if (exp == 0x0){
        
        if ((msb&0x400000) == 0x400000){
            exp = 0x800000;
            msb = (msb << 1) & 0x7fffff;
            result = sign + exp + msb;
            return result;
        }
        else{
            msb = (msb << 1) & 0x4fffff;
            result = sign + exp + msb;
            return result;
        }
    }
    
    else {
        exp = exp + 0x800000;
        result = sign + exp + msb;
    }
    return result;
    
    
}
/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {
    unsigned result = 0x0;
    unsigned sign = 0x0;
    unsigned count = 0;
    unsigned exp = 0x4b000000;
    unsigned temp = 0x0;
    unsigned temp1 = 0x0;
    unsigned para = 0x0;
    unsigned para1 = 0x0;
    
    if (x < 0) {
        sign = 0x80000000;
        x = -x;
    }
    if (x==0) {
        return 0;
    }
    if (x == 0x80000000)  {
        return 0xcf000000;
    }
    result = x;
    //decide to shift
    while(x!=0x1){
        x = x >> 1;
        count = count + 1;
    }
    //if shift right, calculate the round situation, and change result in advance
    if (count>23){
        para = 1<<(count-23); //bit24 detection for round
        temp =  (para<<1) - 1; //mask for the last few bits
        x = result&temp;
        temp1 = temp>>1;
        para1 = para >>1;
        
        if( (x& (temp1) ) > ( para1) )
            result = result + (para);
        else if ( (x& (temp1)) == (para1) && (x& para) >0 ){
            result = result + (para);
        }
        while((result&0xff800000)!=0x800000){
                result = result >> 1;
                exp = exp + 0x800000;
        }
    }
    //now, result is under round, and we can normally shift it
    
    else {
        count = 23 - count;
            result = result << count;
            exp = exp - ( count << 23 );
    }
    
    result = (result&0x7fffff) + exp + sign;
    return result;
}
/* 
 * float_f2i - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int float_f2i(unsigned uf) {
    
    unsigned exp =0x0;
    unsigned content =0x0;
    unsigned overflow = 0x0;
    unsigned sign =0x0;
    if(uf==0 ||uf==0x80000000){return 0;}
    exp = uf & 0x7f800000;
    
    if (exp == 0x7f800000 ) return 0x80000000u;
   
    exp = (exp >>23)&0xff;
    
    sign =  (uf>>31)&0x1;
    content = (uf&0x7fffff) | 0x800000;
    
    if (exp > 127){
        
        while(exp!=127){
            if (((content&0x80000000 )== 0x80000000) && exp>127)
            {content = 0x80000000u;
                overflow = 1;
                break;
            }
            content = content << 1;
            exp--;
        }
    }
    else if (exp<127){
        content = 0;
        overflow = 1;
    }
    
    if(overflow == 0) {
        
        content = content >> 23;
        if(sign == 0x1){
            content = - content;}
    }
  return content;
}
