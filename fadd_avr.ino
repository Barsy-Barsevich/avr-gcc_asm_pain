#define XSHLA01  34

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  float a = -13.23;
  float b = 0.;
  float c;
  //PrintFloat(a);
  Serial.print(a);
  Serial.print(" + ");
  Serial.print(b);
  Serial.print(" = ");
  //FADD_S((uint8_t*)&a, (uint8_t*)&b);
  FADD_D(&a, &b, &c);
  Serial.println(c);
  FADD();
}

void loop() {
  // put your main code here, to run repeatedly:
}




void FADD_D(float* op_a, float* op_b, float* rec)
{
  //Выделение порядков
  uint16_t temp;
  uint8_t exp_A, exp_B;
  temp = ((uint16_t)((uint8_t*)op_a)[3]<<1)|(((uint8_t*)op_a)[2]>>7);
  exp_A = temp & 0xFF;
  temp = ((uint16_t)((uint8_t*)op_b)[3]<<1)|(((uint8_t*)op_b)[2]>>7);
  exp_B = temp & 0xFF;
  //Сравнить порядки
  float copy_a, copy_b;
  if (exp_A < exp_B)
  {
    copy_a = *op_b; /*перекрестная замена*/
    copy_b = *op_a; /*копирование операндов в локальные переменные*/
    uint8_t temp8 = exp_A;
    exp_A = exp_B;
    exp_B = temp8;
  }
  else
  {
    copy_a = *op_a;
    copy_b = *op_b;
  }
  //Получение указателей на копии операндов
  uint8_t* a = (uint8_t*)&copy_a;
  uint8_t* b = (uint8_t*)&copy_b;
  //Выделение мантисс
  int32_t man_A, man_B;
  man_A = (int32_t)a[0] | ((int32_t)a[1]<<8) | ((int32_t)a[2]<<16) | (1UL<<23);
  man_B = (int32_t)b[0] | ((int32_t)b[1]<<8) | ((int32_t)b[2]<<16) | (1UL<<23);
  //Получить разницу порядков
  uint8_t exp_razr = exp_A - exp_B;
  if (exp_razr >= 24)
  {
    *rec = copy_a;
    return;
  }
  //Денормализация мантиссы В
  man_B >>= exp_razr;
  //Учет знака
  if (a[3] & 0x80) man_A = -man_A;
  if (b[3] & 0x80) man_B = -man_B;
  //Сложение мантисс
  man_A += man_B;
  //Учет знака
  if (man_A < 0)
  {
    man_A = -man_A;
    ((uint8_t*)rec)[3] = 0x80;
  }
  else ((uint8_t*)rec)[3] = 0x00;
  //Нормализация мантиссы А
  if (man_A > (1ul<<23))
  {
    while (man_A >> 23 != 1)
    {
      man_A >>= 1;
      exp_A += 1;
    }
  }
  else if (man_A < (1ul<<23))
  {
    while (man_A >> 23 != 1)
    {
      man_A <<= 1;
      exp_A -= 1;
    }
  }
  //Выгрузка результата
  ((uint8_t*)rec)[0] = (man_A & 0xFF);
  ((uint8_t*)rec)[1] = ((man_A >> 8) & 0xFF);
  ((uint8_t*)rec)[2] = ((man_A >> 16) & 0x7F) | ((exp_A << 7) & 0x80);
  ((uint8_t*)rec)[3] |= ((exp_A >> 1) & 0x7F);
}













