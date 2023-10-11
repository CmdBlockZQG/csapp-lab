sim-tty为我真正用来做homework和lab的模拟器，关闭gui。

sim-gui为带gui的模拟器，在`Ubuntu 22.04.1 LTS`下编译，tcl版本8.6。

官网下载的sim.tar中的代码由于使用了一些旧版本gcc和tcl的特性无法在新环境下直接编译，我对源代码和makefile进行了一些修改。

# Homework

## 4.52 SEQ-full

iaddq for SEQ

| Phase  | iaddq V, rB                                                  |
| ------ | ------------------------------------------------------------ |
| F 取指 | icode:ifun <- M1[PC] <br>rA:rB <- M1[PC+1] <br>valC <- M8[PC+2] <br>valP <- PC+10 |
| D 译码 | valB <- R[rB]                                                |
| E 执行 | valE <- valB + valC <br>set CC                               |
| M 访存 |                                                              |
| W 写回 | R[rB] <- valE                                                |
| PC     | PC <- valP                                                   |

```diff
--- seq-full-orig.hcl
+++ seq-full.hcl
@@ -106,16 +106,16 @@
 
 bool instr_valid = icode in 
        { INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
-              IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ };
+              IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IIADDQ };
 
 # Does fetched instruction require a regid byte?
 bool need_regids =
        icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ, 
-                    IIRMOVQ, IRMMOVQ, IMRMOVQ };
+                    IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ };
 
 # Does fetched instruction require a constant word?
 bool need_valC =
-       icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL };
+       icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL, IIADDQ };

 ################ Decode Stage    ###################################

@@ -128,7 +128,7 @@

 ## What register should be used as the B source?
 word srcB = [
-       icode in { IOPQ, IRMMOVQ, IMRMOVQ  } : rB;
+       icode in { IOPQ, IRMMOVQ, IMRMOVQ, IIADDQ  } : rB;
        icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
        1 : RNONE;  # Don't need register
 ];
@@ -136,7 +136,7 @@
 ## What register should be used as the E destination?
 word dstE = [
        icode in { IRRMOVQ } && Cnd : rB;
-       icode in { IIRMOVQ, IOPQ} : rB;
+       icode in { IIRMOVQ, IOPQ, IIADDQ } : rB;
        icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
        1 : RNONE;  # Don't write any register
 ];
@@ -152,7 +152,7 @@
 ## Select input A to ALU
 word aluA = [
        icode in { IRRMOVQ, IOPQ } : valA;
-       icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ } : valC;
+       icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ } : valC;
        icode in { ICALL, IPUSHQ } : -8;
        icode in { IRET, IPOPQ } : 8;
        # Other instructions don't need ALU
@@ -161,7 +161,7 @@
 ## Select input B to ALU
 word aluB = [
        icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL,
-                     IPUSHQ, IRET, IPOPQ } : valB;
+                     IPUSHQ, IRET, IPOPQ, IIADDQ } : valB;
        icode in { IRRMOVQ, IIRMOVQ } : 0;
        # Other instructions don't need ALU
 ];
@@ -173,7 +173,7 @@
 ];

 ## Should the condition codes be updated?
-bool set_cc = icode in { IOPQ };
+bool set_cc = icode in { IOPQ, IIADDQ };

 ################ Memory Stage    ###################################
```

## 4.53 PIPE-stall

also named PIPE-nobypass

always stall when hazard

数据冒险条件：译码阶段(D)需要取读取的寄存器(d_srcA, d_srcB)会在后续步骤中被写入。

```
bool s_data =
	(d_srcA != RNONE && d_srcA in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
	(d_srcB != RNONE && d_srcB in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM });
```

返回暂停条件：译码(D)执行(E)访存(M)阶段中正在运行ret指令。当ret指令进入写回阶段时下一条指令的地址就已经知悉，可以进行译码。

```
bool s_ret = IRET in { D_icode, E_icode, M_icode };
```

预测错误条件：在执行阶段(E)中的是跳转指令且跳转条件不成立。

```
bool s_pred = E_icode == IJXX && !e_Cnd;
```

发生数据冒险时，应将正在译码的指令卡住直到它要读取的寄存器已经被写入完毕（即写入其需要读取的寄存器的指令通过写回阶段），并在执行阶段插入气泡填充流水线。

需要返回暂停时，应卡住取指阶段直到它能够取到ret之后的正确地址（即ret指令通过访存阶段），并在译码阶段插入气泡填充流水线。

发生预测错误时，在执行和译码阶段插入气泡取代错误取出的指令，而后跳转指令进入访存阶段，取指阶段能够获得正确的后续指令地址。

表中S for stall, B for bubble

|                 | F    | D    | E    |
| --------------- | ---- | ---- | ---- |
| s_data          | S    | S    | B    |
| s_ret           | S    | B    |      |
| s_pred          |      | B    | B    |
| s_data && s_ret | S    | S    | B    |
| s_pred && *     |      | B    | B    |

