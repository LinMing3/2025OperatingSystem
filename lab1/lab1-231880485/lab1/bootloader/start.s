.code16

.global start
start:
   
    movw %cs, %ax        
    movw %ax, %ds        
    movw %ax, %es       
    movw %ax, %ss       

 
    cli                 


    inb $0x92, %al      
    orb $2, %al         
    outb %al, $0x92   


    data32 addr32 lgdt gdtDesc 

 
    movl %cr0, %eax     
    orb $0x1, %al       
    movl %eax, %cr0     


    data32 ljmp $0x08, $start32 
.code32
start32:

    movw $0x10, %ax     
    movw %ax, %ds        
    movw %ax, %es       
    movw %ax, %fs     
    movw %ax, %ss       
    movw $0x18, %ax    
    movw %ax, %gs        

    
    movl $0x8000, %eax   
    movl %eax, %esp      

   
    jmp bootMain         

.p2align 2
gdt:
   
    .word 0x0000, 0x0000 
    .byte 0x00, 0x00, 0x00, 0x00 
   
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x9A, 0xCF, 0x00 
    
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x92, 0xCF, 0x00 
   
    .word 0xFFFF, 0x8000 
    .byte 0x0B, 0x92, 0xCF, 0x00 

gdtDesc:
    .word gdt - gdtDesc - 1 
    .long gdt              