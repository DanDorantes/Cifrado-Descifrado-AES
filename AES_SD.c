#include <io.h>
#include <delay.h>
#include <stdio.h>
#include <ff.h>
#include <display.h>
#include <tablas.h>

#asm
    .equ __lcd_port=0x11
    .equ __lcd_EN=4
    .equ __lcd_RS=5
    .equ __lcd_D4=0
    .equ __lcd_D5=1
    .equ __lcd_D6=2
    .equ __lcd_D7=3
#endasm

char NombreArchivo[17];
char Cadena[15];
char NombreImp[17] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',};
char BloquesImp[17];

unsigned long int mseg=0,tiempo, tiempo1,tiempo2;
unsigned long int tiempoC,tiempoR,tiempoW;
char opcion;
unsigned char i;
unsigned char Llave[176];   //Llave original (0 a 15) y extendida el resto
unsigned long NumBloques,j;
unsigned char error;
unsigned char Buffer[16];

void ImprimeError(int Err)
{
    switch (Err)
    {
        case 1:
            printf("(1) A hard error occured in the low level disk I/O layer");
            break;
        case 2:
            printf("(2) Assertion failed");
            break;
        case 3:
            printf("(3) The physical drive doesn't work ");
            break;   
        case 4:
            printf("(4) Could not find the file");
            break;   
        case 5:
            printf("(5) Could not find the path");
            break;   
        case 6:
            printf("(6) The path name format is invalid");
            break;
        case 7:
            printf("(7) Acces denied due to prohibited access or directory full ");
            break;
        case 8:
            printf("(8) Acces denied due to prohibited access ");
            break;   
        case 9:
            printf("(9) The file/directory object is invalid ");
            break;   
        case 10:
            printf("(10) The physical drive is write protected ");
            break;        
        case 11:
            printf("(11) The logical drive number is invalid ");
            break;
        case 12:
            printf("(12) The volume has no work area ");
            break;
        case 13:
            printf("(13) There is no valid FAT volume ");
            break;   
        case 14:
            printf("(14) f_mkfs() aborted due to a parameter error ");
            break;   
        case 15:
            printf("(15) Could not access the volume within the defined period ");
            break;                       
    }
}
interrupt [TIM1_COMPA] void timer1_compa_isr(void)
{
disk_timerproc();
/* MMC/SD/SD HC card access low level timing function */
}
  

// Timer2 output compare interrupt service routine
interrupt [TIM2_COMPA] void timer2_compa_isr(void)
{
  mseg++;
}  

//Rotación de palabra de 4 bytes
void RotWord(char word[4]){
    int i;
    char last;      
    last = word[0];             // Guardar el valor del primer byte en una variable temporal    
        
    for(i = 0; i < 3; i++){       
       word[i] = word[i+1];     //Rotar los primeros 3 valores por el siguiente   
    }    
    
    word[3] = last;             // Asignar el valor del primer byte anterior a la última posición
}


//Expansión de llave
void KeyExpansion (char expandedkey[176]){
    char temp[4], c = 16, i = 1;
    int a;                           
    
    while(c < 176){                     
        for(a = 0; a < 4; a++){
            temp[a] = expandedkey[a+c-4];       // Guardar el último conjunto de 4 bytes en una variable temporal
        }                                
        if(c % 16 == 0){                        //Solo si es el primer bloque de la llave
            RotWord(temp);                      //Rotar la palabra
            for(a = 0; a < 4; a++){
                temp[a] = sbox[temp[a]];        //Buscar y reeplazar su equivalente sbox
            }                            
            temp[0] = temp[0] ^ rcon[i];        // Hacer el xor con el valor de rcon e incrementar el valor
            i++;
        }
        for(a = 0; a < 4; a++){
             expandedkey[c] = expandedkey[c - 16] ^ temp[a]; // Hacer el xor con la palabra 16 posiciones atrás e incrementar la cuenta
             c++;
        } 
        
    }
}

//SubBytes, encontrar byte equivalente en la tablade substitucion dada ya
void subBytes(unsigned char state[16])
{
    unsigned char i;
    for(i=0;i<16;i++)
    {
        state[i]=sbox[state[i]];    //Loop por cada byte y substituir
    }
}