当数据冒险和返回暂停同时发生时，应按照数据冒险处理。若ret比产生数据冒险的指令更早进入流水线，产生数据冒险的指令根本不会被译码。故ret本身一定是产生数据冒险的指令，需要暂停。

当预测错误和其他类型一起发生（包括全部发生）时，应该总是按照预测错误处理。若预测错误后发生，无论是数据冒险还是ret都会使得产生预测错误的JXX指令被卡住无法译码。故预测错误一定先发生，而后续的指令无论如何都是被错误取出的，故应该按照预测错误处理全部取消掉。

于是F,D,E阶段的stall与bubble更新条件改为：

```
bool F_bubble = 0;
bool F_stall = (s_data || s_ret) && !s_pred;

bool D_stall = s_data && !s_pred;
bool D_bubble = s_pred || (s_ret && !s_data);

bool E_stall = 0;
bool E_bubble = s_data || s_pred;
```

```diff
--- pipe-nobypass-orig.hcl
+++ pipe-nobypass.hcl
@@ -304,38 +304,48 @@

 ################ Pipeline Register Control #########################

+# data hazard
+bool s_data =
+       (d_srcA != RNONE && d_srcA in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+       (d_srcB != RNONE && d_srcB in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM });
+
+# ret
+bool s_ret = IRET in { D_icode, E_icode, M_icode };
+
+# PC misprediction
+bool s_pred = E_icode == IJXX && !e_Cnd;
+
 # Should I stall or inject a bubble into Pipeline Register F?
 # At most one of these can be true.
 bool F_bubble = 0;
-bool F_stall =
-       # Modify the following to stall the update of pipeline register F
-       0 ||
-       # Stalling at fetch while ret passes through pipeline
-       IRET in { D_icode, E_icode, M_icode };
+# bool F_stall = (s_data || s_ret) && !s_pred;
+bool F_stall = (
+               (d_srcA != RNONE && d_srcA in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+               (d_srcB != RNONE && d_srcB in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+               IRET in { D_icode, E_icode, M_icode }
+       ) && !(E_icode == IJXX && !e_Cnd);

 # Should I stall or inject a bubble into Pipeline Register D?
 # At most one of these can be true.
-bool D_stall =
-       # Modify the following to stall the instruction in decode
-       0;
-
-bool D_bubble =
-       # Mispredicted branch
-       (E_icode == IJXX && !e_Cnd) ||
-       # Stalling at fetch while ret passes through pipeline
-       !(E_icode in { IMRMOVQ, IPOPQ } && E_dstM in { d_srcA, d_srcB }) &&
-       # but not condition for a generate/use hazard
-       !0 &&
-         IRET in { D_icode, E_icode, M_icode };
+# bool D_stall = s_data && !s_pred;
+bool D_stall = (
+               (d_srcA != RNONE && d_srcA in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+               (d_srcB != RNONE && d_srcB in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM })
+       ) && !(E_icode == IJXX && !e_Cnd);
+# bool D_bubble = s_pred || (s_ret && !s_data);
+bool D_bubble = (E_icode == IJXX && !e_Cnd) || (IRET in { D_icode, E_icode, M_icode } && !(
+               (d_srcA != RNONE && d_srcA in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+               (d_srcB != RNONE && d_srcB in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM })
+       ));

 # Should I stall or inject a bubble into Pipeline Register E?
 # At most one of these can be true.
 bool E_stall = 0;
-bool E_bubble =
-       # Mispredicted branch
-       (E_icode == IJXX && !e_Cnd) ||
-       # Modify the following to inject bubble into the execute stage
-       0;
+# bool E_bubble = s_data || s_pred;
+bool E_bubble =
+       (d_srcA != RNONE && d_srcA in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+       (d_srcB != RNONE && d_srcB in { e_dstE, E_dstM, M_dstE, M_dstM, W_dstE, W_dstM }) ||
+       (E_icode == IJXX && !e_Cnd);

 # Should I stall or inject a bubble into Pipeline Register M?
 # At most one of these can be true.
```

## 4.54 PIPE-full

iaddq for PIPE

