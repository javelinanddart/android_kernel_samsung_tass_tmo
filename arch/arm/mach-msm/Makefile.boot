# MSM7x01A
   zreladdr-$(CONFIG_ARCH_MSM7X01A)	:= 0x10008000
params_phys-$(CONFIG_ARCH_MSM7X01A)	:= 0x10000100
initrd_phys-$(CONFIG_ARCH_MSM7X01A)	:= 0x10800000

# MSM7x25
   zreladdr-$(CONFIG_ARCH_MSM7X25)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X25)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X25)	:= 0x0A000000

# MSM7x27
ifeq ($(CONFIG_MACH_CALLISTO),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif

ifeq ($(CONFIG_MACH_COOPER),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_BENI),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_TASS),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_TASSATT),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_TASSTMO),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_GT2),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_FLIPBOOK),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_GIOATT),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_LUCAS),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x13608000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x13600100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x14600000
endif
ifeq ($(CONFIG_MACH_EUROPA),y)
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x0A000000
endif

# MSM7x30
   zreladdr-$(CONFIG_ARCH_MSM7X30)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X30)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X30)	:= 0x01200000

ifeq ($(CONFIG_MSM_SOC_REV_A),y)
# QSD8X50A
   zreladdr-$(CONFIG_ARCH_QSD8X50)	:= 0x00008000
params_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x00000100
initrd_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x04000000
else
# QSD8x50
   zreladdr-$(CONFIG_ARCH_QSD8X50)	:= 0x20008000
params_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x20000100
initrd_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x24000000
endif

# MSM8x60
   zreladdr-$(CONFIG_ARCH_MSM8X60)	:= 0x40208000
