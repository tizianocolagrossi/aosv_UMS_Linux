#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xff1efd75, "module_layout" },
	{ 0x343daa6c, "kmalloc_caches" },
	{ 0x91fb7914, "proc_symlink" },
	{ 0x754d539c, "strlen" },
	{ 0x3854774b, "kstrtoll" },
	{ 0x5367b4b4, "boot_cpu_data" },
	{ 0xb43f9365, "ktime_get" },
	{ 0x7c4aa8a2, "pv_ops" },
	{ 0xc3caedd1, "proc_remove" },
	{ 0xfb384d37, "kasprintf" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x387038fc, "misc_register" },
	{ 0xa9764fb7, "proc_mkdir" },
	{ 0x385522d4, "current_task" },
	{ 0xc5850110, "printk" },
	{ 0xf11543ff, "find_first_zero_bit" },
	{ 0xf8b93387, "module_put" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0x8427cc7b, "_raw_spin_lock_irq" },
	{ 0xb5d4b431, "wake_up_process" },
	{ 0xc6b10427, "ex_handler_fprestore" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x4130a5ec, "kmem_cache_alloc_trace" },
	{ 0x37a0cba, "kfree" },
	{ 0x656e4a6e, "snprintf" },
	{ 0xab219b29, "proc_create" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x41fe232a, "misc_deregister" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x2fd9f796, "try_module_get" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "B549097B76F8E02F58B2B54");