```diff
--- pipe-full-orig.hcl
+++ pipe-full.hcl
@@ -158,7 +158,7 @@
 # Is instruction valid?
 bool instr_valid = f_icode in
        { INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
-         IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ };
+         IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IIADDQ };

 # Determine status code for fetched instruction
 word f_stat = [
@@ -171,11 +171,11 @@
 # Does fetched instruction require a regid byte?
 bool need_regids =
        f_icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ,
-                    IIRMOVQ, IRMMOVQ, IMRMOVQ };
+                    IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ };

 # Does fetched instruction require a constant word?
 bool need_valC =
-       f_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL };
+       f_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL, IIADDQ };

 # Predict next value of PC
 word f_predPC = [
@@ -195,14 +195,14 @@

 ## What register should be used as the B source?
 word d_srcB = [
-       D_icode in { IOPQ, IRMMOVQ, IMRMOVQ  } : D_rB;
+       D_icode in { IOPQ, IRMMOVQ, IMRMOVQ, IIADDQ  } : D_rB;
        D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
        1 : RNONE;  # Don't need register
 ];

 ## What register should be used as the E destination?
 word d_dstE = [
-       D_icode in { IRRMOVQ, IIRMOVQ, IOPQ} : D_rB;
+       D_icode in { IRRMOVQ, IIRMOVQ, IOPQ, IIADDQ } : D_rB;
        D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
        1 : RNONE;  # Don't write any register
 ];
@@ -239,7 +239,7 @@
 ## Select input A to ALU
 word aluA = [
        E_icode in { IRRMOVQ, IOPQ } : E_valA;
-       E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ } : E_valC;
+       E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IIADDQ } : E_valC;
        E_icode in { ICALL, IPUSHQ } : -8;
        E_icode in { IRET, IPOPQ } : 8;
        # Other instructions don't need ALU
@@ -248,7 +248,7 @@
 ## Select input B to ALU
 word aluB = [
        E_icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL,
-                    IPUSHQ, IRET, IPOPQ } : E_valB;
+                    IPUSHQ, IRET, IPOPQ, IIADDQ } : E_valB;
        E_icode in { IRRMOVQ, IIRMOVQ } : 0;
        # Other instructions don't need ALU
 ];
@@ -260,7 +260,7 @@
 ];

 ## Should the condition codes be updated?
-bool set_cc = E_icode == IOPQ &&
+bool set_cc = E_icode in { IOPQ, IIADDQ } &&
        # State changes only during normal operation
        !m_stat in { SADR, SINS, SHLT } && !W_stat in { SADR, SINS, SHLT };
```

## 4.55 PIPE-nt

never taken strategy when predict PC

在预测PC时改变策略：

```
word f_predPC = [
	f_icode == ICALL : f_valC;  # call指令直接跳转
	f_icode == IJXX && f_ifun == UNCOND : f_valC; # 无条件跳转
	1 : f_valP; # 有条件跳转和其他指令一样都预测为valP
];
```

执行阶段，在原本全部预测跳转的设计中，预测错误时，Select PC使用M_valA里的地址（实际上是val_P）修正分支，重新开始执行。而在本设计中，对于条件跳转预测的本来就是val_P。要使此时M_valA中的地址能够被Select PC用于修正，其中的值必须是val_C。于是修改e_valA的产生逻辑如下：

```
word e_valA = [
	E_icode == IJXX && E_ifun != UNCOND && e_Cond : E_valC;  # 在预测错误时，向M_valA传递正确的修正地址
	1: E_valA;
];
```

注意到对于所有JXX指令，valA除了在预测错误时修正PC之外没有其他用处（JXX并不操作内存和寄存器文件），所以逻辑可进行一些简化：

```
word e_valA = [
	E_icode == IJXX : E_valC;  # 对于JXX指令，向M_valA传递跳转地址
	1 : E_valA;
];
```

对Select PC逻辑进行修改：

```
word f_pc = [
	# M_icode == IJXX && !M_Cnd : M_valA;  # 原本的预测错误
	M_icode == IJXX && M_ifun != UNCOND && M_Cnd : M_valA;  # 预测错误
	W_icode == IRET : W_valM;
	1 : F_predPC;
];
```

最后对流水线中判断分支预测错误的语句进行修改：

```
E_icode == IJXX && !e_Cnd  # old
E_icode == IJXX && E_ifun != UNCOND && e_Cnd  # new
```

```diff
--- pipe-nt-orig.hcl
+++ pipe-nt.hcl
@@ -139,7 +139,7 @@
 ## What address should instruction be fetched at
 word f_pc = [
        # Mispredicted branch.  Fetch at incremented PC
-       M_icode == IJXX && !M_Cnd : M_valA;
+       M_icode == IJXX && M_ifun != UNCOND && M_Cnd : M_valA;
        # Completion of RET instruction
        W_icode == IRET : W_valM;
        # Default: Use predicted value of PC
@@ -183,7 +183,8 @@
 # Predict next value of PC
 word f_predPC = [
        # BNT: This is where you'll change the branch prediction rule
-       f_icode in { IJXX, ICALL } : f_valC;
+       f_icode == ICALL : f_valC;  # call
+       f_icode == IJXX && f_ifun == UNCOND : f_valC;  # unconditional jump
        1 : f_valP;
 ];

@@ -273,7 +274,10 @@
        !m_stat in { SADR, SINS, SHLT } && !W_stat in { SADR, SINS, SHLT };

 ## Generate valA in execute stage
-word e_valA = E_valA;    # Pass valA through stage
+word e_valA = [
+       E_icode == IJXX : E_valC;
+       1 : E_valA;
+];

 ## Set dstE to RNONE in event of not-taken conditional move
 word e_dstE = [
@@ -343,7 +347,7 @@

 bool D_bubble =
        # Mispredicted branch
-       (E_icode == IJXX && !e_Cnd) ||
+       (E_icode == IJXX && E_ifun != UNCOND && e_Cnd) ||
        # Stalling at fetch while ret passes through pipeline
        # but not condition for a load/use hazard
        !(E_icode in { IMRMOVQ, IPOPQ } && E_dstM in { d_srcA, d_srcB }) &&
@@ -354,7 +358,7 @@
 bool E_stall = 0;
 bool E_bubble =
        # Mispredicted branch
-       (E_icode == IJXX && !e_Cnd) ||
+       (E_icode == IJXX && E_ifun != UNCOND && e_Cnd) ||
        # Conditions for a load/use hazard
        E_icode in { IMRMOVQ, IPOPQ } &&
         E_dstM in { d_srcA, d_srcB};
```

