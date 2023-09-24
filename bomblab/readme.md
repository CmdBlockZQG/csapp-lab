# Bomb Lab

## Phase 1

反编译函数`phase_1`：

```
0000000000400ee0 <phase_1>:
  400ee0:	48 83 ec 08          	sub    $0x8,%rsp
  400ee4:	be 00 24 40 00       	mov    $0x402400,%esi
  400ee9:	e8 4a 04 00 00       	call   401338 <strings_not_equal>
  400eee:	85 c0                	test   %eax,%eax
  400ef0:	74 05                	je     400ef7 <phase_1+0x17>
  400ef2:	e8 43 05 00 00       	call   40143a <explode_bomb>
  400ef7:	48 83 c4 08          	add    $0x8,%rsp
  400efb:	c3                   	ret    
```

发现是单纯判断读入和某个字符串是否相等。用gdb读出`0x402400`处的字符串。

```
(gdb) x/s 0x402400
0x402400:       "Border relations with Canada have never been better."
```

得到答案

```
Border relations with Canada have never been better.
```

## Phase 2

先看`read_six_numbers`的作用

```
000000000040145c <read_six_numbers>:
  40145c:	48 83 ec 18          	sub    $0x18,%rsp
  401460:	48 89 f2             	mov    %rsi,%rdx
  401463:	48 8d 4e 04          	lea    0x4(%rsi),%rcx
  401467:	48 8d 46 14          	lea    0x14(%rsi),%rax
  40146b:	48 89 44 24 08       	mov    %rax,0x8(%rsp)
  401470:	48 8d 46 10          	lea    0x10(%rsi),%rax
  401474:	48 89 04 24          	mov    %rax,(%rsp)
  401478:	4c 8d 4e 0c          	lea    0xc(%rsi),%r9
  40147c:	4c 8d 46 08          	lea    0x8(%rsi),%r8
  401480:	be c3 25 40 00       	mov    $0x4025c3,%esi                  // "%d %d %d %d %d %d"
  401485:	b8 00 00 00 00       	mov    $0x0,%eax
  40148a:	e8 61 f7 ff ff       	call   400bf0 <__isoc99_sscanf@plt>
  40148f:	83 f8 05             	cmp    $0x5,%eax                       // if (eax <= 5) explode
  401492:	7f 05                	jg     401499 <read_six_numbers+0x3d>
  401494:	e8 a1 ff ff ff       	call   40143a <explode_bomb>
  401499:	48 83 c4 18          	add    $0x18,%rsp
  40149d:	c3                   	ret    
```

就是从第一个参数指针指向的字符串中读6个整数，放进一个int数组里面。

下面再看`phase_2`函数

```
0000000000400efc <phase_2>:
  400efc:	55                   	push   %rbp
  400efd:	53                   	push   %rbx
  400efe:	48 83 ec 28          	sub    $0x28,%rsp // 栈上分配40字节，设起始地址记为arr
  400f02:	48 89 e6             	mov    %rsp,%rsi // 将栈上新空间的起始地址作为参数2传入read_six_numbers
  400f05:	e8 52 05 00 00       	call   40145c <read_six_numbers> // 从输入字符串读6个整数放在arr里面
  400f0a:	83 3c 24 01          	cmpl   $0x1,(%rsp)
  400f0e:	74 20                	je     400f30 <phase_2+0x34> // if (arr[0] == 1) goto 初始化循环
  400f10:	e8 25 05 00 00       	call   40143a <explode_bomb> // else explode()
  400f15:	eb 19                	jmp    400f30 <phase_2+0x34>
  400f17:	8b 43 fc             	mov    -0x4(%rbx),%eax // 循环开始
  400f1a:	01 c0                	add    %eax,%eax
  400f1c:	39 03                	cmp    %eax,(%rbx)
  400f1e:	74 05                	je     400f25 <phase_2+0x29> // if (2 * arr[i - 1] != arr[i])
  400f20:	e8 15 05 00 00       	call   40143a <explode_bomb> // explode()
  400f25:	48 83 c3 04          	add    $0x4,%rbx
  400f29:	48 39 eb             	cmp    %rbp,%rbx
  400f2c:	75 e9                	jne    400f17 <phase_2+0x1b>
  400f2e:	eb 0c                	jmp    400f3c <phase_2+0x40>
  400f30:	48 8d 5c 24 04       	lea    0x4(%rsp),%rbx // 初始化循环 i = 1
  400f35:	48 8d 6c 24 18       	lea    0x18(%rsp),%rbp // i <= 6
  400f3a:	eb db                	jmp    400f17 <phase_2+0x1b> // goto 循环开始
  400f3c:	48 83 c4 28          	add    $0x28,%rsp
  400f40:	5b                   	pop    %rbx
  400f41:	5d                   	pop    %rbp
  400f42:	c3                   	ret    
```

分析汇编可知是空格隔开的6个整数组成的数列，首项为1公比为2

得到答案

```
1 2 4 8 16 32
```

## Phase 3

用gdb读出switch的跳转表

```
(gdb) x/8xg 0x402470
0x402470:       0x0000000000400f7c      0x0000000000400fb9
0x402480:       0x0000000000400f83      0x0000000000400f8a
0x402490:       0x0000000000400f91      0x0000000000400f98
0x4024a0:       0x0000000000400f9f      0x0000000000400fa6
```

反编译`phase_3`

