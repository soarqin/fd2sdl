// opcode @ 0x36e3d (size=5)

void thunk_FUN_0003cbb3(void)

{
                    /* WARNING: Subroutine does not return */
  FUN_0003cbb3();
}


// opcode @ 0x36e57 (size=14)

void __regparm3 FUN_00036e57(char *param_1,undefined4 param_2,char param_3)

{
  undefined4 *puVar1;
  byte *pbVar2;
  uint uVar3;
  char extraout_CL;
  char extraout_CL_00;
  int iVar4;
  int unaff_EBX;
  undefined1 *puVar5;
  int unaff_ESI;
  undefined8 uVar6;
  
  *param_1 = *param_1 + (char)param_1;
  puVar5 = &stack0xfffffffc;
  iVar4 = DAT_00004178 + 1;
  DAT_00004178 = iVar4;
  if ((DAT_00052766 != 0) && ((iVar4 == 1 || (DAT_00004170 != 0)))) {
    uVar6 = FUN_0003c972();
    iVar4 = (int)((ulonglong)uVar6 >> 0x20);
    param_3 = extraout_CL;
    if ((int)uVar6 == 0) {
      uVar6 = FUN_000353e4();
      iVar4 = (int)((ulonglong)uVar6 >> 0x20);
      param_3 = extraout_CL_00;
      if ((int)uVar6 != 0) {
        uVar3 = 1;
        goto LAB_00036ea0;
      }
    }
  }
  uVar3 = 0;
LAB_00036ea0:
  if (uVar3 != 0) {
    puVar1 = *(undefined4 **)(unaff_ESI + 0x27);
    *(char *)(uVar3 + 0x24448b68) =
         (*(char *)(uVar3 + 0x24448b68) - (char)iVar4) - (0xdbbb74ff < uVar3);
    pbVar2 = (byte *)*puVar1;
    *pbVar2 = *pbVar2 | (byte)pbVar2;
    *(char *)(unaff_EBX + 0x416415) = *(char *)(unaff_EBX + 0x416415) + param_3;
    *(char *)(iVar4 + -0x18) = *(char *)(iVar4 + -0x18) + (char)iVar4;
    out((short)iVar4,pbVar2);
    iVar4 = puVar1[1];
    *pbVar2 = *pbVar2 + (byte)pbVar2;
    puVar5 = (undefined1 *)(iVar4 + 0x14);
  }
  *(undefined4 *)(puVar5 + -4) = *(undefined4 *)(puVar5 + 0x18);
  *(undefined4 *)(puVar5 + -8) = *(undefined4 *)(puVar5 + 0x14);
  *(undefined4 *)(puVar5 + -0xc) = *(undefined4 *)(puVar5 + 0x10);
  *(undefined4 *)(puVar5 + -0x10) = 0x36edb;
  FUN_0003ed18();
  DAT_00004178 = DAT_00004178 + -1;
  return;
}


// opcode @ 0x36e65 (size=63)

void __regparm3 FUN_00036e65(char *param_1,int param_2,char param_3)

{
  undefined4 *puVar1;
  byte *pbVar2;
  int iVar3;
  uint uVar4;
  char extraout_CL;
  char extraout_CL_00;
  int unaff_EBX;
  int unaff_ESI;
  undefined8 uVar5;
  
  *param_1 = *param_1 + (char)param_1;
  param_2 = param_2 + 1;
  DAT_00004178 = param_2;
  if ((DAT_00052766 != 0) && ((param_2 == 1 || (DAT_00004170 != 0)))) {
    uVar5 = FUN_0003c972();
    param_2 = (int)((ulonglong)uVar5 >> 0x20);
    param_3 = extraout_CL;
    if ((int)uVar5 == 0) {
      uVar5 = FUN_000353e4();
      param_2 = (int)((ulonglong)uVar5 >> 0x20);
      param_3 = extraout_CL_00;
      if ((int)uVar5 != 0) {
        uVar4 = 1;
        goto LAB_00036ea0;
      }
    }
  }
  uVar4 = 0;
LAB_00036ea0:
  if (uVar4 != 0) {
    puVar1 = *(undefined4 **)(unaff_ESI + 0x27);
    *(char *)(uVar4 + 0x24448b68) =
         (*(char *)(uVar4 + 0x24448b68) - (char)param_2) - (0xdbbb74ff < uVar4);
    pbVar2 = (byte *)*puVar1;
    *pbVar2 = *pbVar2 | (byte)pbVar2;
    *(char *)(unaff_EBX + 0x416415) = *(char *)(unaff_EBX + 0x416415) + param_3;
    *(char *)(param_2 + -0x18) = *(char *)(param_2 + -0x18) + (char)param_2;
    out((short)param_2,pbVar2);
    iVar3 = puVar1[1];
    *pbVar2 = *pbVar2 + (byte)pbVar2;
    register0x00000010 = (BADSPACEBASE *)(iVar3 + 0x14);
  }
  *(undefined4 *)((int)register0x00000010 + -4) = *(undefined4 *)((int)register0x00000010 + 0x18);
  *(undefined4 *)((int)register0x00000010 + -8) = *(undefined4 *)((int)register0x00000010 + 0x14);
  *(undefined4 *)((int)register0x00000010 + -0xc) = *(undefined4 *)((int)register0x00000010 + 0x10);
  *(undefined4 *)((int)register0x00000010 + -0x10) = 0x36edb;
  FUN_0003ed18();
  DAT_00004178 = DAT_00004178 + -1;
  return;
}