## 4.56 PIPE-btfnt

backward taken, forward not-taken strategy when predict PC

在预测PC时改变策略：

```
word f_predPC = [
	f_icode == ICALL : f_valC;  # call 直接跳转
	f_icode == IJXX && (f_ifun == UNCOND || f_valC < f_valP) : f_valC;  # 无条件跳转或后向跳转
	1 : f_valP;  # forward not taken
];
```

注意到对于所有JXX指令，都不操作内存和寄存器文件，所以流水线寄存器中的val都可以被用来存储地址。这里我们使用valE存储valC（由valC和0在ALU中加法得到），用valA存储valP（直接传递）。

```
word aluA = [
	E_icode in { IRRMOVQ, IOPQ } : E_valA;
	E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX } : E_valC;  # 在JXX指令执行时，valC作为第一个加数
	E_icode in { ICALL, IPUSHQ } : -8;
	E_icode in { IRET, IPOPQ } : 8;
];

word aluB = [
	E_icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL, 
		     IPUSHQ, IRET, IPOPQ } : E_valB;
	E_icode in { IRRMOVQ, IIRMOVQ, IJXX } : 0;  # 在JXX指令执行时，0作为第二个加数
	# Other instructions don't need ALU
];

word e_valA = E_valA;
```

在JXX指令执行阶段，aluA设置为E_valC，aluB设置为0，就会在e_valE中得到valC，进而将valC传入M_valE。直接传递valA，可将valP传入M_valA。

这样，Select PC就可以使用M_valE和M_valA在分支预测错误时修正产生正确的PC。

```
word f_pc = [  # valC in M_valE, valP in M_valA
	M_icode == IJXX && M_ifun != UNCOND && M_Cnd && M_valE >= M_valA : M_valE;  # 修正应进行的向后跳转
	M_icode == IJXX && M_ifun != UNCOND && !M_Cnd && M_valE < M_valA : M_valA;  # 修正不应进行的向前跳转
	W_icode == IRET : W_valM;
	1 : F_predPC;
];
```

最后对流水线中判断分支预测错误的语句进行修改：

```
E_icode == IJXX && !e_Cnd  # old
E_icode == IJXX && E_ifun != UNCOND && ((e_Cnd && E_valC >= E_valA) || (!e_Cnd && E_valC < E_valA))  # new
```

```diff
--- pipe-btfnt-orig.hcl
+++ pipe-btfnt.hcl
@@ -139,7 +139,8 @@
 ## What address should instruction be fetched at
 word f_pc = [
        # Mispredicted branch.  Fetch at incremented PC
-       M_icode == IJXX && !M_Cnd : M_valA;
+       M_icode == IJXX && M_ifun != UNCOND && M_Cnd && M_valE >= M_valA : M_valE;
+       M_icode == IJXX && M_ifun != UNCOND && !M_Cnd && M_valE < M_valA : M_valA;
        # Completion of RET instruction
        W_icode == IRET : W_valM;
        # Default: Use predicted value of PC
@@ -183,8 +184,9 @@
 # Predict next value of PC
 word f_predPC = [
        # BBTFNT: This is where you'll change the branch prediction rule
-       f_icode in { IJXX, ICALL } : f_valC;
-       1 : f_valP;
+       f_icode == ICALL : f_valC;  # call
+       f_icode == IJXX && (f_ifun == UNCOND || f_valC < f_valP) : f_valC;  # unconditional or backward
+       1 : f_valP;  # forward not taken
 ];

 ################ Decode Stage ######################################
@@ -247,7 +249,7 @@
 ## Select input A to ALU
 word aluA = [
        E_icode in { IRRMOVQ, IOPQ } : E_valA;
-       E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ } : E_valC;
+       E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX } : E_valC;
        E_icode in { ICALL, IPUSHQ } : -8;
        E_icode in { IRET, IPOPQ } : 8;
        # Other instructions don't need ALU
@@ -257,7 +259,7 @@
 word aluB = [
        E_icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL,
                     IPUSHQ, IRET, IPOPQ } : E_valB;
-       E_icode in { IRRMOVQ, IIRMOVQ } : 0;
+       E_icode in { IRRMOVQ, IIRMOVQ, IJXX } : 0;
        # Other instructions don't need ALU
 ];

@@ -343,7 +345,7 @@

 bool D_bubble =
        # Mispredicted branch
-       (E_icode == IJXX && !e_Cnd) ||
+       (E_icode == IJXX && E_ifun != UNCOND && ((e_Cnd && E_valC >= E_valA) || (!e_Cnd && E_valC < E_valA))) ||
        # BBTFNT: This condition will change
        # Stalling at fetch while ret passes through pipeline
        # but not condition for a load/use hazard
@@ -355,7 +357,7 @@
 bool E_stall = 0;
 bool E_bubble =
        # Mispredicted branch
-       (E_icode == IJXX && !e_Cnd) ||
+       (E_icode == IJXX && E_ifun != UNCOND && ((e_Cnd && E_valC >= E_valA) || (!e_Cnd && E_valC < E_valA))) ||
        # BBTFNT: This condition will change
        # Conditions for a load/use hazard
        E_icode in { IMRMOVQ, IPOPQ } &&
```