void FADD()
{
  uint8_t temp1, temp2, temp3;
  static float f_a = 100.21234134;
  static float f_b = 130;
  float f_c;
  PrintFloat(f_a);
  PrintFloat(f_b);

  DSCR |= (1<<DSUEN);
  DSCR |= 0xC0;
  
  asm volatile(
    //Выделение порядков
    "adiw  Z, 2"                "\n\t"
    "ld    __tmp_reg__,Z+"      "\n\t"
    "rol   __tmp_reg__"         "\n\t"
    "ld    %[expA],Z"           "\n\t"
    "rol   %[expA]"             "\n\t"
    "adiw  X, 2"                "\n\t"
    "ld    __tmp_reg__,X+"      "\n\t"
    "rol   __tmp_reg__"         "\n\t"
    "ld    %[expB],X"           "\n\t"
    "rol   %[expB]"             "\n\t"
    "sbiw  X, 3"                "\n\t"
    "sbiw  Z, 3"                "\n\t"
    "cp    %[expA],%[expB]"     "\n\t"
    "brlo  expb_ht_expa"        "\n\t" //переход, если expA>=expB
    "rjmp  metka1"              "\n\t"
    "expb_ht_expa:"
    //expB > expA: перекрестный обмен
//    "mov  __tmp_reg__,%[expA]"  "\n\t"
//    "mov  %[expA],%[expB]"      "\n\t"
//    "mov  %[expB],__tmp_reg__"  "\n\t"
    //Вычислить разницу между expA и expB
    "mov  %[exprazr],%[expB]"   "\n\t"
    "sub  %[exprazr],%[expA]"   "\n\t"
    "cpi  %[exprazr],24"        "\n\t" //если >= 24, то возврат операнда В
    "brlo not_return_opB"       "\n\t"
    "rjmp return_opB"           "\n\t"
    "not_return_opB:"
    //Загрузка мантиссы А. Денормализация А
    //"ORI  R31,0x20"               "\n\t" //addr(a)+=$2000
    "ld   r22,Z+"               "\n\t"
    "ld   r23,Z+"               "\n\t"
    "out  %[dsal],r22"          "\n\t"
    "ld   r22,Z+"               "\n\t"
    "ori  r22,0x80"             "\n\t"
    "clr  r23"                  "\n\t"
    "out  %[dsah],r22"          "\n\t"
    //Денормализация А
    "sbrs %[exprazr],4"         "\n\t" //пропустить если exprazr>15
    "rjmp skipshift"            "\n\t"
    "inc  %[exprazr]"           "\n\t"
    "andi %[exprazr],0x0F"      "\n\t"
    "ldi  r22,0xDF"             "\n\t"
    "out  %[dsir],r22"          "\n\t" //DSA содержит мантиссу, сдвинутую на 15 вправо 
    "skipshift:"
    "ori  %[exprazr],0xD0"      "\n\t" //Сделать код операции XSHRAxx, xx - exprazr-0x0F
    "out  %[dsir],%[exprazr]"   "\n\t" //DSA содержит денормализованную мантиссу
    //Учет знака А
    "ld   r22,Z"                "\n\t"
    "sbrs r22,7"                "\n\t"
    "rjmp inv_man_a"            "\n\t"
    "ldi  r22,0x84"             "\n\t" //Operation DA=NEG(DA)
    "out  %[dsir],r22"          "\n\t"
    "ldi  r22,0x01"             "\n\t"
    "clr  r23"                  "\n\t"
    "out  %[dsdy],r22"          "\n\t"
    "ldi  r22,0x17"             "\n\t" //Operation DA=DA+DY
    "out  %[dsir],r22"          "\n\t"
    "inv_man_a:"
    //Выделение мантиссы В, добавление к А
    "ld    r22,X+"              "\n\t"
    "ld    r23,X+"              "\n\t"
    "out   %[dsdy],r22"         "\n\t"
    "ld    __tmp_reg__,X+"      "\n\t" //save MSB(manB)
    "ld    %[expA],X"           "\n\t" //expA больше не нужен
    "sbrc  %[expA],7"           "\n\t"
    "ldi   r23,0x33"            "\n\t" //Operation DA=DA-DY
    "sbrs  %[expA],7"           "\n\t"
    "ldi   r23,0x37"            "\n\t" //Operation DA=DA+DY
    "out   %[dsir],r23"         "\n\t" //Выполнено сложение. Учет флага переноса
    
    //-------------------------------
    //Test
    //"ldi   r22,0x00"            "\n\t"
    //"ldi   r23,0x00"            "\n\t"
    //"out   %[dsal],r22"         "\n\t"
    //"out   %[dsah],r22"         "\n\t"
    "ldi   r22,0x80"            "\n\t"
    "out   %[dsir],r22"         "\n\t" //DA:=0
    "ldi   r22,0xFF"            "\n\t"
    "ldi   r23,0xFF"            "\n\t"
    "out   %[dsdx],r22"         "\n\t" //DX:=0x0100
    "ldi   r22,0x00"            "\n\t"
    "ldi   r23,0xFF"            "\n\t"
    "out   %[dsdy],r22"         "\n\t" //DY:=0x0001
    
    "ldi   r22,0x46"            "\n\t" //DA:=DA+DX*DY
    "out   %[dsir],r22"         "\n\t"
    "in    r22,%[dsdx]"         "\n\t"
    "mov   %[expA],r22"         "\n\t"
    //-------------------------------
//    "ldi   r23,0xF8"            "\n\t"
//    "out   %[dsir],r23"         "\n\t"
//    "mov   r23,__tmp_reg__"     "\n\t"
//    "ori   r23,0x80"            "\n\t"
//    "clr   r22"                 "\n\t"
//    "out   %[dsdy],r22"         "\n\t"
//    "sbrc  %[expA],7"           "\n\t"
//    "ldi   r23,0x33"            "\n\t" //Operation DA=DA-DY
//    "sbrs  %[expA],7"           "\n\t"
//    "ldi   r23,0x37"            "\n\t" //Operation DA=DA+DY
//    "out   %[dsir],r23"         "\n\t"
//    "ldi   r23,0xC8"            "\n\t"
//    "out   %[dsir],r23"         "\n\t" //DSA содержит сумму мантисс. Теперь нужно взять по модулю
    //Взятие по модулю, учет знака
    "movw  Z,%[rec]"            "\n\t"
    "clr   r22"                 "\n\t"
    "std   Z+3,r22"             "\n\t"
//    "sbic  %[dscr],2"           "\n\t" //пропустить, если положительный
//    "rjmp  skip_if_plus"        "\n\t"
//    "ldi   r23,0xA1"            "\n\t" //Operation DA=ABS(DA)
//    "out   %[dsir],r23"         "\n\t"
//    "ldi   r22,0x80"            "\n\t"
//    "std   Z+3,r22"             "\n\t"
    "skip_if_plus:"
/* Нормализация мантиссы ??? */
    //Загрузка результата
    //"movw  Z,%[rec]"            "\n\t"
    "in    r22,%[dsal]"         "\n\t"
    "st    Z+,r22"              "\n\t"
    "st    Z+,r23"              "\n\t"
    "in    r22,%[dsah]"         "\n\t"
    "bst   %[expB],0"           "\n\t"
    "bld   r22,7"               "\n\t"
    "st    Z+,r22"              "\n\t"
    "ror   %[expB]"             "\n\t"
    "andi  %[expB],0x7F"        "\n\t"
    "ld    r23,Z"               "\n\t"
    "or    r23,%[expB]"         "\n\t"
    "st    Z,r23"               "\n\t"

    
//    "in    r22,%[dsah]"         "\n\t"
//    "mov   %[expA],r22"         "\n\t"
    //"mov  %[expB],r22"          "\n\t"
    


    "rjmp  return"              "\n\t"
    
    "metka1:"

    //Возврат операнда В
    "return_opB:"
    "MOVW  Z,%[rec]"            "\n\t"
    "LD    __tmp_reg__,X+"      "\n\t"
    "ST    Z+,__tmp_reg__"      "\n\t"
    "LD    __tmp_reg__,X+"      "\n\t"
    "ST    Z+,__tmp_reg__"      "\n\t"
    "LD    __tmp_reg__,X+"      "\n\t"
    "ST    Z+,__tmp_reg__"      "\n\t"
    "LD    __tmp_reg__,X+"      "\n\t"
    "ST    Z+,__tmp_reg__"      "\n\t"
    "RJMP  return"              "\n\t"
    //Возврат операнда А
    "return_opA:"
    "MOVW X,%[rec]"             "\n\t"
    "LD   __tmp_reg__,Z+"       "\n\t"
    "ST   X+,__tmp_reg__"       "\n\t"
    "LD   __tmp_reg__,Z+"       "\n\t"
    "ST   X+,__tmp_reg__"       "\n\t"
    "LD   __tmp_reg__,Z+"       "\n\t"
    "ST   X+,__tmp_reg__"       "\n\t"
    "LD   __tmp_reg__,Z+"       "\n\t"
    "ST   X+,__tmp_reg__"       "\n\t"
    "RJMP return"               "\n\t"
    
    //Загрузка мантисс

    "return:"
    ::
    [dsir]"I" (_SFR_IO_ADDR(DSIR)),
    [dscr]"I" (_SFR_IO_ADDR(DSCR)),
    [dsdx]"I" (_SFR_IO_ADDR(DSDX)),
    [dsdy]"I" (_SFR_IO_ADDR(DSDY)),
    [dsal]"I" (_SFR_IO_ADDR(DSAL)),
    [dsah]"I" (_SFR_IO_ADDR(DSAH)),
    [dssd]"I" (_SFR_IO_ADDR(DSSD)),
    [xshla01]"M" (XSHLA01),
    [expA]"d" (temp1),
    [expB]"d" (temp2),
    [exprazr]"d" (temp3),
    "z" (&f_a),
    "x" (&f_b),
    [rec]"w" (&f_c)
    :"r22", "r23"
  );
  PrintFloat(f_c);
  Serial.print(";  ");
  Serial.println(temp1);
}

void BCDtoASCII(uint8_t in, uint8_t* out)
{
  out[0] = ((in>>4)&0x0F) + 0x30;
  out[1] = (in & 0x0F) + 0x30;
  if (out[0] > 0x39) out[0] += 7;
  if (out[1] > 0x39) out[1] += 7;
}

void PrintFloat(float a)
{
  uint8_t* u = (uint8_t*)&a;
  uint8_t buf[2];
  BCDtoASCII(u[0], buf);
  Serial.write(buf[0]);
  Serial.write(buf[1]);
  Serial.print(" ");
  BCDtoASCII(u[1], buf);
  Serial.write(buf[0]);
  Serial.write(buf[1]);
  Serial.print(" ");
  BCDtoASCII(u[2], buf);
  Serial.write(buf[0]);
  Serial.write(buf[1]);
  Serial.print(" ");
  BCDtoASCII(u[3], buf);
  Serial.write(buf[0]);
  Serial.write(buf[1]);
  Serial.print("\n");
}