// opcode @ 0x36ea7 (size=57)

void __regparm3 FUN_00036ea7(undefined4 param_1,int param_2,char param_3,int param_4)

{
  byte *pbVar1;
  char *pcVar2;
  byte bVar3;
  byte bVar5;
  byte bVar6;
  byte bVar8;
  int unaff_EBX;
  int unaff_EDI;
  byte in_CF;
  byte in_AF;
  byte *unaff_retaddr;
  byte bVar4;
  uint uVar7;
  
  pbVar1 = (byte *)(unaff_EDI + -0x75);
  bVar3 = *pbVar1;
  bVar8 = (byte)param_2;
  bVar4 = *pbVar1;
  *pbVar1 = (bVar4 - bVar8) - in_CF;
  bVar5 = 9 < ((byte)param_1 & 0xf) | in_AF;
  bVar6 = (byte)param_1 + bVar5 * '\x06';
  uVar7 = CONCAT31((int3)((uint)param_1 >> 8),
                   bVar6 + (0x90 < (bVar6 & 0xf0) |
                           (bVar3 < bVar8 || (byte)(bVar4 - bVar8) < in_CF) | bVar5 * (0xf9 < bVar6)
                           ) * '`');
  pcVar2 = (char *)(uVar7 + 0x24448b68);
  *pcVar2 = (*pcVar2 - bVar8) - (0xdbbb74ff < uVar7);
  *unaff_retaddr = *unaff_retaddr | (byte)unaff_retaddr;
  *(char *)(unaff_EBX + 0x416415) = *(char *)(unaff_EBX + 0x416415) + param_3;
  *(char *)(param_2 + -0x18) = *(char *)(param_2 + -0x18) + bVar8;
  out((short)param_2,unaff_retaddr);
  *unaff_retaddr = *unaff_retaddr + (byte)unaff_retaddr;
  *(undefined4 *)(param_4 + 0x10) = *(undefined4 *)(param_4 + 0x2c);
  *(undefined4 *)(param_4 + 0xc) = *(undefined4 *)(param_4 + 0x28);
  *(undefined4 *)(param_4 + 8) = *(undefined4 *)(param_4 + 0x24);
  *(undefined4 *)(param_4 + 4) = 0x36edb;
  FUN_0003ed18();
  DAT_00004178 = DAT_00004178 + -1;
  return;
}


// opcode @ 0x36ee0 (size=39)

/* WARNING: Control flow encountered bad instruction data */

void __regparm3 FUN_00036ee0(undefined4 param_1,undefined4 param_2,int param_3)