## 4.57 PIPE-lf

load forwarding

只有rmmovq和pushq会在直到访存阶段才使用某个寄存器的值，而且它们都将rA寄存器中的值写入内存。

```
E_icode in { IMRMOVQ, IPOPQ } && E_dstM in { d_srcA, d_srcB }  # 原来的加载/使用冒险条件
D_icode in { IRMMOVQ, IPUSHQ } && E_dstM == d_srcA  # 加载转发能够生效的条件

E_icode in { IMRMOVQ, IPOPQ } && (
	E_dstM == d_srcB ||
	(E_dstM == d_srcA && !(D_icode in { IRMMOVQ, IPUSHQ }))
)  # 加入加载转发后，加载/使用冒险条件
```
把新的加载/使用冒险条件替换进流水线控制逻辑。

Fwd A实现很简单，只要在加载转发能够生效的条件下将访存阶段读出来的内存值转发到e_valA就可以了。

```
word e_valA = [
	E_icode in { IRMMOVQ, IPUSHQ } && M_dstM == E_srcA : m_valM;  # 上面加载转发能够生效的条件改了个阶段
	1 : E_valA;  # 其他情况维持不变
];
```

```diff
--- pipe-lf-orig.hcl
+++ pipe-lf.hcl
@@ -271,6 +271,7 @@
 ##   from memory stage when appropriate
 ## Here it is set to the default used in the normal pipeline
 word e_valA = [
+       E_icode in { IRMMOVQ, IPUSHQ } && M_dstM == E_srcA : m_valM;
        1 : E_valA;  # Use valA from stage pipe register
 ];

@@ -329,7 +330,7 @@
 bool F_stall =
        # Conditions for a load/use hazard
        ## Set this to the new load/use condition
-       0 ||
+       (E_icode in { IMRMOVQ, IPOPQ } && (E_dstM == d_srcB || (E_dstM == d_srcA && !(D_icode in { IRMMOVQ, IPUSHQ })))) ||
        # Stalling at fetch while ret passes through pipeline
        IRET in { D_icode, E_icode, M_icode };

@@ -338,14 +339,14 @@
 bool D_stall =
        # Conditions for a load/use hazard
        ## Set this to the new load/use condition
-       0;
+       E_icode in { IMRMOVQ, IPOPQ } && (E_dstM == d_srcB || (E_dstM == d_srcA && !(D_icode in { IRMMOVQ, IPUSHQ })));

 bool D_bubble =
        # Mispredicted branch
        (E_icode == IJXX && !e_Cnd) ||
        # Stalling at fetch while ret passes through pipeline
        # but not condition for a load/use hazard
 IPUSHQ })))) &&
          IRET in { D_icode, E_icode, M_icode };

 # Should I stall or inject a bubble into Pipeline Register E?
@@ -356,7 +357,7 @@
        (E_icode == IJXX && !e_Cnd) ||
        # Conditions for a load/use hazard
        ## Set this to the new load/use condition
-       0;
+       E_icode in { IMRMOVQ, IPOPQ } && (E_dstM == d_srcB || (E_dstM == d_srcA && !(D_icode in { IRMMOVQ, IPUSHQ })));

 # Should I stall or inject a bubble into Pipeline Register M?
 # At most one of these can be true.
```

## 4.58 PIPE-1w

register file have only one writing port now

### 取指 F

修改取指和指令预测逻辑，实现将pop指令取出两次：

```
word f_predPC = [
	f_icode in { IJXX, ICALL } : f_valC;
	f_icode == IPOPQ : f_pc;  # 当前指令为popq时，预测下一条指令地址为自身，作为pop2
	1 : f_valP;
];

word f_icode = [
	imem_error : INOP;
	D_icode == IPOPQ : IPOP2;  # 当在译码阶段的指令是popq时说明当前取指阶段的指令是第二次取出的popq，将指令码更改为IPOP2
	1: imem_icode;
];
```

原来的popq被拆成两半，修改后popq将%rsp值减去8，pop2将-8(%rdp)值写入rA，所以现在只有pop2需要使用寄存器标记