//ShiftRows, rotar la filas
void shiftRows(unsigned char state[16])
{
    unsigned char i,temp[16];  
    for (i=0;i<16;i++){
        temp[i]=state[i];           //Guardar el bloque en una variable temporar
    }
      
    for(i=0;i<16;i+=4){
        state[i]=temp[i];           //Primera fila se queda igual 
        state[i+1]=temp[(i+5)%16];  //Segunda fila se recorre un espacio
        state[i+2]=temp[(i+10)%16]; //Tercera fila se recorre 2 espacios
        state[i+3]=temp[(i+15)%16]; //Cuarta fila se recorre 4 espacios
    }
     
}

void MixColumns(unsigned char state[16])
{
    unsigned char i,a0,a1,a2,a3;
    for(i=0;i<16;i+=4)
    {
        a0 = state[i];                      //Guardar los bytes de la columna en variables temporales
        a1 = state[i+1];
        a2 = state[i+2];
        a3 = state[i+3];
        state[i]   = M2[a0]^M3[a1]^a2^a3;   //Se multiplican los valores por unos ya preestablecidos de Rijndael
        state[i+1] = M2[a1]^M3[a2]^a3^a0;
        state[i+2] = M2[a2]^M3[a3]^a0^a1;
        state[i+3] = M2[a3]^M3[a0]^a1^a2;
    }
}

void Cifrado(unsigned char Buffer[16], unsigned char Llave[176])
{
    unsigned char i,r;
    unsigned char state[16];  
    
    //Guardar el bloque en una variable temporal
    for(i=0;i<16;i++)
    {
        state[i]=Buffer[i];
    }
    
    //Primera ronda, agregar llave de la Ronda
    for (i=0;i<16;i++)
    {
      state[i]=state[i]^Llave[i];
    }   
    
    //For de las ronda 1 a 9
    for (r=1;r<10;r++)
    {
        //SubBytes
        subBytes(state);
          
        //ShiftRows
        shiftRows(state); 
      
        //MixColumns
        MixColumns(state);
       
        //Agregar llave de la Ronda
        for (i=0;i<16;i++)
        {
            state[i]=state[i]^Llave[(r)*(16)+i];
        }
    }                            
    
    //Ronda final, sin MixColumns
    //SubBytes
    subBytes(state);
    
    //ShiftRows   
    shiftRows(state);           

    //Add Round Key (160 a 175)
    for (i=0;i<16;i++)
    {
         state[i]=state[i]^Llave[160+i];
         
    }  
    for (i=0;i<16;i++)
    {
         Buffer[i]=state[i];
    }    
}

void InvSubBytes(unsigned char state[16])  //Inverse Sub Bytes
{
    unsigned char i;
    for(i=0;i<16;i++)
    {
        state[i]=rsbox[state[i]];    //Loop por cada byte y substituir
    }
} 
                                                               
void InvShiftRows(unsigned char state[16]) //Inverse Shift Rows
{
    unsigned char i,temp[16];  
    for (i=0;i<16;i++){
        temp[i]=state[i];           //Guardar el bloque en una variable temporar
    }
      
    for(i=0;i<16;i+=4){
        state[i]=temp[i];           //Primera fila se queda igual 
        state[i+1]=temp[(i+13)%16];  //Segunda fila se recorre 4 espacios a la izquierda 
        state[i+2]=temp[(i+10)%16]; //Tercera fila se recorre 2 espacio a la izquierda
        state[i+3]=temp[(i+7) %16]; //Cuarta fila se recorre 1 espacio a la izquierda
    }
     
}

void InvMixColumns(unsigned char state[16])  //Inverse Mix Columns
{
    unsigned char i,a0,a1,a2,a3;
    for(i=0;i<16;i+=4)
    {
        a0 = state[i];                      //Guardar los bytes de la columna en variables temporales
        a1 = state[i+1];
        a2 = state[i+2];
        a3 = state[i+3];
        state[i]   = M14[a0]^M11[a1]^M13[a2]^M9[a3];   //Se multiplican los valores por unos ya preestablecidos de Rijndael
        state[i+1] = M9[a0]^M14[a1]^M11[a2]^M13[a3];
        state[i+2] = M13[a0]^M9[a1]^M14[a2]^M11[a3];
        state[i+3] = M11[a0]^M13[a1]^M9[a2]^M14[a3];
    }
}