```
0000000000400f43 <phase_3>:
  400f43:	48 83 ec 18          	sub    $0x18,%rsp
  400f47:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx // param4 rsp+12 记为b
  400f4c:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx // param3 rsp+8  记为a
  400f51:	be cf 25 40 00       	mov    $0x4025cf,%esi // "%d %d"
  400f56:	b8 00 00 00 00       	mov    $0x0,%eax
  400f5b:	e8 90 fc ff ff       	call   400bf0 <__isoc99_sscanf@plt>
  400f60:	83 f8 01             	cmp    $0x1,%eax
  400f63:	7f 05                	jg     400f6a <phase_3+0x27> // if (ret <= 1)
  400f65:	e8 d0 04 00 00       	call   40143a <explode_bomb> // explode()
  400f6a:	83 7c 24 08 07       	cmpl   $0x7,0x8(%rsp)        // if (a > 7)
  400f6f:	77 3c                	ja     400fad <phase_3+0x6a> // explode()
  400f71:	8b 44 24 08          	mov    0x8(%rsp),%eax
  400f75:	ff 24 c5 70 24 40 00 	jmp    *0x402470(,%rax,8)    // switch(a)
  400f7c:	b8 cf 00 00 00       	mov    $0xcf,%eax            // case 0: res = 207
  400f81:	eb 3b                	jmp    400fbe <phase_3+0x7b> // break
  400f83:	b8 c3 02 00 00       	mov    $0x2c3,%eax           // case 2: res = 707
  400f88:	eb 34                	jmp    400fbe <phase_3+0x7b> // break
  400f8a:	b8 00 01 00 00       	mov    $0x100,%eax           // case 3: res = 256
  400f8f:	eb 2d                	jmp    400fbe <phase_3+0x7b> // break
  400f91:	b8 85 01 00 00       	mov    $0x185,%eax           // case 4: res = 389
  400f96:	eb 26                	jmp    400fbe <phase_3+0x7b> // break
  400f98:	b8 ce 00 00 00       	mov    $0xce,%eax            // case 5: res = 206
  400f9d:	eb 1f                	jmp    400fbe <phase_3+0x7b> // break
  400f9f:	b8 aa 02 00 00       	mov    $0x2aa,%eax           // case 6: res = 682
  400fa4:	eb 18                	jmp    400fbe <phase_3+0x7b> // break
  400fa6:	b8 47 01 00 00       	mov    $0x147,%eax           // case 7: res = 327
  400fab:	eb 11                	jmp    400fbe <phase_3+0x7b> // break
  400fad:	e8 88 04 00 00       	call   40143a <explode_bomb> // default: explode()
  400fb2:	b8 00 00 00 00       	mov    $0x0,%eax             // res = 0
  400fb7:	eb 05                	jmp    400fbe <phase_3+0x7b> // break
  400fb9:	b8 37 01 00 00       	mov    $0x137,%eax           // case 1: res = 311
  400fbe:	3b 44 24 0c          	cmp    0xc(%rsp),%eax        // if (res != b) explode()
  400fc2:	74 05                	je     400fc9 <phase_3+0x86>
  400fc4:	e8 71 04 00 00       	call   40143a <explode_bomb>
  400fc9:	48 83 c4 18          	add    $0x18,%rsp
  400fcd:	c3                   	ret    
```

逻辑是依次读入两个数a和b，用`switch`语句判断对应关系，答案很多。直接用第一个分支，得到答案：

```
0 207
```

## Phase 4

```
000000000040100c <phase_4>:
  40100c:	48 83 ec 18          	sub    $0x18,%rsp
  401010:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx // param4 rsp+12 记为b
  401015:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx // param3 rsp+8  记为a
  40101a:	be cf 25 40 00       	mov    $0x4025cf,%esi // "%d %d"
  40101f:	b8 00 00 00 00       	mov    $0x0,%eax
  401024:	e8 c7 fb ff ff       	call   400bf0 <__isoc99_sscanf@plt>
  401029:	83 f8 02             	cmp    $0x2,%eax
  40102c:	75 07                	jne    401035 <phase_4+0x29> // if (ret != 2) explode()
  40102e:	83 7c 24 08 0e       	cmpl   $0xe,0x8(%rsp)
  401033:	76 05                	jbe    40103a <phase_4+0x2e> // if ((unsigned)a > 14) explode()
  401035:	e8 00 04 00 00       	call   40143a <explode_bomb>
  40103a:	ba 0e 00 00 00       	mov    $0xe,%edx      // param3 = 14
  40103f:	be 00 00 00 00       	mov    $0x0,%esi      // param2 = 0
  401044:	8b 7c 24 08          	mov    0x8(%rsp),%edi // param1 = a
  401048:	e8 81 ff ff ff       	call   400fce <func4> // func4(a, 0, 14)
  40104d:	85 c0                	test   %eax,%eax
  40104f:	75 07                	jne    401058 <phase_4+0x4c> // if (ret != 0) explode()
  401051:	83 7c 24 0c 00       	cmpl   $0x0,0xc(%rsp)
  401056:	74 05                	je     40105d <phase_4+0x51> // if (b != 0) explode() 
  401058:	e8 dd 03 00 00       	call   40143a <explode_bomb>
  40105d:	48 83 c4 18          	add    $0x18,%rsp
  401061:	c3                   	ret    
```

读入两个整数，其中b必须为0，a必须使得`func(a, 0, 14)`返回值为0