```
bool instr_valid = f_icode in 
	{ INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
	  IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IPOP2 };  # 现在IPOP2也是合法的指令码

bool need_regids =
	f_icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOP2,   # pop2需要使用寄存器标记，popq不需要了
		     IIRMOVQ, IRMMOVQ, IMRMOVQ };
```

### 译码D / 写回 W

都是译码阶段完成的操作，写回阶段根据译码阶段已经产生的dst进行写回。

```
word d_srcA = [
	D_icode in { IRRMOVQ, IRMMOVQ, IOPQ, IPUSHQ  } : D_rA;
	# D_icode in { IPOPQ, IRET } : RRSP;  原本popq需要valA中存放%rsp的值作为读取内存的地址，现在popq不需要读内存了，而pop2读内存的地址和mrmovq一样是valE，故不需要将%rsp的值再传进valA
	D_icode == IRET : RRSP;
	1 : RNONE;
];

word d_srcB = [
	D_icode in { IOPQ, IRMMOVQ, IMRMOVQ  } : D_rB;
	# D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	D_icode in { IPUSHQ, IPOPQ, ICALL, IRET, IPOP2 } : RRSP;  # ipop2需要对%rsp进行运算，故需要将其值传入valB
	1 : RNONE;  # Don't need register
];
```

```
word d_dstE = [
	D_icode in { IRRMOVQ, IIRMOVQ, IOPQ} : D_rB;  # 和之前一样，popq需要将运算(+8)后的%rsp值写回%rsp
	D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't write any register
];

word d_dstM = [
	# D_icode in { IMRMOVQ, IPOPQ } : D_rA;  # popq不再需要读内存
	D_icode in { IMRMOVQ, IPOP2 } : D_rA;  # pop2需要将读出的内存值写入目标寄存器
	1 : RNONE;
];
```

### 执行 E

```
word aluA = [
	E_icode in { IRRMOVQ, IOPQ } : E_valA;
	E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ } : E_valC;
	E_icode in { ICALL, IPUSHQ, IPOP2 } : -8;  # pop2需要计算-8(%rdp)
	E_icode in { IRET, IPOPQ } : 8;  # popq需要计算%rdp + 8
];

word aluB = [
	E_icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL, 
		     IPUSHQ, IRET, IPOPQ, IPOP2 } : E_valB;  # valB中是%rsp的值
	E_icode in { IRRMOVQ, IIRMOVQ } : 0;
];
```

### 访存 M

```
word mem_addr = [
	M_icode in { IRMMOVQ, IPUSHQ, ICALL, IMRMOVQ, IPOP2 } : M_valE;  # pop2将计算结果-8(%rdp)作为读内存的地址
	# M_icode in { IPOPQ, IRET } : M_valA;
	M_icode == IRET : M_valA;  # popq现在不读取内存了
];

bool mem_read = M_icode in { IMRMOVQ, IPOP2, IRET };  # 读内存的任务交给pop2了
```

### 流水线控制

加载/使用数据冒险的条件需要修改。popq加载的功能移交给了pop2，所以条件应修改如下：

```
E_icode in { IMRMOVQ, IPOPQ } && E_dstM in { d_srcA, d_srcB }  # 原本
E_icode in { IMRMOVQ, IPOP2 } && E_dstM in { d_srcA, d_srcB }  # 将IPOPQ直接改为IPOP2
```

### DIFF

