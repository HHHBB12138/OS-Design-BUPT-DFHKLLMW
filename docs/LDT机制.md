# 研究作者使用LDT保护用户态程序的机制

## 1. 带LDT的结构体与字段作用

以下结构体与LDT直接相关, 代码见 [kernel/bootpack.h](kernel/bootpack.h#L255-L275):

```c
struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	int es, cs, ss, ds, fs, gs;
	int ldtr, iomap;
};

struct TASK {
	int sel, flags;
	int level, priority;
	struct FIFO32 fifo;
	struct TSS32 tss;
	struct SEGMENT_DESCRIPTOR ldt[2];
	struct CONSOLE *cons;
	int ds_base, cons_stack;
	struct FILEHANDLE *fhandle;
	int *fat;
	char *cmdline;
	unsigned char langmode, langbyte1;
};
```

字段作用(仅列与LDT实现相关的字段):

- `tss.ldtr`: 保存LDT选择子, 任务切换时CPU自动加载该LDT。
- `tss.ss0`/`tss.esp0`: 从用户态切回内核态时的内核栈段与栈顶。
- `ldt[0]`: 用户程序代码段描述符(基址=程序映像, 界限=代码长度, DPL=3)。
- `ldt[1]`: 用户程序数据段描述符(基址=数据区, 界限=数据段大小, DPL=3)。
- `ds_base`: 记录LDT[1]的基址, 供内核在系统调用中进行地址换算。
- `sel`: 该任务对应的TSS选择子, 用于切换任务时加载TSS。

---

## 2. 初始化时在GDT登记LDT

在任务初始化中, 每个任务的LDT会被登记到GDT, 并通过TSS的`ldtr`字段指向:

```c
taskctl->tasks0[i].tss.ldtr = (TASK_GDT0 + MAX_TASKS + i) * 8;
set_segmdesc(gdt + TASK_GDT0 + MAX_TASKS + i, 15, (int) taskctl->tasks0[i].ldt, AR_LDT);
```

含义:
- GDT中为每个任务分配一个LDT描述符
- `ldtr`用于任务切换时自动加载对应LDT

---

## 3. 用户程序加载时设置LDT段

当用户程序被加载时, 内核为代码段和数据段分别填写LDT描述符:

```c
task->ds_base = (int) q;
set_segmdesc(task->ldt + 0, appsiz - 1, (int) p, AR_CODE32_ER + 0x60);
set_segmdesc(task->ldt + 1, segsiz - 1, (int) q, AR_DATA32_RW + 0x60);
```

关键点:
- `AR_CODE32_ER + 0x60` 和 `AR_DATA32_RW + 0x60` 将DPL设为3
- 代码段和数据段的基址分别指向用户程序映像和数据区
- 段界限限制了用户程序的可访问范围

---

## 4. 切换到用户态时的段寄存器设置

进入用户程序前, 内核设置段寄存器为LDT选择子, 并切换特权级:

```assembly
MOV ES,BX
MOV DS,BX
MOV FS,BX
MOV GS,BX
OR  ECX,3
OR  EBX,3
PUSH EBX
PUSH EDX
PUSH ECX
PUSH EAX
RETF
```

这样用户程序运行时:
- CS/DS/SS等段寄存器指向LDT中的段描述符
- CPL=3, 处于用户态

---

## 5. 逻辑地址到物理地址的公式

用户程序访问变量时, 逻辑地址为 $DS:offset$。在本系统中, LDT[1]的数据段基址由 `task->ds_base` 保存:

```c
task->ds_base = (int) q;
```

因此线性地址为:

$$
	ext{linear} = \text{LDT[1].base} + \text{offset}
$$

系统未启用分页, 所以物理地址与线性地址相同:

$$
	ext{physical} = \text{LDT[1].base} + \text{offset}
$$

这一公式就是内核进行系统调用参数换算的依据, 例如将用户指针转换为内核可访问的地址。

---

## 6. 保护效果与访问限制

该机制带来的保护效果:

1. **内存隔离**
	- 用户程序只能访问LDT定义的段范围, 越界会触发保护异常

2. **内核空间不可直接访问**
	- 内核段在GDT中, DPL为0, 用户态无法直接访问

3. **跨任务隔离**
	- 每个任务的LDT基址不同, 用户程序之间互不覆盖

---

## 7. 结论

该系统通过“每任务独立LDT + 用户态DPL=3 + 段界限限制 + 任务切换时加载LDT”的组合, 实现了用户态程序的内存隔离与保护。这是基于x86分段机制的经典保护方案。