```
0000000000400fce <func4>: // func4(a:%edi, b:%esi, c:%edx) d:%ecx
  400fce:	48 83 ec 08          	sub    $0x8,%rsp
  400fd2:	89 d0                	mov    %edx,%eax
  400fd4:	29 f0                	sub    %esi,%eax // res = c - b
  400fd6:	89 c1                	mov    %eax,%ecx
  400fd8:	c1 e9 1f             	shr    $0x1f,%ecx // d = (unsigned)(c - b) >> 31
  400fdb:	01 c8                	add    %ecx,%eax
  400fdd:	d1 f8                	sar    %eax       // res = (res + d) >> 1
  400fdf:	8d 0c 30             	lea    (%rax,%rsi,1),%ecx // d = res + b
  400fe2:	39 f9                	cmp    %edi,%ecx
  400fe4:	7e 0c                	jle    400ff2 <func4+0x24> // if (res + b <= a) goto L1
  400fe6:	8d 51 ff             	lea    -0x1(%rcx),%edx // c = d - 1
  400fe9:	e8 e0 ff ff ff       	call   400fce <func4>
  400fee:	01 c0                	add    %eax,%eax // res = 2 * func4(a, b, c)
  400ff0:	eb 15                	jmp    401007 <func4+0x39> // return res
  400ff2:	b8 00 00 00 00       	mov    $0x0,%eax // L1: res = 0
  400ff7:	39 f9                	cmp    %edi,%ecx
  400ff9:	7d 0c                	jge    401007 <func4+0x39> // if (d >= a) return res
  400ffb:	8d 71 01             	lea    0x1(%rcx),%esi // b = d + 1
  400ffe:	e8 cb ff ff ff       	call   400fce <func4> // res = func4(a, b, c)
  401003:	8d 44 00 01          	lea    0x1(%rax,%rax,1),%eax // return 1 + 2 * res
  401007:	48 83 c4 08          	add    $0x8,%rsp
  40100b:	c3                   	ret    
```

写成等价的C代码：

```c
int func4(int a, int b, int c) {
    int t = c - b;
    if (c < b) ++t;
    t >>= 1;
    int d = t + b;
    if (d > a) {
        return 2 * func4(a, b, d - 1);
    } else if (d == a) {
        return 0;
    } else {
        return 1 + 2 * func4(a, d + 1, c);
    }
}
```

当b=0，c=14时，使得返回值为0最简单的方法是a=7。得到答案：

```
7 0
```

## Phase 5

直接反编译

```
0000000000401062 <phase_5>: // %rdi:input
  401062:	53                   	push   %rbx
  401063:	48 83 ec 20          	sub    $0x20,%rsp    // 栈上分配32字节
  401067:	48 89 fb             	mov    %rdi,%rbx     // %rbx = input
  40106a:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax   // %rax = stack protector
  401071:	00 00 
  401073:	48 89 44 24 18       	mov    %rax,0x18(%rsp) // set stack protector at %rsp+24
  401078:	31 c0                	xor    %eax,%eax
  40107a:	e8 9c 02 00 00       	call   40131b <string_length> // len = strlen(input)
  40107f:	83 f8 06             	cmp    $0x6,%eax
  401082:	74 4e                	je     4010d2 <phase_5+0x70> // if (len == 6) goto L1
  401084:	e8 b1 03 00 00       	call   40143a <explode_bomb> // else explode()
  401089:	eb 47                	jmp    4010d2 <phase_5+0x70>
  40108b:	0f b6 0c 03          	movzbl (%rbx,%rax,1),%ecx    // L2: t = input[i]
  40108f:	88 0c 24             	mov    %cl,(%rsp)
  401092:	48 8b 14 24          	mov    (%rsp),%rdx
  401096:	83 e2 0f             	and    $0xf,%edx             // t &= 0xf (0b1111)
  401099:	0f b6 92 b0 24 40 00 	movzbl 0x4024b0(%rdx),%edx   // t = *(0x4024b0+t)
  4010a0:	88 54 04 10          	mov    %dl,0x10(%rsp,%rax,1) // %rsp+16+i = t
  4010a4:	48 83 c0 01          	add    $0x1,%rax             // ++i
  4010a8:	48 83 f8 06          	cmp    $0x6,%rax
  4010ac:	75 dd                	jne    40108b <phase_5+0x29> // if (i != 6) goto L2
  4010ae:	c6 44 24 16 00       	movb   $0x0,0x16(%rsp)       // %rsp+22 = 0 在字符串结尾加上'\0'
  4010b3:	be 5e 24 40 00       	mov    $0x40245e,%esi        // param2 = 0x40245e "flyers"
  4010b8:	48 8d 7c 24 10       	lea    0x10(%rsp),%rdi       // param1 = rsp+16
  4010bd:	e8 76 02 00 00       	call   401338 <strings_not_equal>
  4010c2:	85 c0                	test   %eax,%eax
  4010c4:	74 13                	je     4010d9 <phase_5+0x77> // if (equal) return
  4010c6:	e8 6f 03 00 00       	call   40143a <explode_bomb> // else explode()
  4010cb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4010d0:	eb 07                	jmp    4010d9 <phase_5+0x77>
  4010d2:	b8 00 00 00 00       	mov    $0x0,%eax             // L1: i = 0
  4010d7:	eb b2                	jmp    40108b <phase_5+0x29> // goto L2
  4010d9:	48 8b 44 24 18       	mov    0x18(%rsp),%rax       // verify stack protector
  4010de:	64 48 33 04 25 28 00 	xor    %fs:0x28,%rax
  4010e5:	00 00 
  4010e7:	74 05                	je     4010ee <phase_5+0x8c>
  4010e9:	e8 42 fa ff ff       	call   400b30 <__stack_chk_fail@plt>
  4010ee:	48 83 c4 20          	add    $0x20,%rsp
  4010f2:	5b                   	pop    %rbx
  4010f3:	c3                   	ret    
```

程序的含义是对输入的6字符字符串进行一个映射，然后将结果和一个字符串比较，相同就通过。

映射规则是取将字符的ascii码的后四位取出来作为偏移量(401096)，然后去查一个位于`0x4024b0`的映射表(401099)，然后与位于`0x40245e`的字符串进行比较。

用gdb把映射表和被比较的字符串拉出来：

```
(gdb) x/16cb 0x4024b0
0x4024b0 <array.3449>:    109 'm' 97 'a'  100 'd' 117 'u' 105 'i' 101 'e' 114 'r' 115 's'
0x4024b8 <array.3449+8>:  110 'n' 102 'f' 111 'o' 116 't' 118 'v' 98 'b'  121 'y' 108 'l'
(gdb) x/s 0x40245e
0x40245e:       "flyers"
```

反向映射一下：