```diff
--- pipe-1w-orig.hcl
+++ pipe-1w.hcl
@@ -157,6 +157,7 @@
 ## so that it will be IPOP2 when fetched for second time.
 word f_icode = [
        imem_error : INOP;
+       D_icode == IPOPQ : IPOP2;
        1: imem_icode;
 ];

@@ -169,7 +170,7 @@
 # Is instruction valid?
 bool instr_valid = f_icode in
        { INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
-         IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ };
+         IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ, IPOP2 };

 # Determine status code for fetched instruction
 word f_stat = [
@@ -181,7 +182,7 @@

 # Does fetched instruction require a regid byte?
 bool need_regids =
-       f_icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ,
+       f_icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOP2,
                     IIRMOVQ, IRMMOVQ, IMRMOVQ };

 # Does fetched instruction require a constant word?
@@ -192,6 +193,7 @@
 word f_predPC = [
        f_icode in { IJXX, ICALL } : f_valC;
        ## 1W: Want to refetch popq one time
+       f_icode == IPOPQ : f_pc;
        1 : f_valP;
 ];

@@ -204,14 +206,14 @@
 ## What register should be used as the A source?
 word d_srcA = [
        D_icode in { IRRMOVQ, IRMMOVQ, IOPQ, IPUSHQ  } : D_rA;
-       D_icode in { IPOPQ, IRET } : RRSP;
+       D_icode == IRET : RRSP;
        1 : RNONE; # Don't need register
 ];

 ## What register should be used as the B source?
 word d_srcB = [
        D_icode in { IOPQ, IRMMOVQ, IMRMOVQ  } : D_rB;
-       D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
+       D_icode in { IPUSHQ, IPOPQ, ICALL, IRET, IPOP2 } : RRSP;
        1 : RNONE;  # Don't need register
 ];

@@ -224,7 +226,7 @@

 ## What register should be used as the M destination?
 word d_dstM = [
-       D_icode in { IMRMOVQ, IPOPQ } : D_rA;
+       D_icode in { IMRMOVQ, IPOP2 } : D_rA;
        1 : RNONE;  # Don't write any register
 ];

@@ -255,7 +257,7 @@
 word aluA = [
        E_icode in { IRRMOVQ, IOPQ } : E_valA;
        E_icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ } : E_valC;
-       E_icode in { ICALL, IPUSHQ } : -8;
+       E_icode in { ICALL, IPUSHQ, IPOP2 } : -8;
        E_icode in { IRET, IPOPQ } : 8;
        # Other instructions don't need ALU
 ];
@@ -263,7 +265,7 @@
 ## Select input B to ALU
 word aluB = [
        E_icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL,
-                    IPUSHQ, IRET, IPOPQ } : E_valB;
+                    IPUSHQ, IRET, IPOPQ, IPOP2 } : E_valB;
        E_icode in { IRRMOVQ, IIRMOVQ } : 0;
        # Other instructions don't need ALU
 ];
@@ -292,13 +294,13 @@

 ## Select memory address
 word mem_addr = [
-       M_icode in { IRMMOVQ, IPUSHQ, ICALL, IMRMOVQ } : M_valE;
-       M_icode in { IPOPQ, IRET } : M_valA;
+       M_icode in { IRMMOVQ, IPUSHQ, ICALL, IMRMOVQ, IPOP2 } : M_valE;
+       M_icode == IRET : M_valA;
        # Other instructions don't need address
 ];

 ## Set read control signal
-bool mem_read = M_icode in { IMRMOVQ, IPOPQ, IRET };
+bool mem_read = M_icode in { IMRMOVQ, IPOP2, IRET };

 ## Set write control signal
 bool mem_write = M_icode in { IRMMOVQ, IPUSHQ, ICALL };
@@ -350,7 +352,7 @@
 bool F_bubble = 0;
 bool F_stall =
        # Conditions for a load/use hazard
-       E_icode in { IMRMOVQ, IPOPQ } &&
+       E_icode in { IMRMOVQ, IPOP2 } &&
         E_dstM in { d_srcA, d_srcB } ||
        # Stalling at fetch while ret passes through pipeline
        IRET in { D_icode, E_icode, M_icode };
@@ -359,7 +361,7 @@
 # At most one of these can be true.
 bool D_stall =
        # Conditions for a load/use hazard
-       E_icode in { IMRMOVQ, IPOPQ } &&
+       E_icode in { IMRMOVQ, IPOP2 } &&
         E_dstM in { d_srcA, d_srcB };

 bool D_bubble =
@@ -367,7 +369,7 @@
        (E_icode == IJXX && !e_Cnd) ||
        # Stalling at fetch while ret passes through pipeline
        # but not condition for a load/use hazard
-       !(E_icode in { IMRMOVQ, IPOPQ } && E_dstM in { d_srcA, d_srcB }) &&
+       !(E_icode in { IMRMOVQ, IPOP2 } && E_dstM in { d_srcA, d_srcB }) &&
        # 1W: This condition will change
          IRET in { D_icode, E_icode, M_icode };

@@ -378,7 +380,7 @@
        # Mispredicted branch
        (E_icode == IJXX && !e_Cnd) ||
        # Conditions for a load/use hazard
-       E_icode in { IMRMOVQ, IPOPQ } &&
+       E_icode in { IMRMOVQ, IPOP2 } &&
         E_dstM in { d_srcA, d_srcB};

 # Should I stall or inject a bubble into Pipeline Register M?
```

# Lab

## Part.A

```
# sum.ys

.pos 0
	irmovq stack, %rsp
	irmovq ele1, %rdi
	call sum_list
	halt

.align 8
ele1:
	.quad 0x00a
	.quad ele2
ele2:
	.quad 0x0b0
	.quad ele3
ele3:
	.quad 0xc00
	.quad 0

sum_list:
	irmovq $0, %rax
	jmp cnd
loop:
	mrmovq (%rdi), %rsi
	addq %rsi, %rax
	mrmovq 8(%rdi), %rdi
cnd:
	andq %rdi, %rdi
	jne loop
	ret

.pos 0x200
stack:

```

```
# rsum.ys

.pos 0
	irmovq stack, %rsp
	irmovq ele1, %rdi
	call rsum_list
	halt

.align 8
ele1:
	.quad 0x00a
	.quad ele2
ele2:
	.quad 0x0b0
	.quad ele3
ele3:
	.quad 0xc00
	.quad 0

rsum_list:
	andq %rdi, %rdi
	jne ne
	irmovq $0, %rax
	ret
ne:
	mrmovq (%rdi), %rsi
	pushq %rsi
	mrmovq 8(%rdi), %rdi
	call rsum_list
	popq %rsi
	addq %rsi, %rax
	ret

.pos 0x200
stack:

```

