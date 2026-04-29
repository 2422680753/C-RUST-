/*
 * Linker script for STM32H743VITx
 * Flash: 2MB (0x08000000 - 0x081FFFFF)
 * RAM: 1MB (0x24000000 - 0x240FFFFF)
 * DTCM: 128KB (0x20000000 - 0x2001FFFF)
 * ITCM: 64KB (0x00000000 - 0x0000FFFF)
 * 
 * 支持：
 * - 栈金丝雀（Stack Canary）用于栈完整性验证
 * - DMA 缓冲区区域（32字节对齐）
 * - 无堆内存
 */

ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM */

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x0;      /* No heap required - 不使用堆内存 */
_Min_Stack_Size = 0x1000;  /* 4KB stack */

/* 栈金丝雀大小 */
_Stack_Canary_Size = 0x8;  /* 8 bytes for two 32-bit canaries */

/* Specify the memory areas */
MEMORY
{
  ITCM (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  DTCM (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K
  RAM (xrw)   : ORIGIN = 0x24000000, LENGTH = 512K
  RAM_D2 (xrw): ORIGIN = 0x30000000, LENGTH = 288K
  RAM_D3 (xrw): ORIGIN = 0x38000000, LENGTH = 64K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /*
   * Stack and heap in RAM
   * No heap is used in this project
   * 
   * 栈布局（从低地址到高地址）：
   * - 底部金丝雀 (stack_canary_bottom)
   * - 栈区域 (_sstack 到 _estack)
   * - 顶部金丝雀 (stack_canary_top)
   */
  
  /* 栈底部金丝雀 - 在栈开始之前 */
  .stack_canary_bottom (NOLOAD) :
  {
    . = ALIGN(4);
    PROVIDE ( _stack_canary_bottom = . );
    *(.stack_canary)
    . = ALIGN(4);
  } >RAM

  /*
   * 实际栈区域
   * 注意：栈从高地址向低地址增长
   * 所以 _sstack 是栈的最低地址，_estack 是栈的最高地址
   */
  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( _sstack = . );
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
    PROVIDE ( _estack = . );
  } >RAM

  /* 栈顶部金丝雀 - 在栈结束之后 */
  .stack_canary_top (NOLOAD) :
  {
    . = ALIGN(4);
    PROVIDE ( _stack_canary_top = . );
    *(.stack_canary)
    . = ALIGN(4);
  } >RAM

  /*
   * DMA 缓冲区区域
   * 必须 32 字节对齐以支持 Cache 维护操作
   * 放置在 .data 之前以确保地址稳定性
   */
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    PROVIDE ( _s_dma_buffer = . );
    *(.dma_buffer)
    *(.dma_buffer.*)
    . = ALIGN(32);
    PROVIDE ( _e_dma_buffer = . );
  } >RAM

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* RamFunc sections */
    *(.RamFunc*)       /* RamFunc* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section - 标记为 0 大小，不使用堆 */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    /* 不分配堆内存 */
    . = . + _Min_Heap_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
    /* 丢弃堆相关的段 */
    *(.heap)
    *(.heap*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

/*
 * 验证链接时断言
 * 确保关键约束条件满足
 */

/* 确保栈大小足够 */
ASSERT(_Min_Stack_Size >= 0x400, "Stack size too small! Minimum 1KB required.")

/* 确保不使用堆 */
ASSERT(_Min_Heap_Size == 0, "Heap is not allowed in this project!")

/* 确保 DMA 缓冲区 32 字节对齐 */
ASSERT((_s_dma_buffer & 0x1F) == 0, "DMA buffer must be 32-byte aligned!")

/* 确保金丝雀在正确位置 */
ASSERT(_stack_canary_bottom < _sstack, "Stack canary bottom must be before stack!")
ASSERT(_stack_canary_top > _estack, "Stack canary top must be after stack!")