| 原字符 | 偏移 | 构造成大写字母 | 构造成小写字母 |
| ------ | ---- | -------------- | -------------- |
| f      | 0x9  | 0x49 I         | 0x69 I         |
| l      | 0xF  | 0x4F O         | 0x6F O         |
| y      | 0xE  | 0x4E N         | 0x6E N         |
| e      | 0x5  | 0x45 E         | 0x65 E         |
| r      | 0x6  | 0x46 F         | 0x66 F         |
| s      | 0x7  | 0x47 G         | 0x67 G         |

得到答案：

```
IONEFG
// OR
ionefg
```

## Phase 6

函数phase_6很长，于是分段来看。

### 准备阶段和返回阶段

```
00000000004010f4 <phase_6>: // %rdi:input
  4010f4:	41 56                	push   %r14
  4010f6:	41 55                	push   %r13
  4010f8:	41 54                	push   %r12
  4010fa:	55                   	push   %rbp
  4010fb:	53                   	push   %rbx
  4010fc:	48 83 ec 50          	sub    $0x50,%rsp // 栈上分配80字节
  ...
  4011f7:	48 83 c4 50          	add    $0x50,%rsp
  4011fb:	5b                   	pop    %rbx
  4011fc:	5d                   	pop    %rbp
  4011fd:	41 5c                	pop    %r12
  4011ff:	41 5d                	pop    %r13
  401201:	41 5e                	pop    %r14
  401203:	c3                   	ret    
```

根据后面代码，约定`arr = %rsp+0`为一个有6个元素的int类型数组，`arrp = %rsp+32`为一个有6个元素的指针类型数组。

### Part 1

```
  // %rdi: input
  401100:	49 89 e5             	mov    %rsp,%r13 // pt = arr
  401103:	48 89 e6             	mov    %rsp,%rsi // param2 = arr
  401106:	e8 51 03 00 00       	call   40145c <read_six_numbers> // arr[6] <- six int
  40110b:	49 89 e6             	mov    %rsp,%r14
  40110e:	41 bc 00 00 00 00    	mov    $0x0,%r12d
  401114:	4c 89 ed             	mov    %r13,%rbp  // L3:
  401117:	41 8b 45 00          	mov    0x0(%r13),%eax // i = 0
  40111b:	83 e8 01             	sub    $0x1,%eax
  40111e:	83 f8 05             	cmp    $0x5,%eax  
  401121:	76 05                	jbe    401128 <phase_6+0x34> // if (*pt > 6) explode()
  401123:	e8 12 03 00 00       	call   40143a <explode_bomb>
  401128:	41 83 c4 01          	add    $0x1,%r12d // ++i
  40112c:	41 83 fc 06          	cmp    $0x6,%r12d
  401130:	74 21                	je     401153 <phase_6+0x5f> // if (i == 6) break
  401132:	44 89 e3             	mov    %r12d,%ebx // j = i
  401135:	48 63 c3             	movslq %ebx,%rax  // L2
  401138:	8b 04 84             	mov    (%rsp,%rax,4),%eax
  40113b:	39 45 00             	cmp    %eax,0x0(%rbp)
  40113e:	75 05                	jne    401145 <phase_6+0x51> // if (arr[j] == *pt) explode()
  401140:	e8 f5 02 00 00       	call   40143a <explode_bomb>
  401145:	83 c3 01             	add    $0x1,%ebx // j += 1
  401148:	83 fb 05             	cmp    $0x5,%ebx
  40114b:	7e e8                	jle    401135 <phase_6+0x41> if (j <= 5) goto L2
  40114d:	49 83 c5 04          	add    $0x4,%r13 // ++pt
  401151:	eb c1                	jmp    401114 <phase_6+0x20> // goto L3
```

这段看到两个大跳转点就能知道是二重循环了。分析这一段汇编，可写出等效的C代码如下：

```c
read_six_numbers(input, arr);
i = 0;
for (pt = arr; ; ++pt) {
    if (*pt > 6) explode_bomb();
    ++i;
    if (i == 6) break;
    for (j = i; j <= 5; ++j) {
        if (arr[j] == *pt) explode_bomb();
    }
}
```

意思是测试这六个数是不是满足：(1)全都小于等于6；(2)两两不同。

### Part 2

```
  401153:	48 8d 74 24 18       	lea    0x18(%rsp),%rsi // %rsi = %rsp+24
  401158:	4c 89 f0             	mov    %r14,%rax // %rax = arr
  40115b:	b9 07 00 00 00       	mov    $0x7,%ecx // %ecx = 7
  401160:	89 ca                	mov    %ecx,%edx // L1: %edx = %ecx
  401162:	2b 10                	sub    (%rax),%edx // %edx -= *(%rax)
  401164:	89 10                	mov    %edx,(%rax) // *(%rax) = %edx
  401166:	48 83 c0 04          	add    $0x4,%rax // %rax += 4
  40116a:	48 39 f0             	cmp    %rsi,%rax // if (%rax != %rsi) goto L1
  40116d:	75 f1                	jne    401160 <phase_6+0x6c>
```

这段很短也很容易能看懂。就是把每个数`x`都替换为`7-x`。等效C代码如下：

```
for (pt = arr; pt != arr + 6; ++pt) {
    *pt = 7 - *pt;
}
```

### Part 3