```
# copy.ys

.pos 0
	irmovq stack, %rsp
	irmovq src, %rdi
	irmovq dest, %rsi
	irmovq $3, %rdx
	call copy_block
	halt

.align 8
src:
	.quad 0x00a
	.quad 0x0b0
	.quad 0xc00
dest:
	.quad 0x111
	.quad 0x222
	.quad 0x333

copy_block:
	irmovq $0, %rax
	irmovq $8, %r8
	irmovq $1, %r9
	jmp cnd
loop:
	mrmovq (%rdi), %rcx
	addq %r8, %rdi
	rmmovq %rcx, (%rsi)
	addq %r8, %rsi
	xorq %rcx, %rax
	subq %r9, %rdx
cnd:
	andq %rdx, %rdx
	jg loop
	ret

.pos 0x200
stack:

```

## Part.B

同Homework 4.52

## Part.C

首先先按照homework 4.54实现iaddq，写出朴素ncopy代码如下：

```
	irmovq $0, %rax
    andq %rdx, %rdx
	jmp cnd
loop:
	mrmovq (%rdi), %rcx
    iaddq $8, %rdi
    rmmovq %rcx, (%rsi)
    iaddq $8, %rsi

    andq %rcx, %rcx
	jle np
	iaddq $1, %rax
np:

	iaddq $-1, %rdx
cnd:
	jg loop
```

注意`mrmovq (%rdi), %rcx`和`rmmovq %rcx, (%rsi)`中间要隔开一条指令以避免加载使用数据冒险。这个版本的平均CPE为10.62，喜提零分。

考虑优化循环中统计正数数量的条件判断。换成条件传送需要提前计算%rax+1的值，平均CPE表现反而会更劣。转换思路，为什么不把iaddq设置成如果条件满足就执行加法呢？

修改取指F阶段功能码部分：

```
word f_ifun = [
	imem_error : FNONE;
	f_icode == IIADDQ : 6;  # greater
	1: imem_ifun;
];
```

将iaddq的功能码设置成和jg/cmovg相同，这样在执行阶段e_Cnd就会指示CC的内容是否意味着大于0。然后在执行阶段实现条件传送：

```
word e_dstE = [
	E_icode in { IRRMOVQ, IIADDQ } && !e_Cnd : RNONE;
	1 : E_dstE;
];
```

如果条件不满足就像条件传送一样取消对寄存器的修改。这种改动并不会影响加法，因为ALU的功能码在非opq指令时始终是加法。

```
word alufun = [
	E_icode == IOPQ : E_ifun;
	1 : ALUADD;
];
```

再修改ncopy的实现：

```
	irmovq $0, %rax
	andq %rdx, %rdx
	jmp cnd
loop:
	mrmovq (%rdi), %rcx
	iaddq $8, %rdi  # 不受影响，因为此时CC由%rdx设置，其一定大于0
	rmmovq %rcx, (%rsi)
	iaddq $8, %rsi

	andq %rcx, %rcx
	iaddq $1, %rax  # 直接根据%rcx是否>0条件加法

	iaddq $-1, %rdx
cnd:
	jg loop
```

这个版本平均CPE为9.19。

然后再简单展开一下：

```
	irmovq $0, %rax
	irmovq $1, %r8
    andq %r8, %r8  # 设置CC使得iaddq一定会执行
    iaddq $-1, %rdx
	jmp cnd
loop:
	mrmovq (%rdi), %rcx
    mrmovq 8(%rdi), %r8
    iaddq $16, %rdi
    rmmovq %rcx, (%rsi)
    rmmovq %r8, 8(%rsi)
    iaddq $16, %rsi  # 一次循环搞两个
    
    andq %rcx, %rcx
	iaddq $1, %rax

    andq %r8, %r8
	iaddq $1, %rax

	iaddq $-2, %rdx
cnd:
	jg loop

    jne Done
    mrmovq (%rdi), %rcx
    rmmovq %rcx, (%rsi)
    andq %rcx, %rcx
	iaddq $1, %rax
```

这个版本的平均CPE为7.45，已经满分了。其实继续展开还能更快，我试了下展开5次能到6.91的平均CPE。

我还做了一些其他尝试：

- 仅仅实现普通的iaddq，展开循环5次，平均CPE7.84，我没法做到满分。

    代码文件`ncopy-simple.ys` `pipe-full-simple.hcl`

- 实现高度集成的iaddq，根据寄存器不同实现不同的功能。除了实现上面的条件加法之外，将读写内存和增加指针结合进同一条指令，可以在**不进行循环展开**的情况下做到7.41的平均CPE。但由于指令的实现内部直接+8，在循环展开后无法享受到统一加法的速度提升，所以展开5次平均CPE提升相对较小，但也能到6.35。

    代码文件`ncopy-magic.ys` `pipe-full-magic.hcl`

