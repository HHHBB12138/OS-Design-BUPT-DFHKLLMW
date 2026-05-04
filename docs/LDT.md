# ldtaddr 设计说明
## 一. 为什么要这样设计

目标是验证“LDT数据段下, 用户态指针是段内偏移, 物理地址=基址+偏移”。
因此需要两类信息:

1. **LDT数据段基址**
  - 通过 `api_getdsbase()` 获取 `ds_base`。
2. **内核视角的物理地址**
  - 通过 `api_virt2phys()` 把用户态指针换算为物理地址。

把用户态自行计算的 `ds_base + offset` 与内核返回值对比, 就能验证公式是否成立。

---

## 二. 这样能观察出什么

1. **逻辑地址是段内偏移**
  - 输出的 `logical DS:<offset>` 会随变量位置变化。

2. **物理地址与基址+偏移一致**
  - `physical` 与 `kernel`（内核换算结果）一致时, 说明公式成立。

3. **LDT数据段是用户态访问的共同基准**
  - 同一程序中 `ds_base` 固定, 说明用户态访问都以 LDT[1] 为基准。

---

## 三. 具体设计了什么

### 1) 输出格式

```
<name>
  logical  DS:<offset>
  ds_base  <base>
  physical <addr>
  kernel   <addr>
  check    <result>
```

含义:
- `logical`: 用户态指针值(段内偏移)
- `ds_base`: LDT[1]基址
- `physical`: 用户态计算的 `ds_base + offset`
- `kernel`: 内核用 `api_virt2phys()` 换算得到的物理地址
- `check`: 是否相等, 用于验证公式

### 2) 使用方法

编译:
```
cd apps/ldtaddr
make
```

运行:
```
ldtaddr
```

---

## 四. api078 与 api080 详细解释

### 1) api078: 读取 LDT 数据段基址

**用户态封装**([apps/apilib/api078.nas](apps/apilib/api078.nas)):
```assembly
_api_getdsbase:     ; int api_getdsbase(void);
   MOV     EDX,78
   INT     0x40
   RET
```

含义:
- `EDX=78` 选择系统调用号
- `INT 0x40` 进入内核的 `hrb_api`
- 返回值在 `EAX` 中

**内核处理**([kernel/console.c](kernel/console.c#L1086-L1091)):
```c
case 78:
   reg[7] = ds_base;
   break;
```

作用:
- 直接返回当前任务的 `ds_base`, 即 LDT[1] 的段基址

---

### 2) api080: 用户指针转物理地址

**用户态封装**([apps/apilib/api080.nas](apps/apilib/api080.nas)):
```assembly
_api_virt2phys:     ; unsigned int api_virt2phys(void *ptr);
   MOV     EDX,80
   MOV     EBX,[ESP+4]
   INT     0x40
   RET
```

含义:
- `EDX=80` 选择系统调用号
- `EBX` 携带用户态指针(段内偏移)
- `INT 0x40` 进入内核

**内核处理**([kernel/console.c](kernel/console.c#L1092-L1096)):
```c
case 80:
   reg[7] = (unsigned int) (ebx + ds_base);
   break;
```

作用:
- 将用户指针(偏移)与 `ds_base` 相加
- 返回“内核视角的物理地址”

---

## 五. 公式总结

本系统未启用分页, 因此:

$$
	ext{physical} = \text{LDT[1].base} + \text{offset}
$$

其中:
- `LDT[1].base` 即 `ds_base`
- `offset` 即用户态指针值