```
  40116f:	be 00 00 00 00       	mov    $0x0,%esi // %rsi = 0
  401174:	eb 21                	jmp    401197 <phase_6+0xa3> // goto L1
  
  401176:	48 8b 52 08          	mov    0x8(%rdx),%rdx // L3: %rdx = *(%rdx+8)
  40117a:	83 c0 01             	add    $0x1,%eax // ++%eax
  40117d:	39 c8                	cmp    %ecx,%eax // if (%eax != ecx) goto L3 else goto L4
  40117f:	75 f5                	jne    401176 <phase_6+0x82>
  401181:	eb 05                	jmp    401188 <phase_6+0x94>

  401183:	ba d0 32 60 00       	mov    $0x6032d0,%edx // L2: %edx = 0x6032d0
  401188:	48 89 54 74 20       	mov    %rdx,0x20(%rsp,%rsi,2) // L4: *(rsp+32 + 2 * %rsi) = 
  40118d:	48 83 c6 04          	add    $0x4,%rsi // %rsi += 4
  401191:	48 83 fe 18          	cmp    $0x18,%rsi // if (%rsi == 24) break;
  401195:	74 14                	je     4011ab <phase_6+0xb7>
  401197:	8b 0c 34             	mov    (%rsp,%rsi,1),%ecx // L1: %ecx = *(arr + %rsi)
  40119a:	83 f9 01             	cmp    $0x1,%ecx // if (%ecx <= 1) goto L2
  40119d:	7e e4                	jle    401183 <phase_6+0x8f>
  40119f:	b8 01 00 00 00       	mov    $0x1,%eax // %eax = 1
  4011a4:	ba d0 32 60 00       	mov    $0x6032d0,%edx // %edx = 0x6032d0
  4011a9:	eb cb                	jmp    401176 <phase_6+0x82> goto L3
```

结构还是挺绕弯的，但是跟着走一走还是能看出来是什么结构。等效C代码如下：

```c
for (i = 0; i < 6; ++i) {
    p = 0x6032d0;
    for (j = 1; j < arr[i]; ++j) {
        p = *(p + 8);
    }
    arrp[i] = p;
}
```

这里的p其实是伪代码，表示一个64位整数，可以当作指针使用。

可以看到，这段代码是根据arr里面的6个整数值得到了6个指针值。从`0x6032d0`开始，迭代`arr[i] - 1`次，每次变成所指内存区域+8位置的指针指向的内存空间的数值。从gdb里面把这一串酷似链表的玩意拉出来。

```
(gdb) x/1xg 0x6032d8
0x6032d8 <node1+8>:     0x00000000006032e0
(gdb) x/1xg 0x6032e8
0x6032e8 <node2+8>:     0x00000000006032f0
(gdb) x/1xg 0x6032f8
0x6032f8 <node3+8>:     0x0000000000603300
(gdb) x/1xg 0x603308
0x603308 <node4+8>:     0x0000000000603310
(gdb) x/1xg 0x603318
0x603318 <node5+8>:     0x0000000000603320
(gdb) x/1xg 0x603328
0x603328 <node6+8>:     0x0000000000000000

(gdb) x/1dw 0x6032d0
0x6032d0 <node1>:       332
(gdb) x/1dw 0x6032e0
0x6032e0 <node2>:       168
(gdb) x/1dw 0x6032f0
0x6032f0 <node3>:       924
(gdb) x/1dw 0x603300
0x603300 <node4>:       691
(gdb) x/1dw 0x603310
0x603310 <node5>:       477
(gdb) x/1dw 0x603320
0x603320 <node6>:       443
```

得到据此可以推知`arr[i]`和`arrp[i]`的对应关系，以及`arrp[i]`指向的数的值。

| arr[i] | arrp[i]  | *arrp[i] |
| ------ | -------- | -------- |
| 1      | 0x6032d0 | 332      |
| 2      | 0x6032e0 | 168      |
| 3      | 0x6032f0 | 924      |
| 4      | 0x603300 | 691      |
| 5      | 0x603310 | 477      |
| 6      | 0x603320 | 443      |
| 7      | 0x000000 |          |

### Part 4

```
  4011ab:	48 8b 5c 24 20       	mov    0x20(%rsp),%rbx // %rbx = arrp[0]
  4011b0:	48 8d 44 24 28       	lea    0x28(%rsp),%rax // %rax = &arrp[1]
  4011b5:	48 8d 74 24 50       	lea    0x50(%rsp),%rsi // %rsi = &arrp[6]
  4011ba:	48 89 d9             	mov    %rbx,%rcx // %rcx = %rbx
  4011bd:	48 8b 10             	mov    (%rax),%rdx // L1: %rdx = *(%rax)
  4011c0:	48 89 51 08          	mov    %rdx,0x8(%rcx) // *(%rcx + 8) = %rdx
  4011c4:	48 83 c0 08          	add    $0x8,%rax // %rax += 8
  4011c8:	48 39 f0             	cmp    %rsi,%rax // if (%rax == %rsi) break
  4011cb:	74 05                	je     4011d2 <phase_6+0xde>
  4011cd:	48 89 d1             	mov    %rdx,%rcx // %rcx = %rdx
  4011d0:	eb eb                	jmp    4011bd <phase_6+0xc9> goto L1
  
  4011d2:	48 c7 42 08 00 00 00 	movq   $0x0,0x8(%rdx) // arrp[6] = 0
  4011d9:	00 
  4011da:	bd 05 00 00 00       	mov    $0x5,%ebp // %ebp = 5
  4011df:	48 8b 43 08          	mov    0x8(%rbx),%rax // L1: %rax = *(arrp[0] + 8)
  4011e3:	8b 00                	mov    (%rax),%eax // %eax = *(%rax)
  4011e5:	39 03                	cmp    %eax,(%rbx) // if (*arrp[0] < %eax) explode_bomb()
  4011e7:	7d 05                	jge    4011ee <phase_6+0xfa>
  4011e9:	e8 4c 02 00 00       	call   40143a <explode_bomb>
  4011ee:	48 8b 5b 08          	mov    0x8(%rbx),%rbx // %rbx = *(%rbx + 8)
  4011f2:	83 ed 01             	sub    $0x1,%ebp // %ebp -= 1
  4011f5:	75 e8                	jne    4011df <phase_6+0xeb> if (%ebp != 0) goto L1
```

