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

最后将流水线中判断分支预测错误的语句进行修改：

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

## 4.57 PIPE-lf

load forwarding

## 4.58 PIPE-1w

register file have only one writing port now