{
  char *pcVar1;
  int *piVar2;
  byte bVar3;
  byte bVar4;
  undefined3 uVar8;
  char *pcVar5;
  int iVar6;
  undefined *puVar7;
  int unaff_EBX;
  int iVar9;
  undefined4 unaff_EBP;
  undefined4 unaff_ESI;
  undefined4 unaff_EDI;
  undefined2 in_SS;
  byte in_CF;
  byte in_AF;
  
  DAT_00004178 = DAT_00004178 + 1;
  bVar3 = 9 < ((byte)param_1 & 0xf) | in_AF;
  uVar8 = (undefined3)((uint)param_1 >> 8);
  bVar4 = (byte)param_1 + bVar3 * '\x06';
  bVar4 = bVar4 + (0x90 < (bVar4 & 0xf0) | in_CF | bVar3 * (0xf9 < bVar4)) * '`';
  if (CONCAT31(uVar8,bVar4) != 0) {
    bVar3 = 9 < (bVar4 & 0xf) | bVar3;
    bVar4 = bVar4 + bVar3 * '\x06';
    pcVar5 = (char *)(CONCAT31(uVar8,bVar4 + (0x90 < (bVar4 & 0xf0) | bVar3 * (0xf9 < bVar4)) * '`')
                     + -0x7cf68c00);
    pcVar1 = (char *)(param_3 + -0x18 + unaff_EBX);
    *pcVar1 = *pcVar1 + (char)((uint)DAT_00004178 >> 8);
    piVar2 = (int *)segment(in_SS,(short)&stack0xffffffd8);
    iVar6 = *piVar2;
    iVar9 = CONCAT22((short)((uint)&stack0xffffffd8 >> 0x10),(short)&stack0xffffffd8 + 4);
    *pcVar5 = *pcVar5 + (char)pcVar5;
    *(char **)(iVar9 + -4) = pcVar5;
    *(int *)(iVar9 + -8) = param_3;
    *(int *)(iVar9 + -0xc) = iVar6;
    *(int *)(iVar9 + -0x10) = unaff_EBX;
    *(int *)(iVar9 + -0x14) = iVar9;
    *(undefined4 *)(iVar9 + -0x18) = unaff_EBP;
    *(undefined4 *)(iVar9 + -0x1c) = unaff_ESI;
    *(undefined4 *)(iVar9 + -0x20) = unaff_EDI;
                    /* WARNING: Could not recover jumptable at 0x00036f12. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    (**(code **)(iVar6 + 0x27))();
    return;
  }
  DAT_00000000 = DAT_00000000 + bVar4;
  if ((DAT_00004174 != 0) && ((DAT_00004178 == 1 || (iRam05276270 != 0)))) {
    iVar6 = FUN_0003c972();
    if (iVar6 == 0) {
      iVar6 = FUN_000353e4();
      if (iVar6 != 0) {
        puVar7 = &DAT_00052762;
        goto code_r0x00036f8e;
      }
    }
  }
  puVar7 = (undefined *)0x0;
code_r0x00036f8e:
  if (puVar7 != (undefined *)0x0) {
                    /* WARNING: Subroutine does not return */
    FUN_0003cbb3(DAT_00004164,0x891,&stack0xfffffff8);
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}


// opcode @ 0x36f08 (size=13)

void __regparm3 FUN_00036f08(char *param_1)

{
  int unaff_retaddr;
  
  *param_1 = *param_1 + (char)param_1;
                    /* WARNING: Could not recover jumptable at 0x00036f12. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(unaff_retaddr + 0x27))();
  return;
}


// opcode @ 0x36f24 (size=41)

void __regparm3 FUN_00036f24(undefined4 param_1,undefined4 param_2,int param_3)

{
  char cVar1;
  int unaff_EBX;
  byte in_CF;
  char *pcVar2;
  
  *(uint *)(&DAT_00052762 + unaff_EBX) =
       (*(int *)(&DAT_00052762 + unaff_EBX) - param_3) - (uint)in_CF;
  cVar1 = (char)param_1 + (char)((uint)param_3 >> 8);
  pcVar2 = (char *)CONCAT31((int3)((uint)param_1 >> 8),cVar1);
  if (-1 < cVar1) {
    *pcVar2 = *pcVar2 + cVar1;
    FUN_0003ee38();
    FUN_00035a12();
    return;
  }
                    /* WARNING: Subroutine does not return */
  FUN_0003cbb3(DAT_00004164,0x891,0x879);
}


// opcode @ 0x36f69 (size=24)

/* WARNING: Control flow encountered bad instruction data */

void __regparm3 FUN_00036f69(uint param_1)

{
  char *pcVar1;
  int iVar2;
  undefined *puVar3;
  int unaff_EBX;
  
  *(uint *)(unaff_EBX + 0x2762703d) = *(uint *)(unaff_EBX + 0x2762703d) | param_1;
  pcVar1 = (char *)(param_1 + 0xe8197400);
  *pcVar1 = *pcVar1 + (char)pcVar1;
  if (pcVar1 == (char *)0x0) {
    iVar2 = FUN_000353e4();
    if (iVar2 != 0) {
      puVar3 = &DAT_00052762;
      goto code_r0x00036f8e;
    }
  }
  puVar3 = (undefined *)0x0;
code_r0x00036f8e:
  if (puVar3 != (undefined *)0x0) {
                    /* WARNING: Subroutine does not return */
    FUN_0003cbb3(DAT_00004164,0x891);
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}


// opcode @ 0x36f82 (size=38)

void __regparm3 FUN_00036f82(int param_1)

{
  byte *pbVar1;
  char *pcVar2;
  int unaff_EBP;
  int unaff_EDI;
  
  pcVar2 = (char *)(unaff_EDI + -0x48 + param_1);
  *pcVar2 = *pcVar2 << 2;
  pbVar1 = (byte *)(unaff_EBP + -0x74e68b40);
  *pbVar1 = *pbVar1 << 4 | *pbVar1 >> 4;
                    /* WARNING: Subroutine does not return */
  FUN_0003cbb3(DAT_00004164,0x891);
}


// opcode @ 0x36fac (size=18)

/* WARNING: Control flow encountered bad instruction data */

void FUN_00036fac(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}