真的想吐槽，这循环编译出来的样子完全就是增加阅读难度。C等效代码如下：

```c
for (int i = 1; i < 6; ++i) {
    *(arrp[i - 1] + 8) = arrp[i];
}

p = arrp[0];
arrp[6] = 0;
for (i = 5; i > 0; --i) {
    if (*p < **(p + 8)) explode_bomb();
    p = *(p + 8);
}
```

arrp和p这里也是伪代码，表示可以当指针用的64位整数。乍一看乱七八糟，但是只要把这两个循环干的事情列出来就清楚了。

第一个循环做了下面的赋值：

```c
*(arrp[0] + 8) = arrp[1]
*(arrp[1] + 8) = arrp[2]
    ...
*(arrp[4] + 8) = arrp[5]
```

注意到第二个循环更新p时，`p = *(p + 8)`，p一开始为`arrp[0]`。而经过第一个循环的赋值，`*(arrp[i] + 8)`等于`arrp[i + 1]`。

于是改写第二个循环如下：

```c
for (i = 0; i < 5; ++i) {
    if (*arrp[i] < *arrp[i + 1]) explode_bomb();
}
```

### 反推答案

Part4中第二个循环要求arrp中存储的指针所指向的值递减。根据从Part3中得到的对应表可以推知答案

| i    | *arrp[i] | arr[i] (7-x) | arr[i] (orig) |
| ---- | -------- | ------------ | ------------- |
| 0    | 924      | 3            | 4             |
| 1    | 691      | 4            | 3             |
| 2    | 477      | 5            | 2             |
| 3    | 443      | 6            | 1             |
| 4    | 332      | 1            | 6             |
| 5    | 168      | 2            | 5             |

得到答案：

```
4 3 2 1 6 5
```

## Secret Phase

### 入口

观察`phase_defused`函数

```
00000000004015c4 <phase_defused>:
  4015c4:	48 83 ec 78          	sub    $0x78,%rsp
  4015c8:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
  4015cf:	00 00 
  4015d1:	48 89 44 24 68       	mov    %rax,0x68(%rsp)
  4015d6:	31 c0                	xor    %eax,%eax
  4015d8:	83 3d 81 21 20 00 06 	cmpl   $0x6,0x202181(%rip)        # 603760 <num_input_strings>
  4015df:	75 5e                	jne    40163f <phase_defused+0x7b>
  4015e1:	4c 8d 44 24 10       	lea    0x10(%rsp),%r8 // param5 = %rsp+16
  4015e6:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx // param4 = %rsp+12
  4015eb:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx // param3 = %rsp+8
  4015f0:	be 19 26 40 00       	mov    $0x402619,%esi // param2 = "%d %d %s"
  4015f5:	bf 70 38 60 00       	mov    $0x603870,%edi // param1 = 0x603870
  4015fa:	e8 f1 f5 ff ff       	call   400bf0 <__isoc99_sscanf@plt>
  4015ff:	83 f8 03             	cmp    $0x3,%eax
  401602:	75 31                	jne    401635 <phase_defused+0x71> // if (ret != 3) GG
  401604:	be 22 26 40 00       	mov    $0x402622,%esi // param2 = "DrEvil"
  401609:	48 8d 7c 24 10       	lea    0x10(%rsp),%rdi // param1 = %rsp+16
  40160e:	e8 25 fd ff ff       	call   401338 <strings_not_equal>
  401613:	85 c0                	test   %eax,%eax
  401615:	75 1e                	jne    401635 <phase_defused+0x71> // if (strings_not_equal) GG
  401617:	bf f8 24 40 00       	mov    $0x4024f8,%edi
  40161c:	e8 ef f4 ff ff       	call   400b10 <puts@plt>
  401621:	bf 20 25 40 00       	mov    $0x402520,%edi
  401626:	e8 e5 f4 ff ff       	call   400b10 <puts@plt>
  40162b:	b8 00 00 00 00       	mov    $0x0,%eax
  401630:	e8 0d fc ff ff       	call   401242 <secret_phase>
  401635:	bf 58 25 40 00       	mov    $0x402558,%edi
  40163a:	e8 d1 f4 ff ff       	call   400b10 <puts@plt>
  40163f:	48 8b 44 24 68       	mov    0x68(%rsp),%rax
  401644:	64 48 33 04 25 28 00 	xor    %fs:0x28,%rax
  40164b:	00 00 
  40164d:	74 05                	je     401654 <phase_defused+0x90>
  40164f:	e8 dc f4 ff ff       	call   400b30 <__stack_chk_fail@plt>
  401654:	48 83 c4 78          	add    $0x78,%rsp
  401658:	c3                   	ret    
  401659:	90                   	nop
  40165a:	90                   	nop
  40165b:	90                   	nop
  40165c:	90                   	nop
  40165d:	90                   	nop
  40165e:	90                   	nop
  40165f:	90                   	nop
```

在拆除每个阶段时，这个函数都会判断`num_input_strings`全局变量的值是否为6。若为6则进行下面的判断。根据变量名以及gdb中运行程序观察可知，里面存的是到目前为止输入的字符串数量。也就是说在所有的阶段都被拆除之后，会进行下面的判断。