void Descifrado(unsigned char Buffer[16], unsigned char Llave[176])
{
   unsigned char i,r;
   unsigned char state[16];
   for(i=0;i<16;i++)
   {
     state[i]=Buffer[i];
   }
   //Add Round Key (160 a 175)
   for (i=0;i<16;i++)
   {
     state[i]=state[i]^Llave[160+i];     
   }    
   for(r=9;r>0;r--)
   {
     //Inverse Shift Rows
     InvShiftRows(state);
     //Inverse Sub Bytes
     InvSubBytes(state);
     //Add Round key
     for (i=0;i<16;i++)
     {
        state[i]=state[i]^Llave[(r)*(16)+i];
     }
     //Inverse Mix Columns
     InvMixColumns(state);
   }
   //Inverse Shift Rows
   InvShiftRows(state);
   //Inverse Sub Bytes
   InvSubBytes(state);
   //Add Round Key (0 15)
   for (i=0;i<16;i++)
   {
      state[i]=state[i]^Llave[i];
   }

   for (i=0;i<16;i++)
   {
      Buffer[i]=state[i];
   } 
}

void main()
{
    unsigned int  br;
         
    /* FAT function result */
    FRESULT res;
    
    /* will hold the information for logical drive 0: */
    FATFS drive;
    FIL archivo1, archivo2; // file objects
    
    CLKPR=0x80;
    CLKPR=1;    //Micro Trabajará a 8MHz
        
    NombreArchivo[0]='0';
    NombreArchivo[1]=':';
    
    // USART1 initialization
    // Communication Parameters: 8 Data, 1 Stop, No Parity
    // USART1 Receiver: On
    // USART1 Transmitter: On
    // USART1 Mode: Asynchronous
    // USART1 Baud Rate: 9600
    UCSR1A=(0<<RXC1) | (0<<TXC1) | (0<<UDRE1) | (0<<FE1) | (0<<DOR1) | (0<<UPE1) | (0<<U2X1) | (0<<MPCM1);
    UCSR1B=(0<<RXCIE1) | (0<<TXCIE1) | (0<<UDRIE1) | (1<<RXEN1) | (1<<TXEN1) | (0<<UCSZ12) | (0<<RXB81) | (0<<TXB81);
    UCSR1C=(0<<UMSEL11) | (0<<UMSEL10) | (0<<UPM11) | (0<<UPM10) | (0<<USBS1) | (1<<UCSZ11) | (1<<UCSZ10) | (0<<UCPOL1);
    UBRR1H=0x00;
    UBRR1L=0x33;
                 
    // Timer/Counter 2 initialization
    // Clock source: System Clock
    // Clock value: 250.000 kHz
    // Mode: CTC top=OCR2A
    // OC2A output: Disconnected
    // OC2B output: Disconnected
    TCCR2A=0x02;
    TCCR2B=0x03;       //Interrupción cada 1mseg para conteo de tiempo
    OCR2A=249;     
    TIMSK2=0x02;
    
    // Código para hacer una interrupción periódica cada 10ms que pide SD
    // Timer/Counter 1 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR1A
    // Compare A Match Interrupt: On
    TCCR1B=0x0A;
    OCR1AH=0x27;
    OCR1AL=0x0F;
    TIMSK1=0x02;
    SetupLCD();
    StringLCD("Proyecto AES");
    #asm("sei")
    /* Inicia el puerto SPI para la SD */
    disk_initialize(0);
    delay_ms(200);
    
    /* mount logical drive 0: */
    if ((res=f_mount(0,&drive))==FR_OK){          
      while(1){    
        while((UCSR1A&0x80)!=0){
                                //Limpiar buffer serial (útil para terminal real) 
           opcion=getchar(); 
        };
        printf("MENU CIFRADO AES   1)CIFRAR  2)DESCIFRAR\n\r");
        opcion=getchar();
        if (opcion=='1')
        {   
           br=0;    
           do{              //Lectura de llave
            printf("\n\rDa el nombre del archivo de la llave: ");
            scanf("%s",Cadena);
            i=2;
            for (i=2;i<17;i++)
              NombreArchivo[i]=Cadena[i-2];      //Copiar string con el que antecede con "0:"       
            res = f_open(&archivo1, NombreArchivo, FA_READ);
            if (res==FR_OK){ 
               f_read(&archivo1, Llave, 16,&br);             //Leer 16 bytes de la llave
               }   
            else             
            {
                printf("Error en el archivo de la llave\n\r");
                ImprimeError(res);
            }       
           }while(br!=16);    
           f_close(&archivo1);
           //Hacer aquí la expansión de llave     
           KeyExpansion(Llave);
           error=1;    
           do{              //Lectura de archivo y cifrado
            printf("Da el nombre del archivo a cifrar: ");
            scanf("%s",Cadena);
            i=0;
            while(Cadena[i]!='\0'){
                NombreImp[i]=Cadena[i];  
                i++;
            }
            
            i=2;               
            for (i=2;i<17;i++)
              NombreArchivo[i]=Cadena[i-2];             
            res = f_open(&archivo1, NombreArchivo, FA_READ);
            if (res==FR_OK){       
               printf("Archivo encontrado\n\r");
               i=0;
               while(NombreArchivo[++i]!='.');  //Busca el punto
               NombreArchivo[i+1]='A'; 
               NombreArchivo[i+2]='E';
               NombreArchivo[i+3]='S';
               res = f_open(&archivo2, NombreArchivo, FA_READ | FA_WRITE | FA_CREATE_ALWAYS );
               if (res==FR_OK)
               {      
                    tiempo=mseg;
                    printf("Cifrando...\n\r");
                    EraseLCD();
                    MoveCursor(0,0);
                    StringLCD("C: ");
                    StringLCDVar(NombreImp); 
                    i=archivo1.fsize%16;        //Calcula el número de bytes del último bloque 
                    NumBloques=archivo1.fsize/16;
                    if (i!=0)
                         NumBloques++;
                    Buffer[0]=i;
                    f_write(&archivo2, Buffer, 1,&br); //Escribe el número de bytes del último bloque
                    j=0;
                    while(j!=NumBloques)
                    {
                        tiempo1 = mseg;
                        f_read(&archivo1, Buffer, 16,&br);   //LeeBloque
                        tiempoR += (mseg -tiempo1);
                        
                        tiempo = mseg;
                        Cifrado(Buffer,Llave);
                        tiempoC += (mseg - tiempo);              
                        
                        tiempo2 = mseg;
                        f_write(&archivo2,Buffer, 16,&br);   //LeeBloque
                        tiempoW += (mseg - tiempo2);               
                        
                        j++;
                        
                        if(j%128==0 || j == NumBloques){
                            MoveCursor(0,1);
                            sprintf(BloquesImp,"%lu/%lu ",j,NumBloques);
                            StringLCDVar(BloquesImp);
                        }
                    }
                    f_close(&archivo2);
                    MoveCursor(0,1);
                    StringLCD("Archivo cifrado ");
                    printf("Nombre del archivo de entrada: %s\n\r",Cadena);
                    printf("Nombre del archivo cifrado: %s\n\r",NombreArchivo);
                    printf("Total de datos cifrados: %lu bytes [%lu bits]\n\r", NumBloques*16, NumBloques*16*8); 
                    printf("Archivo crifrado en %lu mseg (%lu bloques)\n\r",tiempoC, NumBloques);
                    printf("Tiempo de lectura: %lu mseg, tiempo de escritura: %lu mseg\n\r", tiempoR, tiempoW);
                    printf("Tiempo total del proceso: %lu mseg\n\r", tiempoR+tiempoC+tiempoW);
                    printf("Tiempo promedio por bloque: %lu mseg\n\r\n\r", (tiempoR+tiempoC+tiempoW) / NumBloques);
                         
                    
                    error=0; 
               }
               else
               {                                           
                 printf("Error al generar archivo de salida %i");
                 ImprimeError(res);              
                 error=1;
               }
            }      
            else         
               printf("Archivo no encontrado\n\r");   
           }while(error==1);    
           f_close(&archivo1);    
              
        }
        
        if (opcion=='2')
        {   
           br=0;    
           do{              //Lectura de llave
            printf("\n\rDa el nombre del archivo de la llave: ");
            scanf("%s",Cadena);
            i=2;
            for (i=2;i<17;i++)
              NombreArchivo[i]=Cadena[i-2];             
            res = f_open(&archivo1, NombreArchivo, FA_READ);
            if (res==FR_OK){  
               f_read(&archivo1, Llave, 16,&br);   
               }   
            else
                printf("Error en el archivo de la llave %i\n\r");
                ImprimeError(res);   
           }while(br!=16);    
           f_close(&archivo1);
           //Hacer aquí la expansión de llave   
           KeyExpansion(Llave);
           error=1;    
           do{              //Lectura de archivo y cifrado
            printf("Da el nombre del archivo a descifrar: ");
            scanf("%s",Cadena);
            i=2;
            for (i=2;i<17;i++)
              NombreArchivo[i]=Cadena[i-2];             
            res = f_open(&archivo1, NombreArchivo, FA_READ);
            if (res==FR_OK){        
               printf("Archivo encontrado\n\r");
               printf("Da el nombre del archivo de salida: "); 
               scanf("%s",Cadena);
               i=2;
               for (i=2;i<17;i++)
                 NombreArchivo[i]=Cadena[i-2];
               res = f_open(&archivo2, NombreArchivo, FA_READ | FA_WRITE | FA_CREATE_ALWAYS );
               if (res==FR_OK)
               {      
                    printf("Descifrando...\n\r");
                    EraseLCD();
                    MoveCursor(0,0);
                    StringLCD("D: ");
                    StringLCDVar(Cadena);   
                    f_read(&archivo1, Buffer, 1,&br);   
                    i=Buffer[0];                        //No de bytes del último bloque
                    if (i==0)
                      i=16;
                    NumBloques=archivo1.fsize/16;
                    j=0;
                    if (NumBloques>1)
                    {
                        while(j!=(NumBloques-1))
                        {   
                            tiempo1 = mseg;
                            f_read(&archivo1, Buffer, 16,&br);   //LeeBloque
                            tiempoR += (mseg -tiempo1);
                            
                            tiempo = mseg;
                            Descifrado(Buffer,Llave);
                            tiempoC += (mseg - tiempo);              
                            
                            tiempo2 = mseg;
                            f_write(&archivo2,Buffer, 16,&br);   //LeeBloque
                            tiempoW += (mseg - tiempo2);
                                
                            j++;
                            
                            if(j%128==0 || j == NumBloques){
                                MoveCursor(0,1);
                                sprintf(BloquesImp,"%lu/%lu ",j,NumBloques);
                                StringLCDVar(BloquesImp);
                            }
                        }
                    }
                    f_read(&archivo1, Buffer, 16,&br);   //LeeBloque  
                    
                    tiempo = mseg;
                    Descifrado(Buffer,Llave);
                    tiempoC += (mseg - tiempo);
                    
                    f_write(&archivo2,Buffer, i,&br);   //LeeBloque                     
                    
                    f_close(&archivo2);
                    
                    MoveCursor(0,1);
                    StringLCD("Archivo descifrado");
                    printf("Nombre del archivo de entrada: %s\n\r",Cadena);
                    printf("Nombre del archivo cifrado: %s\n\r",NombreArchivo);
                    printf("Total de datos descifrados: %lu bytes [%lu bits]\n\r", NumBloques*16, NumBloques*16*8); 
                    printf("Archivo crifrado en %lu mseg (%lu bloques)\n\r",tiempoC, NumBloques);
                    printf("Tiempo de lectura: %lu mseg, tiempo de escritura: %lu mseg\n\r", tiempoR, tiempoW);
                    printf("Tiempo total del proceso: %lu mseg\n\r", tiempoR+tiempoC+tiempoW);
                    printf("Tiempo promedio por bloque: %lu mseg\n\r\n\r", (tiempoR+tiempoC+tiempoW) / NumBloques);
                    
                    error=0; 
               }
               else
               {                                           
                 printf("Error al generar archivo de salida %i");
                 ImprimeError(res);              
                 error=1;
               }
            }      
            else         
               printf("Archivo no encontrado\n\r");   
           }while(error==1);    
           f_close(&archivo1);    
              
        }
            
        
    }  
    }
    else{
         StringLCD("Drive NO Detectado");
         while(1);
    }
    f_mount(0,0); //Cerrar drive de SD
    while(1);
}