```
4015e1:	4c 8d 44 24 10       	lea    0x10(%rsp),%r8 // param5 = %rsp+16
  4015e6:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx // param4 = %rsp+12
  4015eb:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx // param3 = %rsp+8
  4015f0:	be 19 26 40 00       	mov    $0x402619,%esi // param2 = "%d %d %s"
  4015f5:	bf 70 38 60 00       	mov    $0x603870,%edi // param1 = 0x603870
  4015fa:	e8 f1 f5 ff ff       	call   400bf0 <__isoc99_sscanf@plt>
  4015ff:	83 f8 03             	cmp    $0x3,%eax
  401602:	75 31                	jne    401635 <phase_defused+0x71> // if (ret != 3) GG
  401604:	be 22 26 40 00       	mov    $0x402622,%esi // param2 = "DrEvil"
  401609:	48 8d 7c 24 10       	lea    0x10(%rsp),%rdi // param1 = %rsp+16
  40160e:	e8 25 fd ff ff       	call   401338 <strings_not_equal>
  401613:	85 c0                	test   %eax,%eax
  401615:	75 1e                	jne    401635 <phase_defused+0x71> // if (strings_not_equal) GG
  401617:	bf f8 24 40 00       	mov    $0x4024f8,%edi
  40161c:	e8 ef f4 ff ff       	call   400b10 <puts@plt>
  401621:	bf 20 25 40 00       	mov    $0x402520,%edi
  401626:	e8 e5 f4 ff ff       	call   400b10 <puts@plt>
  40162b:	b8 00 00 00 00       	mov    $0x0,%eax
  401630:	e8 0d fc ff ff       	call   401242 <secret_phase>
```

这里会进行一个`sscanf`操作。其中第一个参数是个绝对地址，目测是全局变量的区域。用dgb查看：

```
(gdb) x/s 0x603870
0x603870 <input_strings+240>:   "7 0"
```

这就是在第四阶段输入的字符串。sscanf会读入两个整数之后的那个字符串，并判断是不是等于`"DrEvil"`，若是则调用函数`secret_phase`。

所以在第三阶段时输入：

```
7 0 DrEvil
```

即可在通过第六阶段之后触发秘密阶段。

### 秘密阶段

首先看`secret_phase`函数

```
0000000000401242 <secret_phase>:
  401242:	53                   	push   %rbx
  401243:	e8 56 02 00 00       	call   40149e <read_line>
  401248:	ba 0a 00 00 00       	mov    $0xa,%edx // param3 = 10
  40124d:	be 00 00 00 00       	mov    $0x0,%esi // param2 = 0
  401252:	48 89 c7             	mov    %rax,%rdi // param1 = input
  401255:	e8 76 f9 ff ff       	call   400bd0 <strtol@plt> // %rax = t (将输入转成10进制数)
  40125a:	48 89 c3             	mov    %rax,%rbx // %rbx = t
  40125d:	8d 40 ff             	lea    -0x1(%rax),%eax // %eax = t - 1
  401260:	3d e8 03 00 00       	cmp    $0x3e8,%eax // if (%eax > 1000) explode()
  401265:	76 05                	jbe    40126c <secret_phase+0x2a>
  401267:	e8 ce 01 00 00       	call   40143a <explode_bomb>
  40126c:	89 de                	mov    %ebx,%esi // param2 = %ebx = t
  40126e:	bf f0 30 60 00       	mov    $0x6030f0,%edi // param1 = 0x6030f0
  401273:	e8 8c ff ff ff       	call   401204 <fun7>
  401278:	83 f8 02             	cmp    $0x2,%eax // if (%eax != 2) explode() 
  40127b:	74 05                	je     401282 <secret_phase+0x40>
  40127d:	e8 b8 01 00 00       	call   40143a <explode_bomb>
  401282:	bf 38 24 40 00       	mov    $0x402438,%edi // 输出成功信息
  401287:	e8 84 f8 ff ff       	call   400b10 <puts@plt>
  40128c:	e8 33 03 00 00       	call   4015c4 <phase_defused>
  401291:	5b                   	pop    %rbx
  401292:	c3                   	ret    
```

这个函数含义很简单，就是把输入的字符串转换成10进制，判断是否小于等于1001，然后调用`func7(0x6030f0, t)`，t为输入的整数。判断返回值是否为2，若是则成功。

下面看`func7`：

```
0000000000401204 <fun7>: // %rdi:p  %esi:x
  401204:	48 83 ec 08          	sub    $0x8,%rsp // 栈上分配8字节
  401208:	48 85 ff             	test   %rdi,%rdi 
  40120b:	74 2b                	je     401238 <fun7+0x34> // if (p == NULL) goto L1
  40120d:	8b 17                	mov    (%rdi),%edx // %edx = *(%rdi)
  40120f:	39 f2                	cmp    %esi,%edx // if (%edx <= x) goto L2
  401211:	7e 0d                	jle    401220 <fun7+0x1c>
  401213:	48 8b 7f 08          	mov    0x8(%rdi),%rdi // %rdi = *(%rdi + 8)
  401217:	e8 e8 ff ff ff       	call   401204 <fun7> // 递归
  40121c:	01 c0                	add    %eax,%eax // res = 2 * res
  40121e:	eb 1d                	jmp    40123d <fun7+0x39> // return
  401220:	b8 00 00 00 00       	mov    $0x0,%eax // L2: res = 0
  401225:	39 f2                	cmp    %esi,%edx // if (edx == x) return
  401227:	74 14                	je     40123d <fun7+0x39>
  401229:	48 8b 7f 10          	mov    0x10(%rdi),%rdi // p = *(p + 16)
  40122d:	e8 d2 ff ff ff       	call   401204 <fun7> // 递归
  401232:	8d 44 00 01          	lea    0x1(%rax,%rax,1),%eax // res = 2 * res + 1
  401236:	eb 05                	jmp    40123d <fun7+0x39> // return
  401238:	b8 ff ff ff ff       	mov    $0xffffffff,%eax // L1: res = -1
  40123d:	48 83 c4 08          	add    $0x8,%rsp
  401241:	c3                   	ret    
```

总体还是很简单明了的。注意到是递归，于是翻译成等效C代码：

```c
// %rdi:p  %esi:x
int fun7(pointer p, int x) {
    if (p == NULL) return -1;
    int y = *p;
    if (x < y) {
        return 2 * fun7(*(p + 8), x);
    } else if (x == y) {
        return 0;
    } else if (x > y) {
        return 1 + 2 * fun7(*(p + 16), x);
    }
}
```

这里的p是伪代码，表示可以当指针使用的64位整数。

程序的行为和传入的p指针有关，于是用gdb把那`0x6030f0`后面一片内存区域的东西都拉出来看看。

```
0x6030f0 <n1>:          0x0000000000000024      0x0000000000603110
0x603100 <n1+16>:       0x0000000000603130      0x0000000000000000
0x603110 <n21>:         0x0000000000000008      0x0000000000603190
0x603120 <n21+16>:      0x0000000000603150      0x0000000000000000
0x603130 <n22>:         0x0000000000000032      0x0000000000603170
0x603140 <n22+16>:      0x00000000006031b0      0x0000000000000000
0x603150 <n32>:         0x0000000000000016      0x0000000000603270
0x603160 <n32+16>:      0x0000000000603230      0x0000000000000000
0x603170 <n33>:         0x000000000000002d      0x00000000006031d0
0x603180 <n33+16>:      0x0000000000603290      0x0000000000000000
0x603190 <n31>:         0x0000000000000006      0x00000000006031f0
0x6031a0 <n31+16>:      0x0000000000603250      0x0000000000000000
0x6031b0 <n34>:         0x000000000000006b      0x0000000000603210
0x6031c0 <n34+16>:      0x00000000006032b0      0x0000000000000000
0x6031d0 <n45>:         0x0000000000000028      0x0000000000000000
0x6031e0 <n45+16>:      0x0000000000000000      0x0000000000000000
0x6031f0 <n41>:         0x0000000000000001      0x0000000000000000
0x603200 <n41+16>:      0x0000000000000000      0x0000000000000000
0x603210 <n47>:         0x0000000000000063      0x0000000000000000
0x603220 <n47+16>:      0x0000000000000000      0x0000000000000000
0x603230 <n44>:         0x0000000000000023      0x0000000000000000
0x603240 <n44+16>:      0x0000000000000000      0x0000000000000000
0x603250 <n42>:         0x0000000000000007      0x0000000000000000
0x603260 <n42+16>:      0x0000000000000000      0x0000000000000000
0x603270 <n43>:         0x0000000000000014      0x0000000000000000
0x603280 <n43+16>:      0x0000000000000000      0x0000000000000000
0x603290 <n46>:         0x000000000000002f      0x0000000000000000
0x6032a0 <n46+16>:      0x0000000000000000      0x0000000000000000
0x6032b0 <n48>:         0x00000000000003e9      0x0000000000000000
0x6032c0 <n48+16>:      0x0000000000000000      0x0000000000000000
```

可以看到，存储的结构是一个数后面跟两个指针，指向其他的数。整理如下表（`0x000000`留空）：

| p        | *p   | *(p + 8) | *(p + 16) |
| -------- | ---- | -------- | --------- |
| 0x6030f0 | 36   | 0x603110 | 0x603130  |
| 0x603110 | 8    | 0x603190 | 0x603150  |
| 0x603130 | 50   | 0x603170 | 0x6031b0  |
| 0x603150 | 22   | 0x603270 | 0x603230  |
| 0x603170 | 45   | 0x6031d0 | 0x603290  |
| 0x603190 | 6    | 0x6031f0 | 0x603250  |
| 0x6031b0 | 107  | 0x603210 | 0x6032b0  |
| 0x6031d0 | 40   |          |           |
| 0x6031f0 | 1    |          |           |
| 0x603210 | 99   |          |           |
| 0x603230 | 35   |          |           |
| 0x603250 | 7    |          |           |
| 0x603270 | 20   |          |           |
| 0x603290 | 47   |          |           |
| 0x6032b0 | 1001 |          |           |

草稿纸上画一画就能发现，这其实是一颗二叉树，而且是满二叉树，而且是二叉平衡树！把`*(p + 8)`当作左儿子指针，`*(p + 16)`当作右儿子指针，根据func7的逻辑编写求解程序：

```c
#include <stdio.h>
// 用满二叉树的表示方法表示这棵树
const int tree[16] = {0, 36, 8, 50, 6, 22, 45, 107, 1, 7, 20, 35, 40, 47, 99, 1001};

// 原fun7
int fun7(void *p, int x) {
    if (p == NULL) return -1;
    int y = *(int*)p;
    if (x < y) {
        return 2 * fun7(*(void**)(p + 8), x);
    } else if (x == y) {
        return 0;
    } else if (x > y) {
        return 1 + 2 * fun7(*(void**)(p + 16), x);
    }
}

// 相同逻辑的新fun7，p为模拟指针
int fun7_new(int p, int x) {
    if (p >= 16) return -1; // 对应空指针
    int y = tree[p];
    if (x < y) {
        return 2 * fun7_new(p * 2, x);
    } else if (x == y) {
        return 0;
    } else if (x > y) {
        return 1 + 2 * fun7_new(p * 2 + 1, x);
    }
}

int main() {
    int i;
    // 输入的数一定要在树上出现，不然返回值出现-1就不大可能得到返回值2了
    for (i = 1; i < 16; ++i) {
        if (fun7_new(1, tree[i]) == 2) printf("%d ", tree[i]);
    }
    return 0;
}
```

求解程序输出：

```
22 20
```

得到答案：

```
22
// OR
20
```

## 最终参考答案

answer.txt

```
Border relations with Canada have never been better.
1 2 4 8 16 32
0 207
7 0 DrEvil
ionefg
4 3 2 1 6 5
20

```

控制台：

```
# ./bomb answer.txt
Welcome to my fiendish little bomb. You have 6 phases with
which to blow yourself up. Have a nice day!
Phase 1 defused. How about the next one?
That's number 2.  Keep going!
Halfway there!
So you got that one.  Try this one.
Good work!  On to the next...
Curses, you've found the secret phase!
But finding it and solving it are quite different...
Wow! You've defused the secret stage!
Congratulations! You've defused the bomb!
```

